/*
 * Copyright (c) 2007 Cisco Systems, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <rdma/ib_mad.h>
#include <rdma/ib_smi.h>
#include <rdma/ib_sa.h>
#include <rdma/ib_cache.h>

#include <linux/random.h>
#include <dev/mlx4/cmd.h>
#include <linux/gfp.h>
#include <rdma/ib_pma.h>

#include "mlx4_ib.h"

enum {
	MLX4_IB_VENDOR_CLASS1 = 0x9,
	MLX4_IB_VENDOR_CLASS2 = 0xa
};

#define MLX4_TUN_SEND_WRID_SHIFT 34
#define MLX4_TUN_QPN_SHIFT 32
#define MLX4_TUN_WRID_RECV (((u64) 1) << MLX4_TUN_SEND_WRID_SHIFT)
#define MLX4_TUN_SET_WRID_QPN(a) (((u64) ((a) & 0x3)) << MLX4_TUN_QPN_SHIFT)

#define MLX4_TUN_IS_RECV(a)  (((a) >>  MLX4_TUN_SEND_WRID_SHIFT) & 0x1)
#define MLX4_TUN_WRID_QPN(a) (((a) >> MLX4_TUN_QPN_SHIFT) & 0x3)

 /* Port mgmt change event handling */

#define GET_BLK_PTR_FROM_EQE(eqe) be32_to_cpu(eqe->event.port_mgmt_change.params.tbl_change_info.block_ptr)
#define GET_MASK_FROM_EQE(eqe) be32_to_cpu(eqe->event.port_mgmt_change.params.tbl_change_info.tbl_entries_mask)
#define NUM_IDX_IN_PKEY_TBL_BLK 32
#define GUID_TBL_ENTRY_SIZE 8	   /* size in bytes */
#define GUID_TBL_BLK_NUM_ENTRIES 8
#define GUID_TBL_BLK_SIZE (GUID_TBL_ENTRY_SIZE * GUID_TBL_BLK_NUM_ENTRIES)

struct mlx4_mad_rcv_buf {
	struct ib_grh grh;
	u8 payload[256];
} __packed;

struct mlx4_mad_snd_buf {
	u8 payload[256];
} __packed;

struct mlx4_tunnel_mad {
	struct ib_grh grh;
	struct mlx4_ib_tunnel_header hdr;
	struct ib_mad mad;
} __packed;

struct mlx4_rcv_tunnel_mad {
	struct mlx4_rcv_tunnel_hdr hdr;
	struct ib_grh grh;
	struct ib_mad mad;
} __packed;

static void handle_client_rereg_event(struct mlx4_ib_dev *dev, u8 port_num);
static void handle_lid_change_event(struct mlx4_ib_dev *dev, u8 port_num);
static void __propagate_pkey_ev(struct mlx4_ib_dev *dev, int port_num,
				int block, u32 change_bitmap);

__be64 mlx4_ib_gen_node_guid(void)
{
#define NODE_GUID_HI	((u64) (((u64)IB_OPENIB_OUI) << 40))
	return cpu_to_be64(NODE_GUID_HI | random());
}

__be64 mlx4_ib_get_new_demux_tid(struct mlx4_ib_demux_ctx *ctx)
{
	return cpu_to_be64(atomic_inc_return(&ctx->tid)) |
		cpu_to_be64(0xff00000000000000LL);
}

int mlx4_MAD_IFC(struct mlx4_ib_dev *dev, int mad_ifc_flags,
		 int port, struct ib_wc *in_wc, struct ib_grh *in_grh,
		 void *in_mad, void *response_mad)
{
	struct mlx4_cmd_mailbox *inmailbox, *outmailbox;
	void *inbox;
	int err;
	u32 in_modifier = port;
	u8 op_modifier = 0;

	inmailbox = mlx4_alloc_cmd_mailbox(dev->dev);
	if (IS_ERR(inmailbox))
		return PTR_ERR(inmailbox);
	inbox = inmailbox->buf;

	outmailbox = mlx4_alloc_cmd_mailbox(dev->dev);
	if (IS_ERR(outmailbox)) {
		mlx4_free_cmd_mailbox(dev->dev, inmailbox);
		return PTR_ERR(outmailbox);
	}

	memcpy(inbox, in_mad, 256);

	/*
	 * Key check traps can't be generated unless we have in_wc to
	 * tell us where to send the trap.
	 */
	if ((mad_ifc_flags & MLX4_MAD_IFC_IGNORE_MKEY) || !in_wc)
		op_modifier |= 0x1;
	if ((mad_ifc_flags & MLX4_MAD_IFC_IGNORE_BKEY) || !in_wc)
		op_modifier |= 0x2;
	if (mlx4_is_mfunc(dev->dev) &&
	    (mad_ifc_flags & MLX4_MAD_IFC_NET_VIEW || in_wc))
		op_modifier |= 0x8;

	if (in_wc) {
		struct {
			__be32		my_qpn;
			u32		reserved1;
			__be32		rqpn;
			u8		sl;
			u8		g_path;
			u16		reserved2[2];
			__be16		pkey;
			u32		reserved3[11];
			u8		grh[40];
		} *ext_info;

		memset(inbox + 256, 0, 256);
		ext_info = inbox + 256;

		ext_info->my_qpn = cpu_to_be32(in_wc->qp->qp_num);
		ext_info->rqpn   = cpu_to_be32(in_wc->src_qp);
		ext_info->sl     = in_wc->sl << 4;
		ext_info->g_path = in_wc->dlid_path_bits |
			(in_wc->wc_flags & IB_WC_GRH ? 0x80 : 0);
		ext_info->pkey   = cpu_to_be16(in_wc->pkey_index);

		if (in_grh)
			memcpy(ext_info->grh, in_grh, 40);

		op_modifier |= 0x4;

		in_modifier |= in_wc->slid << 16;
	}

	err = mlx4_cmd_box(dev->dev, inmailbox->dma, outmailbox->dma, in_modifier,
			   mlx4_is_master(dev->dev) ? (op_modifier & ~0x8) : op_modifier,
			   MLX4_CMD_MAD_IFC, MLX4_CMD_TIME_CLASS_C,
			   (op_modifier & 0x8) ? MLX4_CMD_NATIVE : MLX4_CMD_WRAPPED);

	if (!err)
		memcpy(response_mad, outmailbox->buf, 256);

	mlx4_free_cmd_mailbox(dev->dev, inmailbox);
	mlx4_free_cmd_mailbox(dev->dev, outmailbox);

	return err;
}

static void update_sm_ah(struct mlx4_ib_dev *dev, u8 port_num, u16 lid, u8 sl)
{
	struct ib_ah *new_ah;
	struct ib_ah_attr ah_attr;
	unsigned long flags;

	if (!dev->send_agent[port_num - 1][0])
		return;

	memset(&ah_attr, 0, sizeof ah_attr);
	ah_attr.dlid     = lid;
	ah_attr.sl       = sl;
	ah_attr.port_num = port_num;

	new_ah = ib_create_ah(dev->send_agent[port_num - 1][0]->qp->pd,
			      &ah_attr);
	if (IS_ERR(new_ah))
		return;

	spin_lock_irqsave(&dev->sm_lock, flags);
	if (dev->sm_ah[port_num - 1])
		ib_destroy_ah(dev->sm_ah[port_num - 1]);
	dev->sm_ah[port_num - 1] = new_ah;
	spin_unlock_irqrestore(&dev->sm_lock, flags);
}

/*
 * Snoop SM MADs for port info, GUID info, and  P_Key table sets, so we can
 * synthesize LID change, Client-Rereg, GID change, and P_Key change events.
 */
static void smp_snoop(struct ib_device *ibdev, u8 port_num, struct ib_mad *mad,
		      u16 prev_lid)
{
	struct ib_port_info *pinfo;
	u16 lid;
	__be16 *base;
	u32 bn, pkey_change_bitmap;
	int i;


	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	if ((mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_LID_ROUTED ||
	     mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE) &&
	    mad->mad_hdr.method == IB_MGMT_METHOD_SET)
		switch (mad->mad_hdr.attr_id) {
		case IB_SMP_ATTR_PORT_INFO:
			pinfo = (struct ib_port_info *) ((struct ib_smp *) mad)->data;
			lid = be16_to_cpu(pinfo->lid);

			update_sm_ah(dev, port_num,
				     be16_to_cpu(pinfo->sm_lid),
				     pinfo->neighbormtu_mastersmsl & 0xf);

			if (pinfo->clientrereg_resv_subnetto & 0x80)
				handle_client_rereg_event(dev, port_num);

			if (prev_lid != lid)
				handle_lid_change_event(dev, port_num);
			break;

		case IB_SMP_ATTR_PKEY_TABLE:
			if (!mlx4_is_mfunc(dev->dev)) {
				mlx4_ib_dispatch_event(dev, port_num,
						       IB_EVENT_PKEY_CHANGE);
				break;
			}

			/* at this point, we are running in the master.
			 * Slaves do not receive SMPs.
			 */
			bn  = be32_to_cpu(((struct ib_smp *)mad)->attr_mod) & 0xFFFF;
			base = (__be16 *) &(((struct ib_smp *)mad)->data[0]);
			pkey_change_bitmap = 0;
			for (i = 0; i < 32; i++) {
				pr_debug("PKEY[%d] = x%x\n",
					 i + bn*32, be16_to_cpu(base[i]));
				if (be16_to_cpu(base[i]) !=
				    dev->pkeys.phys_pkey_cache[port_num - 1][i + bn*32]) {
					pkey_change_bitmap |= (1 << i);
					dev->pkeys.phys_pkey_cache[port_num - 1][i + bn*32] =
						be16_to_cpu(base[i]);
				}
			}
			pr_debug("PKEY Change event: port=%d, "
				 "block=0x%x, change_bitmap=0x%x\n",
				 port_num, bn, pkey_change_bitmap);

			if (pkey_change_bitmap) {
				mlx4_ib_dispatch_event(dev, port_num,
						       IB_EVENT_PKEY_CHANGE);
				if (!dev->sriov.is_going_down)
					__propagate_pkey_ev(dev, port_num, bn,
							    pkey_change_bitmap);
			}
			break;

		case IB_SMP_ATTR_GUID_INFO:
			/* paravirtualized master's guid is guid 0 -- does not change */
			if (!mlx4_is_master(dev->dev))
				mlx4_ib_dispatch_event(dev, port_num,
						       IB_EVENT_GID_CHANGE);
			/*if master, notify relevant slaves*/
			if (mlx4_is_master(dev->dev) &&
			    !dev->sriov.is_going_down) {
				bn = be32_to_cpu(((struct ib_smp *)mad)->attr_mod);
				mlx4_ib_update_cache_on_guid_change(dev, bn, port_num,
								    (u8 *)(&((struct ib_smp *)mad)->data));
				mlx4_ib_notify_slaves_on_guid_change(dev, bn, port_num,
								     (u8 *)(&((struct ib_smp *)mad)->data));
			}
			break;

		default:
			break;
		}
}

static void __propagate_pkey_ev(struct mlx4_ib_dev *dev, int port_num,
				int block, u32 change_bitmap)
{
	int i, ix, slave, err;
	int have_event = 0;

	for (slave = 0; slave < dev->dev->caps.sqp_demux; slave++) {
		if (slave == mlx4_master_func_num(dev->dev))
			continue;
		if (!mlx4_is_slave_active(dev->dev, slave))
			continue;

		have_event = 0;
		for (i = 0; i < 32; i++) {
			if (!(change_bitmap & (1 << i)))
				continue;
			for (ix = 0;
			     ix < dev->dev->caps.pkey_table_len[port_num]; ix++) {
				if (dev->pkeys.virt2phys_pkey[slave][port_num - 1]
				    [ix] == i + 32 * block) {
					err = mlx4_gen_pkey_eqe(dev->dev, slave, port_num);
					pr_debug("propagate_pkey_ev: slave %d,"
						 " port %d, ix %d (%d)\n",
						 slave, port_num, ix, err);
					have_event = 1;
					break;
				}
			}
			if (have_event)
				break;
		}
	}
}

static void node_desc_override(struct ib_device *dev,
			       struct ib_mad *mad)
{
	unsigned long flags;

	if ((mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_LID_ROUTED ||
	     mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE) &&
	    mad->mad_hdr.method == IB_MGMT_METHOD_GET_RESP &&
	    mad->mad_hdr.attr_id == IB_SMP_ATTR_NODE_DESC) {
		spin_lock_irqsave(&to_mdev(dev)->sm_lock, flags);
		memcpy(((struct ib_smp *) mad)->data, dev->node_desc, 64);
		spin_unlock_irqrestore(&to_mdev(dev)->sm_lock, flags);
	}
}

static void forward_trap(struct mlx4_ib_dev *dev, u8 port_num, struct ib_mad *mad)
{
	int qpn = mad->mad_hdr.mgmt_class != IB_MGMT_CLASS_SUBN_LID_ROUTED;
	struct ib_mad_send_buf *send_buf;
	struct ib_mad_agent *agent = dev->send_agent[port_num - 1][qpn];
	int ret;
	unsigned long flags;

	if (agent) {
		send_buf = ib_create_send_mad(agent, qpn, 0, 0, IB_MGMT_MAD_HDR,
					      IB_MGMT_MAD_DATA, GFP_ATOMIC);
		if (IS_ERR(send_buf))
			return;
		/*
		 * We rely here on the fact that MLX QPs don't use the
		 * address handle after the send is posted (this is
		 * wrong following the IB spec strictly, but we know
		 * it's OK for our devices).
		 */
		spin_lock_irqsave(&dev->sm_lock, flags);
		memcpy(send_buf->mad, mad, sizeof *mad);
		if ((send_buf->ah = dev->sm_ah[port_num - 1]))
			ret = ib_post_send_mad(send_buf, NULL);
		else
			ret = -EINVAL;
		spin_unlock_irqrestore(&dev->sm_lock, flags);

		if (ret)
			ib_free_send_mad(send_buf);
	}
}

static int mlx4_ib_demux_sa_handler(struct ib_device *ibdev, int port, int slave,
							     struct ib_sa_mad *sa_mad)
{
	int ret = 0;

	/* dispatch to different sa handlers */
	switch (be16_to_cpu(sa_mad->mad_hdr.attr_id)) {
	case IB_SA_ATTR_MC_MEMBER_REC:
		ret = mlx4_ib_mcg_demux_handler(ibdev, port, slave, sa_mad);
		break;
	default:
		break;
	}
	return ret;
}

int mlx4_ib_find_real_gid(struct ib_device *ibdev, u8 port, __be64 guid)
{
	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	int i;

	for (i = 0; i < dev->dev->caps.sqp_demux; i++) {
		if (dev->sriov.demux[port - 1].guid_cache[i] == guid)
			return i;
	}
	return -1;
}


static int find_slave_port_pkey_ix(struct mlx4_ib_dev *dev, int slave,
				   u8 port, u16 pkey, u16 *ix)
{
	int i, ret;
	u8 unassigned_pkey_ix, pkey_ix, partial_ix = 0xFF;
	u16 slot_pkey;

	if (slave == mlx4_master_func_num(dev->dev))
		return ib_find_cached_pkey(&dev->ib_dev, port, pkey, ix);

	unassigned_pkey_ix = dev->dev->phys_caps.pkey_phys_table_len[port] - 1;

	for (i = 0; i < dev->dev->caps.pkey_table_len[port]; i++) {
		if (dev->pkeys.virt2phys_pkey[slave][port - 1][i] == unassigned_pkey_ix)
			continue;

		pkey_ix = dev->pkeys.virt2phys_pkey[slave][port - 1][i];

		ret = ib_get_cached_pkey(&dev->ib_dev, port, pkey_ix, &slot_pkey);
		if (ret)
			continue;
		if ((slot_pkey & 0x7FFF) == (pkey & 0x7FFF)) {
			if (slot_pkey & 0x8000) {
				*ix = (u16) pkey_ix;
				return 0;
			} else {
				/* take first partial pkey index found */
				if (partial_ix == 0xFF)
					partial_ix = pkey_ix;
			}
		}
	}

	if (partial_ix < 0xFF) {
		*ix = (u16) partial_ix;
		return 0;
	}

	return -EINVAL;
}

int mlx4_ib_send_to_slave(struct mlx4_ib_dev *dev, int slave, u8 port,
			  enum ib_qp_type dest_qpt, struct ib_wc *wc,
			  struct ib_grh *grh, struct ib_mad *mad)
{
	struct ib_sge list;
	struct ib_send_wr wr, *bad_wr;
	struct mlx4_ib_demux_pv_ctx *tun_ctx;
	struct mlx4_ib_demux_pv_qp *tun_qp;
	struct mlx4_rcv_tunnel_mad *tun_mad;
	struct ib_ah_attr attr;
	struct ib_ah *ah;
	struct ib_qp *src_qp = NULL;
	unsigned tun_tx_ix = 0;
	int dqpn;
	int ret = 0;
	u16 tun_pkey_ix;
	u16 cached_pkey;
	u8 is_eth = dev->dev->caps.port_type[port] == MLX4_PORT_TYPE_ETH;

	if (dest_qpt > IB_QPT_GSI)
		return -EINVAL;

	tun_ctx = dev->sriov.demux[port-1].tun[slave];

	/* check if proxy qp created */
	if (!tun_ctx || tun_ctx->state != DEMUX_PV_STATE_ACTIVE)
		return -EAGAIN;

	/* QP0 forwarding only for Dom0 */
	if (!dest_qpt && (mlx4_master_func_num(dev->dev) != slave))
		return -EINVAL;

	if (!dest_qpt)
		tun_qp = &tun_ctx->qp[0];
	else
		tun_qp = &tun_ctx->qp[1];

	/* compute P_Key index to put in tunnel header for slave */
	if (dest_qpt) {
		u16 pkey_ix;
		ret = ib_get_cached_pkey(&dev->ib_dev, port, wc->pkey_index, &cached_pkey);
		if (ret)
			return -EINVAL;

		ret = find_slave_port_pkey_ix(dev, slave, port, cached_pkey, &pkey_ix);
		if (ret)
			return -EINVAL;
		tun_pkey_ix = pkey_ix;
	} else
		tun_pkey_ix = dev->pkeys.virt2phys_pkey[slave][port - 1][0];

	dqpn = dev->dev->phys_caps.base_proxy_sqpn + 8 * slave + port + (dest_qpt * 2) - 1;

	/* get tunnel tx data buf for slave */
	src_qp = tun_qp->qp;

	/* create ah. Just need an empty one with the port num for the post send.
	 * The driver will set the force loopback bit in post_send */
	memset(&attr, 0, sizeof attr);
	attr.port_num = port;
	if (is_eth) {
		memcpy(&attr.grh.dgid.raw[0], &grh->dgid.raw[0], 16);
		attr.ah_flags = IB_AH_GRH;
	}
	ah = ib_create_ah(tun_ctx->pd, &attr);
	if (IS_ERR(ah))
		return -ENOMEM;

	/* allocate tunnel tx buf after pass failure returns */
	spin_lock(&tun_qp->tx_lock);
	if (tun_qp->tx_ix_head - tun_qp->tx_ix_tail >=
	    (MLX4_NUM_TUNNEL_BUFS - 1))
		ret = -EAGAIN;
	else
		tun_tx_ix = (++tun_qp->tx_ix_head) & (MLX4_NUM_TUNNEL_BUFS - 1);
	spin_unlock(&tun_qp->tx_lock);
	if (ret)
		goto out;

	tun_mad = (struct mlx4_rcv_tunnel_mad *) (tun_qp->tx_ring[tun_tx_ix].buf.addr);
	if (tun_qp->tx_ring[tun_tx_ix].ah)
		ib_destroy_ah(tun_qp->tx_ring[tun_tx_ix].ah);
	tun_qp->tx_ring[tun_tx_ix].ah = ah;
	ib_dma_sync_single_for_cpu(&dev->ib_dev,
				   tun_qp->tx_ring[tun_tx_ix].buf.map,
				   sizeof (struct mlx4_rcv_tunnel_mad),
				   DMA_TO_DEVICE);

	/* copy over to tunnel buffer */
	if (grh)
		memcpy(&tun_mad->grh, grh, sizeof *grh);
	memcpy(&tun_mad->mad, mad, sizeof *mad);

	/* adjust tunnel data */
	tun_mad->hdr.pkey_index = cpu_to_be16(tun_pkey_ix);
	tun_mad->hdr.flags_src_qp = cpu_to_be32(wc->src_qp & 0xFFFFFF);
	tun_mad->hdr.g_ml_path = (grh && (wc->wc_flags & IB_WC_GRH)) ? 0x80 : 0;

	if (is_eth) {
		u16 vlan = 0;
		if (mlx4_get_slave_default_vlan(dev->dev, port, slave, &vlan,
		    NULL)) {
			if (vlan != wc->vlan_id)
				/* VST and default vlan is not the packet vlan drop the
				 * packet*/
				goto out;
			 else
				/* VST , remove hide the vlan from the VF */
				vlan = 0;
		} else {
			vlan = wc->vlan_id;
		}

		tun_mad->hdr.sl_vid = cpu_to_be16(vlan);
		memcpy((char *)&tun_mad->hdr.mac_31_0, &(wc->smac[0]), 4);
		memcpy((char *)&tun_mad->hdr.slid_mac_47_32, &(wc->smac[4]), 2);
	} else {
		tun_mad->hdr.sl_vid = cpu_to_be16(((u16)(wc->sl)) << 12);
		tun_mad->hdr.slid_mac_47_32 = cpu_to_be16(wc->slid);
	}

	ib_dma_sync_single_for_device(&dev->ib_dev,
				      tun_qp->tx_ring[tun_tx_ix].buf.map,
				      sizeof (struct mlx4_rcv_tunnel_mad),
				      DMA_TO_DEVICE);

	list.addr = tun_qp->tx_ring[tun_tx_ix].buf.map;
	list.length = sizeof (struct mlx4_rcv_tunnel_mad);
	list.lkey = tun_ctx->mr->lkey;

	wr.wr.ud.ah = ah;
	wr.wr.ud.port_num = port;
	wr.wr.ud.remote_qkey = IB_QP_SET_QKEY;
	wr.wr.ud.remote_qpn = dqpn;
	wr.next = NULL;
	wr.wr_id = ((u64) tun_tx_ix) | MLX4_TUN_SET_WRID_QPN(dest_qpt);
	wr.sg_list = &list;
	wr.num_sge = 1;
	wr.opcode = IB_WR_SEND;
	wr.send_flags = IB_SEND_SIGNALED;

	ret = ib_post_send(src_qp, &wr, &bad_wr);
out:
	if (ret)
		ib_destroy_ah(ah);
	return ret;
}

static int mlx4_ib_demux_mad(struct ib_device *ibdev, u8 port,
			struct ib_wc *wc, struct ib_grh *grh,
			struct ib_mad *mad)
{
	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	int err;
	int slave;
	u8 *slave_id;
	int is_eth = 0;

	if (rdma_port_get_link_layer(ibdev, port) == IB_LINK_LAYER_INFINIBAND)
		is_eth = 0;
	else
		is_eth = 1;

	if (is_eth) {
		if (!(wc->wc_flags & IB_WC_GRH)) {
			mlx4_ib_warn(ibdev, "RoCE grh not present.\n");
			return -EINVAL;
		}
		if (mad->mad_hdr.mgmt_class != IB_MGMT_CLASS_CM) {
			mlx4_ib_warn(ibdev, "RoCE mgmt class is not CM\n");
			return -EINVAL;
		}
		if (mlx4_get_slave_from_roce_gid(dev->dev, port, grh->dgid.raw, &slave)) {
			mlx4_ib_warn(ibdev, "failed matching grh\n");
			return -ENOENT;
		}
		if (slave >= dev->dev->caps.sqp_demux) {
			mlx4_ib_warn(ibdev, "slave id: %d is bigger than allowed:%d\n",
				     slave, dev->dev->caps.sqp_demux);
			return -ENOENT;
		}

		if (mlx4_ib_demux_cm_handler(ibdev, port, &slave, mad, is_eth))
			return 0;

		err = mlx4_ib_send_to_slave(dev, slave, port, wc->qp->qp_type, wc, grh, mad);
		if (err)
			pr_debug("failed sending to slave %d via tunnel qp (%d)\n",
				 slave, err);
		return 0;
	}

	/* Initially assume that this mad is for us */
	slave = mlx4_master_func_num(dev->dev);

	/* See if the slave id is encoded in a response mad */
	if (mad->mad_hdr.method & 0x80) {
		slave_id = (u8 *) &mad->mad_hdr.tid;
		slave = *slave_id;
		if (slave != 255) /*255 indicates the dom0*/
			*slave_id = 0; /* remap tid */
	}

	/* If a grh is present, we demux according to it */
	if (wc->wc_flags & IB_WC_GRH) {
		slave = mlx4_ib_find_real_gid(ibdev, port, grh->dgid.global.interface_id);
		if (slave < 0) {
			mlx4_ib_warn(ibdev, "failed matching grh\n");
			return -ENOENT;
		}
	}
	/* Class-specific handling */
	switch (mad->mad_hdr.mgmt_class) {
	case IB_MGMT_CLASS_SUBN_ADM:
		if (mlx4_ib_demux_sa_handler(ibdev, port, slave,
					     (struct ib_sa_mad *) mad))
			return 0;
		break;
	case IB_MGMT_CLASS_CM:
		if (mlx4_ib_demux_cm_handler(ibdev, port, &slave, mad, is_eth))
			return 0;
		break;
	case IB_MGMT_CLASS_DEVICE_MGMT:
		if (mad->mad_hdr.method != IB_MGMT_METHOD_GET_RESP)
			return 0;
		break;
	default:
		/* Drop unsupported classes for slaves in tunnel mode */
		if (slave != mlx4_master_func_num(dev->dev)) {
			pr_debug("dropping unsupported ingress mad from class:%d "
				 "for slave:%d\n", mad->mad_hdr.mgmt_class, slave);
			return 0;
		}
	}
	/*make sure that no slave==255 was not handled yet.*/
	if (slave >= dev->dev->caps.sqp_demux) {
		mlx4_ib_warn(ibdev, "slave id: %d is bigger than allowed:%d\n",
			     slave, dev->dev->caps.sqp_demux);
		return -ENOENT;
	}

	err = mlx4_ib_send_to_slave(dev, slave, port, wc->qp->qp_type, wc, grh, mad);
	if (err)
		pr_debug("failed sending to slave %d via tunnel qp (%d)\n",
			 slave, err);
	return 0;
}

static int ib_process_mad(struct ib_device *ibdev, int mad_flags, u8 port_num,
			struct ib_wc *in_wc, struct ib_grh *in_grh,
			struct ib_mad *in_mad, struct ib_mad *out_mad)
{
	u16 slid, prev_lid = 0;
	int err;
	struct ib_port_attr pattr;

	if (in_wc && in_wc->qp->qp_num) {
		pr_debug("received MAD: slid:%d sqpn:%d "
			"dlid_bits:%d dqpn:%d wc_flags:0x%x, cls %x, mtd %x, atr %x\n",
			in_wc->slid, in_wc->src_qp,
			in_wc->dlid_path_bits,
			in_wc->qp->qp_num,
			in_wc->wc_flags,
			in_mad->mad_hdr.mgmt_class, in_mad->mad_hdr.method,
			be16_to_cpu(in_mad->mad_hdr.attr_id));
		if (in_wc->wc_flags & IB_WC_GRH) {
			pr_debug("sgid_hi:0x%016llx sgid_lo:0x%016llx\n",
				 (unsigned long long)be64_to_cpu(in_grh->sgid.global.subnet_prefix),
				 (unsigned long long)be64_to_cpu(in_grh->sgid.global.interface_id));
			pr_debug("dgid_hi:0x%016llx dgid_lo:0x%016llx\n",
				 (unsigned long long)be64_to_cpu(in_grh->dgid.global.subnet_prefix),
				 (unsigned long long)be64_to_cpu(in_grh->dgid.global.interface_id));
		}
	}

	slid = in_wc ? in_wc->slid : be16_to_cpu(IB_LID_PERMISSIVE);

	if (in_mad->mad_hdr.method == IB_MGMT_METHOD_TRAP && slid == 0) {
		forward_trap(to_mdev(ibdev), port_num, in_mad);
		return IB_MAD_RESULT_SUCCESS | IB_MAD_RESULT_CONSUMED;
	}

	if (in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_LID_ROUTED ||
	    in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE) {
		if (in_mad->mad_hdr.method   != IB_MGMT_METHOD_GET &&
		    in_mad->mad_hdr.method   != IB_MGMT_METHOD_SET &&
		    in_mad->mad_hdr.method   != IB_MGMT_METHOD_TRAP_REPRESS)
			return IB_MAD_RESULT_SUCCESS;

		/*
		 * Don't process SMInfo queries -- the SMA can't handle them.
		 */
		if (in_mad->mad_hdr.attr_id == IB_SMP_ATTR_SM_INFO)
			return IB_MAD_RESULT_SUCCESS;
	} else if (in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_PERF_MGMT ||
		   in_mad->mad_hdr.mgmt_class == MLX4_IB_VENDOR_CLASS1   ||
		   in_mad->mad_hdr.mgmt_class == MLX4_IB_VENDOR_CLASS2   ||
		   in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_CONG_MGMT) {
		if (in_mad->mad_hdr.method  != IB_MGMT_METHOD_GET &&
		    in_mad->mad_hdr.method  != IB_MGMT_METHOD_SET)
			return IB_MAD_RESULT_SUCCESS;
	} else
		return IB_MAD_RESULT_SUCCESS;

	if ((in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_LID_ROUTED ||
	     in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE) &&
	    in_mad->mad_hdr.method == IB_MGMT_METHOD_SET &&
	    in_mad->mad_hdr.attr_id == IB_SMP_ATTR_PORT_INFO &&
	    !ib_query_port(ibdev, port_num, &pattr))
		prev_lid = pattr.lid;

	err = mlx4_MAD_IFC(to_mdev(ibdev),
			   (mad_flags & IB_MAD_IGNORE_MKEY ? MLX4_MAD_IFC_IGNORE_MKEY : 0) |
			   (mad_flags & IB_MAD_IGNORE_BKEY ? MLX4_MAD_IFC_IGNORE_BKEY : 0) |
			   MLX4_MAD_IFC_NET_VIEW,
			   port_num, in_wc, in_grh, in_mad, out_mad);
	if (err)
		return IB_MAD_RESULT_FAILURE;

	if (!out_mad->mad_hdr.status) {
		if (!(to_mdev(ibdev)->dev->caps.flags & MLX4_DEV_CAP_FLAG_PORT_MNG_CHG_EV))
			smp_snoop(ibdev, port_num, in_mad, prev_lid);
		/* slaves get node desc from FW */
		if (!mlx4_is_slave(to_mdev(ibdev)->dev))
			node_desc_override(ibdev, out_mad);
	}

	/* set return bit in status of directed route responses */
	if (in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE)
		out_mad->mad_hdr.status |= cpu_to_be16(1 << 15);

	if (in_mad->mad_hdr.method == IB_MGMT_METHOD_TRAP_REPRESS)
		/* no response for trap repress */
		return IB_MAD_RESULT_SUCCESS | IB_MAD_RESULT_CONSUMED;

	return IB_MAD_RESULT_SUCCESS | IB_MAD_RESULT_REPLY;
}

static void edit_counter_ext(struct mlx4_if_stat_extended *cnt, void *counters,
			     __be16 attr_id)
{
	switch (attr_id) {
	case IB_PMA_PORT_COUNTERS:
	{
		struct ib_pma_portcounters *pma_cnt =
				(struct ib_pma_portcounters *)counters;
		pma_cnt->port_xmit_data =
			cpu_to_be32((be64_to_cpu(cnt->counters[0].
						 IfTxUnicastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfTxMulticastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfTxBroadcastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfTxDroppedOctets)) >> 2);
		pma_cnt->port_rcv_data  =
			cpu_to_be32((be64_to_cpu(cnt->counters[0].
						 IfRxUnicastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfRxMulticastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfRxBroadcastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfRxNoBufferOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfRxErrorOctets)) >> 2);
		pma_cnt->port_xmit_packets =
			cpu_to_be32(be64_to_cpu(cnt->counters[0].
						IfTxUnicastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfTxMulticastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfTxBroadcastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfTxDroppedFrames));
		pma_cnt->port_rcv_packets  =
			cpu_to_be32(be64_to_cpu(cnt->counters[0].
						IfRxUnicastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfRxMulticastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfRxBroadcastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfRxNoBufferFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfRxErrorFrames));
		pma_cnt->port_rcv_errors = cpu_to_be32(be64_to_cpu(cnt->
						       counters[0].
						       IfRxErrorFrames));
		break;
	}

	case IB_PMA_PORT_COUNTERS_EXT:
	{
		struct ib_pma_portcounters_ext *pma_cnt_ext =
				(struct ib_pma_portcounters_ext *)counters;

		pma_cnt_ext->port_xmit_data =
			cpu_to_be64((be64_to_cpu(cnt->counters[0].
						 IfTxUnicastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfTxMulticastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfTxBroadcastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfTxDroppedOctets)) >> 2);
		pma_cnt_ext->port_rcv_data  =
			cpu_to_be64((be64_to_cpu(cnt->counters[0].
						 IfRxUnicastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfRxMulticastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfRxBroadcastOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfRxNoBufferOctets) +
				     be64_to_cpu(cnt->counters[0].
						 IfRxErrorOctets)) >> 2);
		pma_cnt_ext->port_xmit_packets =
			cpu_to_be64(be64_to_cpu(cnt->counters[0].
						IfTxUnicastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfTxMulticastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfTxBroadcastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfTxDroppedFrames));
		pma_cnt_ext->port_rcv_packets  =
			cpu_to_be64(be64_to_cpu(cnt->counters[0].
						IfRxUnicastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfRxMulticastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfRxBroadcastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfRxNoBufferFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfRxErrorFrames));
		pma_cnt_ext->port_unicast_xmit_packets = cnt->counters[0].
						IfTxUnicastFrames;
		pma_cnt_ext->port_unicast_rcv_packets = cnt->counters[0].
						IfRxUnicastFrames;
		pma_cnt_ext->port_multicast_xmit_packets =
			cpu_to_be64(be64_to_cpu(cnt->counters[0].
						IfTxMulticastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfTxBroadcastFrames));
		pma_cnt_ext->port_multicast_rcv_packets =
			cpu_to_be64(be64_to_cpu(cnt->counters[0].
						IfTxMulticastFrames) +
				    be64_to_cpu(cnt->counters[0].
						IfTxBroadcastFrames));

		break;
	}

	default:
		pr_warn("Unsupported attr_id 0x%x\n", attr_id);
		break;
	}

}

static void edit_counter(struct mlx4_if_stat_basic *cnt, void *counters,
			 __be16	attr_id)
{
	switch (attr_id) {
	case IB_PMA_PORT_COUNTERS:
	{
		struct ib_pma_portcounters *pma_cnt =
				(struct ib_pma_portcounters *) counters;
		pma_cnt->port_xmit_data =
			cpu_to_be32(be64_to_cpu(
				    cnt->counters[0].IfTxOctets) >> 2);
		pma_cnt->port_rcv_data  =
			cpu_to_be32(be64_to_cpu(
				    cnt->counters[0].IfRxOctets) >> 2);
		pma_cnt->port_xmit_packets =
			cpu_to_be32(be64_to_cpu(cnt->counters[0].IfTxFrames));
		pma_cnt->port_rcv_packets  =
			cpu_to_be32(be64_to_cpu(cnt->counters[0].IfRxFrames));
		break;
	}
	case IB_PMA_PORT_COUNTERS_EXT:
	{
		struct ib_pma_portcounters_ext *pma_cnt_ext =
				(struct ib_pma_portcounters_ext *) counters;

		pma_cnt_ext->port_xmit_data =
			cpu_to_be64((be64_to_cpu(cnt->counters[0].
						 IfTxOctets) >> 2));
		pma_cnt_ext->port_rcv_data  =
			cpu_to_be64((be64_to_cpu(cnt->counters[0].
						 IfRxOctets) >> 2));
		pma_cnt_ext->port_xmit_packets = cnt->counters[0].IfTxFrames;
		pma_cnt_ext->port_rcv_packets  = cnt->counters[0].IfRxFrames;
		break;
	}
	default:
		pr_warn("Unsupported attr_id 0x%x\n", attr_id);
		break;
	}
}

int mlx4_ib_query_if_stat(struct mlx4_ib_dev *dev, u32 counter_index,
		       union mlx4_counter *counter, u8 clear)
{
	struct mlx4_cmd_mailbox *mailbox;
	int err;
	u32 inmod = counter_index | ((clear & 1) << 31);

	mailbox = mlx4_alloc_cmd_mailbox(dev->dev);
	if (IS_ERR(mailbox))
		return IB_MAD_RESULT_FAILURE;

	err = mlx4_cmd_box(dev->dev, 0, mailbox->dma, inmod, 0,
			   MLX4_CMD_QUERY_IF_STAT, MLX4_CMD_TIME_CLASS_C,
			   MLX4_CMD_NATIVE);
	if (!err)
		memcpy(counter, mailbox->buf, MLX4_IF_STAT_SZ(1));

	mlx4_free_cmd_mailbox(dev->dev, mailbox);

	return err;
}

static int iboe_process_mad(struct ib_device *ibdev, int mad_flags, u8 port_num,
			struct ib_wc *in_wc, struct ib_grh *in_grh,
			struct ib_mad *in_mad, struct ib_mad *out_mad)
{
	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	int err;
	u32 counter_index = dev->counters[port_num - 1].counter_index & 0xffff;
	u8 mode;
	char				counter_buf[MLX4_IF_STAT_SZ(1)] __aligned(8);
	union  mlx4_counter		*counter = (union mlx4_counter *)
						   counter_buf;

	if (in_mad->mad_hdr.mgmt_class != IB_MGMT_CLASS_PERF_MGMT)
		return -EINVAL;

	/* in case of default counter IB shares the counter with ETH */
	/* the state could be -EEXIST or -ENOSPC */
	if (dev->counters[port_num - 1].status) {
		memset(out_mad->data, 0, sizeof out_mad->data);
		err = IB_MAD_RESULT_SUCCESS | IB_MAD_RESULT_REPLY;
	} else {
		if (mlx4_ib_query_if_stat(dev, counter_index, counter, 0))
			return IB_MAD_RESULT_FAILURE;

		memset(out_mad->data, 0, sizeof(out_mad->data));
		mode = counter->control.cnt_mode & 0xFF;
		err = IB_MAD_RESULT_SUCCESS | IB_MAD_RESULT_REPLY;
		switch (mode & 0xf) {
		case 0:
			edit_counter((void *)counter,
				     (void *)(out_mad->data + 40),
				     in_mad->mad_hdr.attr_id);
			break;
		case 1:
			edit_counter_ext((void *)counter,
					 (void *)(out_mad->data + 40),
					 in_mad->mad_hdr.attr_id);
			break;
		default:
			err = IB_MAD_RESULT_FAILURE;
		}
	}

	return err;
}

int mlx4_ib_process_mad(struct ib_device *ibdev, int mad_flags, u8 port_num,
			struct ib_wc *in_wc, struct ib_grh *in_grh,
			struct ib_mad *in_mad, struct ib_mad *out_mad)
{
	switch (rdma_port_get_link_layer(ibdev, port_num)) {
	case IB_LINK_LAYER_INFINIBAND:
		return ib_process_mad(ibdev, mad_flags, port_num, in_wc,
				      in_grh, in_mad, out_mad);
	case IB_LINK_LAYER_ETHERNET:
		return iboe_process_mad(ibdev, mad_flags, port_num, in_wc,
					  in_grh, in_mad, out_mad);
	default:
		return -EINVAL;
	}
}

static void send_handler(struct ib_mad_agent *agent,
			 struct ib_mad_send_wc *mad_send_wc)
{
	if (mad_send_wc->send_buf->context[0])
		ib_destroy_ah(mad_send_wc->send_buf->context[0]);
	ib_free_send_mad(mad_send_wc->send_buf);
}

int mlx4_ib_mad_init(struct mlx4_ib_dev *dev)
{
	struct ib_mad_agent *agent;
	int p, q;
	int ret;
	enum rdma_link_layer ll;

	for (p = 0; p < dev->num_ports; ++p) {
		ll = rdma_port_get_link_layer(&dev->ib_dev, p + 1);
		for (q = 0; q <= 1; ++q) {
			if (ll == IB_LINK_LAYER_INFINIBAND) {
				agent = ib_register_mad_agent(&dev->ib_dev, p + 1,
							      q ? IB_QPT_GSI : IB_QPT_SMI,
							      NULL, 0, send_handler,
							      NULL, NULL);
				if (IS_ERR(agent)) {
					ret = PTR_ERR(agent);
					goto err;
				}
				dev->send_agent[p][q] = agent;
			} else
				dev->send_agent[p][q] = NULL;
		}
	}

	return 0;

err:
	for (p = 0; p < dev->num_ports; ++p)
		for (q = 0; q <= 1; ++q)
			if (dev->send_agent[p][q])
				ib_unregister_mad_agent(dev->send_agent[p][q]);

	return ret;
}

void mlx4_ib_mad_cleanup(struct mlx4_ib_dev *dev)
{
	struct ib_mad_agent *agent;
	int p, q;

	for (p = 0; p < dev->num_ports; ++p) {
		for (q = 0; q <= 1; ++q) {
			agent = dev->send_agent[p][q];
			if (agent) {
				dev->send_agent[p][q] = NULL;
				ib_unregister_mad_agent(agent);
			}
		}

		if (dev->sm_ah[p])
			ib_destroy_ah(dev->sm_ah[p]);
	}
}

static void handle_lid_change_event(struct mlx4_ib_dev *dev, u8 port_num)
{
	mlx4_ib_dispatch_event(dev, port_num, IB_EVENT_LID_CHANGE);

	if (mlx4_is_master(dev->dev) && !dev->sriov.is_going_down)
		mlx4_gen_slaves_port_mgt_ev(dev->dev, port_num,
					    MLX4_EQ_PORT_INFO_LID_CHANGE_MASK, 0, 0);
}

static void handle_client_rereg_event(struct mlx4_ib_dev *dev, u8 port_num)
{
	/* re-configure the alias-guid and mcg's */
	if (mlx4_is_master(dev->dev)) {
		mlx4_ib_invalidate_all_guid_record(dev, port_num);

		if (!dev->sriov.is_going_down) {
			mlx4_ib_mcg_port_cleanup(&dev->sriov.demux[port_num - 1], 0);
			mlx4_gen_slaves_port_mgt_ev(dev->dev, port_num,
						    MLX4_EQ_PORT_INFO_CLIENT_REREG_MASK, 0, 0);
		}
	}
	mlx4_ib_dispatch_event(dev, port_num, IB_EVENT_CLIENT_REREGISTER);
}

static void propagate_pkey_ev(struct mlx4_ib_dev *dev, int port_num,
			      struct mlx4_eqe *eqe)
{
	__propagate_pkey_ev(dev, port_num, GET_BLK_PTR_FROM_EQE(eqe),
			    GET_MASK_FROM_EQE(eqe));
}

static void handle_slaves_guid_change(struct mlx4_ib_dev *dev, u8 port_num,
				      u32 guid_tbl_blk_num, u32 change_bitmap)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad  = NULL;
	u16 i;

	if (!mlx4_is_mfunc(dev->dev) || !mlx4_is_master(dev->dev))
		return;

	in_mad  = kmalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad) {
		mlx4_ib_warn(&dev->ib_dev, "failed to allocate memory for guid info mads\n");
		goto out;
	}

	guid_tbl_blk_num  *= 4;

	for (i = 0; i < 4; i++) {
		if (change_bitmap && (!((change_bitmap >> (8 * i)) & 0xff)))
			continue;
		memset(in_mad, 0, sizeof *in_mad);
		memset(out_mad, 0, sizeof *out_mad);

		in_mad->base_version  = 1;
		in_mad->mgmt_class    = IB_MGMT_CLASS_SUBN_LID_ROUTED;
		in_mad->class_version = 1;
		in_mad->method        = IB_MGMT_METHOD_GET;
		in_mad->attr_id       = IB_SMP_ATTR_GUID_INFO;
		in_mad->attr_mod      = cpu_to_be32(guid_tbl_blk_num + i);

		if (mlx4_MAD_IFC(dev,
				 MLX4_MAD_IFC_IGNORE_KEYS | MLX4_MAD_IFC_NET_VIEW,
				 port_num, NULL, NULL, in_mad, out_mad)) {
			mlx4_ib_warn(&dev->ib_dev, "Failed in get GUID INFO MAD_IFC\n");
			goto out;
		}

		mlx4_ib_update_cache_on_guid_change(dev, guid_tbl_blk_num + i,
						    port_num,
						    (u8 *)(&((struct ib_smp *)out_mad)->data));
		mlx4_ib_notify_slaves_on_guid_change(dev, guid_tbl_blk_num + i,
						     port_num,
						     (u8 *)(&((struct ib_smp *)out_mad)->data));
	}

out:
	kfree(in_mad);
	kfree(out_mad);
	return;
}

void handle_port_mgmt_change_event(struct work_struct *work)
{
	struct ib_event_work *ew = container_of(work, struct ib_event_work, work);
	struct mlx4_ib_dev *dev = ew->ib_dev;
	struct mlx4_eqe *eqe = &(ew->ib_eqe);
	u8 port = eqe->event.port_mgmt_change.port;
	u32 changed_attr;
	u32 tbl_block;
	u32 change_bitmap;

	switch (eqe->subtype) {
	case MLX4_DEV_PMC_SUBTYPE_PORT_INFO:
		changed_attr = be32_to_cpu(eqe->event.port_mgmt_change.params.port_info.changed_attr);

		/* Update the SM ah - This should be done before handling
		   the other changed attributes so that MADs can be sent to the SM */
		if (changed_attr & MSTR_SM_CHANGE_MASK) {
			u16 lid = be16_to_cpu(eqe->event.port_mgmt_change.params.port_info.mstr_sm_lid);
			u8 sl = eqe->event.port_mgmt_change.params.port_info.mstr_sm_sl & 0xf;
			update_sm_ah(dev, port, lid, sl);
			mlx4_ib_dispatch_event(dev, port, IB_EVENT_SM_CHANGE);
			if (mlx4_is_master(dev->dev))
				mlx4_gen_slaves_port_mgt_ev(dev->dev, port,
							    changed_attr & MSTR_SM_CHANGE_MASK,
							    lid, sl);
		}

		/* Check if it is a lid change event */
		if (changed_attr & MLX4_EQ_PORT_INFO_LID_CHANGE_MASK)
			handle_lid_change_event(dev, port);

		/* Generate GUID changed event */
		if (changed_attr & MLX4_EQ_PORT_INFO_GID_PFX_CHANGE_MASK) {
			mlx4_ib_dispatch_event(dev, port, IB_EVENT_GID_CHANGE);
			/*if master, notify all slaves*/
			if (mlx4_is_master(dev->dev))
				mlx4_gen_slaves_port_mgt_ev(dev->dev, port,
							    MLX4_EQ_PORT_INFO_GID_PFX_CHANGE_MASK, 0, 0);
		}

		if (changed_attr & MLX4_EQ_PORT_INFO_CLIENT_REREG_MASK)
			handle_client_rereg_event(dev, port);
		break;

	case MLX4_DEV_PMC_SUBTYPE_PKEY_TABLE:
		mlx4_ib_dispatch_event(dev, port, IB_EVENT_PKEY_CHANGE);
		if (mlx4_is_master(dev->dev) && !dev->sriov.is_going_down)
			propagate_pkey_ev(dev, port, eqe);
		break;
	case MLX4_DEV_PMC_SUBTYPE_GUID_INFO:
		/* paravirtualized master's guid is guid 0 -- does not change */
		if (!mlx4_is_master(dev->dev))
			mlx4_ib_dispatch_event(dev, port, IB_EVENT_GID_CHANGE);
		/*if master, notify relevant slaves*/
		else if (!dev->sriov.is_going_down) {
			tbl_block = GET_BLK_PTR_FROM_EQE(eqe);
			change_bitmap = GET_MASK_FROM_EQE(eqe);
			handle_slaves_guid_change(dev, port, tbl_block, change_bitmap);
		}
		break;
	default:
		pr_warn("Unsupported subtype 0x%x for "
			"Port Management Change event\n", eqe->subtype);
	}

	kfree(ew);
}

void mlx4_ib_dispatch_event(struct mlx4_ib_dev *dev, u8 port_num,
			    enum ib_event_type type)
{
	struct ib_event event;

	event.device		= &dev->ib_dev;
	event.element.port_num	= port_num;
	event.event		= type;

	ib_dispatch_event(&event);
}

static void mlx4_ib_tunnel_comp_handler(struct ib_cq *cq, void *arg)
{
	unsigned long flags;
	struct mlx4_ib_demux_pv_ctx *ctx = cq->cq_context;
	struct mlx4_ib_dev *dev = to_mdev(ctx->ib_dev);
	spin_lock_irqsave(&dev->sriov.going_down_lock, flags);
	if (!dev->sriov.is_going_down && ctx->state == DEMUX_PV_STATE_ACTIVE)
		queue_work(ctx->wq, &ctx->work);
	spin_unlock_irqrestore(&dev->sriov.going_down_lock, flags);
}

static int mlx4_ib_post_pv_qp_buf(struct mlx4_ib_demux_pv_ctx *ctx,
				  struct mlx4_ib_demux_pv_qp *tun_qp,
				  int index)
{
	struct ib_sge sg_list;
	struct ib_recv_wr recv_wr, *bad_recv_wr;
	int size;

	size = (tun_qp->qp->qp_type == IB_QPT_UD) ?
		sizeof (struct mlx4_tunnel_mad) : sizeof (struct mlx4_mad_rcv_buf);

	sg_list.addr = tun_qp->ring[index].map;
	sg_list.length = size;
	sg_list.lkey = ctx->mr->lkey;

	recv_wr.next = NULL;
	recv_wr.sg_list = &sg_list;
	recv_wr.num_sge = 1;
	recv_wr.wr_id = (u64) index | MLX4_TUN_WRID_RECV |
		MLX4_TUN_SET_WRID_QPN(tun_qp->proxy_qpt);
	ib_dma_sync_single_for_device(ctx->ib_dev, tun_qp->ring[index].map,
				      size, DMA_FROM_DEVICE);
	return ib_post_recv(tun_qp->qp, &recv_wr, &bad_recv_wr);
}

static int mlx4_ib_multiplex_sa_handler(struct ib_device *ibdev, int port,
		int slave, struct ib_sa_mad *sa_mad)
{
	int ret = 0;

	/* dispatch to different sa handlers */
	switch (be16_to_cpu(sa_mad->mad_hdr.attr_id)) {
	case IB_SA_ATTR_MC_MEMBER_REC:
		ret = mlx4_ib_mcg_multiplex_handler(ibdev, port, slave, sa_mad);
		break;
	default:
		break;
	}
	return ret;
}

static int is_proxy_qp0(struct mlx4_ib_dev *dev, int qpn, int slave)
{
	int proxy_start = dev->dev->phys_caps.base_proxy_sqpn + 8 * slave;

	return (qpn >= proxy_start && qpn <= proxy_start + 1);
}


int mlx4_ib_send_to_wire(struct mlx4_ib_dev *dev, int slave, u8 port,
			 enum ib_qp_type dest_qpt, u16 pkey_index,
			 u32 remote_qpn, u32 qkey, struct ib_ah_attr *attr,
			 u8 *s_mac, struct ib_mad *mad)
{
	struct ib_sge list;
	struct ib_send_wr wr, *bad_wr;
	struct mlx4_ib_demux_pv_ctx *sqp_ctx;
	struct mlx4_ib_demux_pv_qp *sqp;
	struct mlx4_mad_snd_buf *sqp_mad;
	struct ib_ah *ah;
	struct ib_qp *send_qp = NULL;
	unsigned wire_tx_ix = 0;
	int ret = 0;
	u16 wire_pkey_ix;
	int src_qpnum;
	u8 sgid_index;


	sqp_ctx = dev->sriov.sqps[port-1];

	/* check if proxy qp created */
	if (!sqp_ctx || sqp_ctx->state != DEMUX_PV_STATE_ACTIVE)
		return -EAGAIN;

	/* QP0 forwarding only for Dom0 */
	if (dest_qpt == IB_QPT_SMI && (mlx4_master_func_num(dev->dev) != slave))
		return -EINVAL;

	if (dest_qpt == IB_QPT_SMI) {
		src_qpnum = 0;
		sqp = &sqp_ctx->qp[0];
		wire_pkey_ix = dev->pkeys.virt2phys_pkey[slave][port - 1][0];
	} else {
		src_qpnum = 1;
		sqp = &sqp_ctx->qp[1];
		wire_pkey_ix = dev->pkeys.virt2phys_pkey[slave][port - 1][pkey_index];
	}

	send_qp = sqp->qp;

	/* create ah */
	sgid_index = attr->grh.sgid_index;
	attr->grh.sgid_index = 0;
	ah = ib_create_ah(sqp_ctx->pd, attr);
	if (IS_ERR(ah))
		return -ENOMEM;
	attr->grh.sgid_index = sgid_index;
	to_mah(ah)->av.ib.gid_index = sgid_index;
	/* get rid of force-loopback bit */
	to_mah(ah)->av.ib.port_pd &= cpu_to_be32(0x7FFFFFFF);
	spin_lock(&sqp->tx_lock);
	if (sqp->tx_ix_head - sqp->tx_ix_tail >=
	    (MLX4_NUM_TUNNEL_BUFS - 1))
		ret = -EAGAIN;
	else
		wire_tx_ix = (++sqp->tx_ix_head) & (MLX4_NUM_TUNNEL_BUFS - 1);
	spin_unlock(&sqp->tx_lock);
	if (ret)
		goto out;

	sqp_mad = (struct mlx4_mad_snd_buf *) (sqp->tx_ring[wire_tx_ix].buf.addr);
	if (sqp->tx_ring[wire_tx_ix].ah)
		ib_destroy_ah(sqp->tx_ring[wire_tx_ix].ah);
	sqp->tx_ring[wire_tx_ix].ah = ah;
	ib_dma_sync_single_for_cpu(&dev->ib_dev,
				   sqp->tx_ring[wire_tx_ix].buf.map,
				   sizeof (struct mlx4_mad_snd_buf),
				   DMA_TO_DEVICE);

	memcpy(&sqp_mad->payload, mad, sizeof *mad);

	ib_dma_sync_single_for_device(&dev->ib_dev,
				      sqp->tx_ring[wire_tx_ix].buf.map,
				      sizeof (struct mlx4_mad_snd_buf),
				      DMA_TO_DEVICE);

	list.addr = sqp->tx_ring[wire_tx_ix].buf.map;
	list.length = sizeof (struct mlx4_mad_snd_buf);
	list.lkey = sqp_ctx->mr->lkey;

	wr.wr.ud.ah = ah;
	wr.wr.ud.port_num = port;
	wr.wr.ud.pkey_index = wire_pkey_ix;
	wr.wr.ud.remote_qkey = qkey;
	wr.wr.ud.remote_qpn = remote_qpn;
	wr.next = NULL;
	wr.wr_id = ((u64) wire_tx_ix) | MLX4_TUN_SET_WRID_QPN(src_qpnum);
	wr.sg_list = &list;
	wr.num_sge = 1;
	wr.opcode = IB_WR_SEND;
	wr.send_flags = IB_SEND_SIGNALED;
	if (s_mac)
		memcpy(to_mah(ah)->av.eth.s_mac, s_mac, 6);


	ret = ib_post_send(send_qp, &wr, &bad_wr);
out:
	if (ret)
		ib_destroy_ah(ah);
	return ret;
}

static int get_slave_base_gid_ix(struct mlx4_ib_dev *dev, int slave, int port)
{
	int gids;
	int vfs;

	if (rdma_port_get_link_layer(&dev->ib_dev, port) == IB_LINK_LAYER_INFINIBAND)
		return slave;

	gids = MLX4_ROCE_MAX_GIDS - MLX4_ROCE_PF_GIDS;
	vfs = dev->dev->num_vfs;

	if (slave == 0)
		return 0;
	if (slave <= gids % vfs)
		return MLX4_ROCE_PF_GIDS + ((gids / vfs) + 1) * (slave - 1);

	return MLX4_ROCE_PF_GIDS + (gids % vfs) + ((gids / vfs) * (slave - 1));
}

static int get_real_sgid_index(struct mlx4_ib_dev *dev, int slave, int port,
			       struct ib_ah_attr *ah_attr)
{
	if (rdma_port_get_link_layer(&dev->ib_dev, port) == IB_LINK_LAYER_INFINIBAND) {
		ah_attr->grh.sgid_index = slave;
		return 0;
	}
	ah_attr->grh.sgid_index += get_slave_base_gid_ix(dev, slave, port);
	return 0;
}

static void mlx4_ib_multiplex_mad(struct mlx4_ib_demux_pv_ctx *ctx, struct ib_wc *wc)
{
	struct mlx4_ib_dev *dev = to_mdev(ctx->ib_dev);
	struct mlx4_ib_demux_pv_qp *tun_qp = &ctx->qp[MLX4_TUN_WRID_QPN(wc->wr_id)];
	int wr_ix = wc->wr_id & (MLX4_NUM_TUNNEL_BUFS - 1);
	struct mlx4_tunnel_mad *tunnel = tun_qp->ring[wr_ix].addr;
	struct mlx4_ib_ah ah;
	struct ib_ah_attr ah_attr;
	u8 *slave_id;
	int slave;

	/* Get slave that sent this packet */
	if (wc->src_qp < dev->dev->phys_caps.base_proxy_sqpn ||
	    wc->src_qp >= dev->dev->phys_caps.base_proxy_sqpn + 8 * MLX4_MFUNC_MAX ||
	    (wc->src_qp & 0x1) != ctx->port - 1 ||
	    wc->src_qp & 0x4) {
		mlx4_ib_warn(ctx->ib_dev, "can't multiplex bad sqp:%d\n", wc->src_qp);
		return;
	}
	slave = ((wc->src_qp & ~0x7) - dev->dev->phys_caps.base_proxy_sqpn) / 8;
	if (slave != ctx->slave) {
		mlx4_ib_warn(ctx->ib_dev, "can't multiplex bad sqp:%d: "
			     "belongs to another slave\n", wc->src_qp);
		return;
	}
	if (slave != mlx4_master_func_num(dev->dev) && !(wc->src_qp & 0x2)) {
		mlx4_ib_warn(ctx->ib_dev, "can't multiplex bad sqp:%d: "
			     "non-master trying to send QP0 packets\n", wc->src_qp);
		return;
	}

	/* Map transaction ID */
	ib_dma_sync_single_for_cpu(ctx->ib_dev, tun_qp->ring[wr_ix].map,
				   sizeof (struct mlx4_tunnel_mad),
				   DMA_FROM_DEVICE);
	switch (tunnel->mad.mad_hdr.method) {
	case IB_MGMT_METHOD_SET:
	case IB_MGMT_METHOD_GET:
	case IB_MGMT_METHOD_REPORT:
	case IB_SA_METHOD_GET_TABLE:
	case IB_SA_METHOD_DELETE:
	case IB_SA_METHOD_GET_MULTI:
	case IB_SA_METHOD_GET_TRACE_TBL:
		slave_id = (u8 *) &tunnel->mad.mad_hdr.tid;
		if (*slave_id) {
			mlx4_ib_warn(ctx->ib_dev, "egress mad has non-null tid msb:%d "
				     "class:%d slave:%d\n", *slave_id,
				     tunnel->mad.mad_hdr.mgmt_class, slave);
			return;
		} else
			*slave_id = slave;
	default:
		/* nothing */;
	}

	/* Class-specific handling */
	switch (tunnel->mad.mad_hdr.mgmt_class) {
	case IB_MGMT_CLASS_SUBN_ADM:
		if (mlx4_ib_multiplex_sa_handler(ctx->ib_dev, ctx->port, slave,
			      (struct ib_sa_mad *) &tunnel->mad))
			return;
		break;
	case IB_MGMT_CLASS_CM:
		if (mlx4_ib_multiplex_cm_handler(ctx->ib_dev, ctx->port, slave,
			      (struct ib_mad *) &tunnel->mad))
			return;
		break;
	case IB_MGMT_CLASS_DEVICE_MGMT:
		if (tunnel->mad.mad_hdr.method != IB_MGMT_METHOD_GET &&
		    tunnel->mad.mad_hdr.method != IB_MGMT_METHOD_SET)
			return;
		break;
	default:
		/* Drop unsupported classes for slaves in tunnel mode */
		if (slave != mlx4_master_func_num(dev->dev)) {
			mlx4_ib_warn(ctx->ib_dev, "dropping unsupported egress mad from class:%d "
				     "for slave:%d\n", tunnel->mad.mad_hdr.mgmt_class, slave);
			return;
		}
	}

	/* We are using standard ib_core services to send the mad, so generate a
	 * stadard address handle by decoding the tunnelled mlx4_ah fields */
	memcpy(&ah.av, &tunnel->hdr.av, sizeof (struct mlx4_av));
	ah.ibah.device = ctx->ib_dev;
	mlx4_ib_query_ah(&ah.ibah, &ah_attr);
	if (ah_attr.ah_flags & IB_AH_GRH)
		if (get_real_sgid_index(dev, slave, ctx->port, &ah_attr))
			return;
	memcpy(ah_attr.dmac, tunnel->hdr.mac, 6);
	ah_attr.vlan_id = tunnel->hdr.vlan;
	/* if slave have default vlan use it */
	mlx4_get_slave_default_vlan(dev->dev, ctx->port, slave,
				    &ah_attr.vlan_id, &ah_attr.sl);

	mlx4_ib_send_to_wire(dev, slave, ctx->port,
			     is_proxy_qp0(dev, wc->src_qp, slave) ?
			     IB_QPT_SMI : IB_QPT_GSI,
			     be16_to_cpu(tunnel->hdr.pkey_index),
			     be32_to_cpu(tunnel->hdr.remote_qpn),
			     be32_to_cpu(tunnel->hdr.qkey),
			     &ah_attr, wc->smac, &tunnel->mad);
}

static int mlx4_ib_alloc_pv_bufs(struct mlx4_ib_demux_pv_ctx *ctx,
				 enum ib_qp_type qp_type, int is_tun)
{
	int i;
	struct mlx4_ib_demux_pv_qp *tun_qp;
	int rx_buf_size, tx_buf_size;

	if (qp_type > IB_QPT_GSI)
		return -EINVAL;

	tun_qp = &ctx->qp[qp_type];

	tun_qp->ring = kzalloc(sizeof (struct mlx4_ib_buf) * MLX4_NUM_TUNNEL_BUFS,
			       GFP_KERNEL);
	if (!tun_qp->ring)
		return -ENOMEM;

	tun_qp->tx_ring = kcalloc(MLX4_NUM_TUNNEL_BUFS,
				  sizeof (struct mlx4_ib_tun_tx_buf),
				  GFP_KERNEL);
	if (!tun_qp->tx_ring) {
		kfree(tun_qp->ring);
		tun_qp->ring = NULL;
		return -ENOMEM;
	}

	if (is_tun) {
		rx_buf_size = sizeof (struct mlx4_tunnel_mad);
		tx_buf_size = sizeof (struct mlx4_rcv_tunnel_mad);
	} else {
		rx_buf_size = sizeof (struct mlx4_mad_rcv_buf);
		tx_buf_size = sizeof (struct mlx4_mad_snd_buf);
	}

	for (i = 0; i < MLX4_NUM_TUNNEL_BUFS; i++) {
		tun_qp->ring[i].addr = kmalloc(rx_buf_size, GFP_KERNEL);
		if (!tun_qp->ring[i].addr)
			goto err;
		tun_qp->ring[i].map = ib_dma_map_single(ctx->ib_dev,
							tun_qp->ring[i].addr,
							rx_buf_size,
							DMA_FROM_DEVICE);
		if (unlikely(ib_dma_mapping_error(ctx->ib_dev,
						  tun_qp->ring[i].map))) {
			mlx4_ib_warn(ctx->ib_dev, "ib_dma_map_single failed\n");
			kfree(tun_qp->ring[i].addr);
			goto err;
		}
	}

	for (i = 0; i < MLX4_NUM_TUNNEL_BUFS; i++) {
		tun_qp->tx_ring[i].buf.addr =
			kmalloc(tx_buf_size, GFP_KERNEL);
		if (!tun_qp->tx_ring[i].buf.addr)
			goto tx_err;
		tun_qp->tx_ring[i].buf.map =
			ib_dma_map_single(ctx->ib_dev,
					  tun_qp->tx_ring[i].buf.addr,
					  tx_buf_size,
					  DMA_TO_DEVICE);
		if (unlikely(ib_dma_mapping_error(ctx->ib_dev,
						  tun_qp->tx_ring[i].buf.map))) {
			mlx4_ib_warn(ctx->ib_dev, "ib_dma_map_single failed\n");
			kfree(tun_qp->tx_ring[i].buf.addr);
			goto tx_err;
		}
		tun_qp->tx_ring[i].ah = NULL;
	}
	spin_lock_init(&tun_qp->tx_lock);
	tun_qp->tx_ix_head = 0;
	tun_qp->tx_ix_tail = 0;
	tun_qp->proxy_qpt = qp_type;

	return 0;

tx_err:
	while (i > 0) {
		--i;
		ib_dma_unmap_single(ctx->ib_dev, tun_qp->tx_ring[i].buf.map,
				    tx_buf_size, DMA_TO_DEVICE);
		kfree(tun_qp->tx_ring[i].buf.addr);
	}
	kfree(tun_qp->tx_ring);
	tun_qp->tx_ring = NULL;
	i = MLX4_NUM_TUNNEL_BUFS;
err:
	while (i > 0) {
		--i;
		ib_dma_unmap_single(ctx->ib_dev, tun_qp->ring[i].map,
				    rx_buf_size, DMA_FROM_DEVICE);
		kfree(tun_qp->ring[i].addr);
	}
	kfree(tun_qp->ring);
	tun_qp->ring = NULL;
	return -ENOMEM;
}

static void mlx4_ib_free_pv_qp_bufs(struct mlx4_ib_demux_pv_ctx *ctx,
				     enum ib_qp_type qp_type, int is_tun)
{
	int i;
	struct mlx4_ib_demux_pv_qp *tun_qp;
	int rx_buf_size, tx_buf_size;

	if (qp_type > IB_QPT_GSI)
		return;

	tun_qp = &ctx->qp[qp_type];
	if (is_tun) {
		rx_buf_size = sizeof (struct mlx4_tunnel_mad);
		tx_buf_size = sizeof (struct mlx4_rcv_tunnel_mad);
	} else {
		rx_buf_size = sizeof (struct mlx4_mad_rcv_buf);
		tx_buf_size = sizeof (struct mlx4_mad_snd_buf);
	}


	for (i = 0; i < MLX4_NUM_TUNNEL_BUFS; i++) {
		ib_dma_unmap_single(ctx->ib_dev, tun_qp->ring[i].map,
				    rx_buf_size, DMA_FROM_DEVICE);
		kfree(tun_qp->ring[i].addr);
	}

	for (i = 0; i < MLX4_NUM_TUNNEL_BUFS; i++) {
		ib_dma_unmap_single(ctx->ib_dev, tun_qp->tx_ring[i].buf.map,
				    tx_buf_size, DMA_TO_DEVICE);
		kfree(tun_qp->tx_ring[i].buf.addr);
		if (tun_qp->tx_ring[i].ah)
			ib_destroy_ah(tun_qp->tx_ring[i].ah);
	}
	kfree(tun_qp->tx_ring);
	kfree(tun_qp->ring);
}

static void mlx4_ib_tunnel_comp_worker(struct work_struct *work)
{
	struct mlx4_ib_demux_pv_ctx *ctx;
	struct mlx4_ib_demux_pv_qp *tun_qp;
	struct ib_wc wc;
	int ret;
	ctx = container_of(work, struct mlx4_ib_demux_pv_ctx, work);
	ib_req_notify_cq(ctx->cq, IB_CQ_NEXT_COMP);

	while (ib_poll_cq(ctx->cq, 1, &wc) == 1) {
		tun_qp = &ctx->qp[MLX4_TUN_WRID_QPN(wc.wr_id)];
		if (wc.status == IB_WC_SUCCESS) {
			switch (wc.opcode) {
			case IB_WC_RECV:
				mlx4_ib_multiplex_mad(ctx, &wc);
				ret = mlx4_ib_post_pv_qp_buf(ctx, tun_qp,
							     wc.wr_id &
							     (MLX4_NUM_TUNNEL_BUFS - 1));
				if (ret)
					pr_err("Failed reposting tunnel "
					       "buf:%lld\n", (unsigned long long)wc.wr_id);
				break;
			case IB_WC_SEND:
				pr_debug("received tunnel send completion:"
					 "wrid=0x%llx, status=0x%x\n",
					 (unsigned long long)wc.wr_id, wc.status);
				ib_destroy_ah(tun_qp->tx_ring[wc.wr_id &
					      (MLX4_NUM_TUNNEL_BUFS - 1)].ah);
				tun_qp->tx_ring[wc.wr_id & (MLX4_NUM_TUNNEL_BUFS - 1)].ah
					= NULL;
				spin_lock(&tun_qp->tx_lock);
				tun_qp->tx_ix_tail++;
				spin_unlock(&tun_qp->tx_lock);

				break;
			default:
				break;
			}
		} else  {
			pr_debug("mlx4_ib: completion error in tunnel: %d."
				 " status = %d, wrid = 0x%llx\n",
				 ctx->slave, wc.status, (unsigned long long)wc.wr_id);
			if (!MLX4_TUN_IS_RECV(wc.wr_id)) {
				ib_destroy_ah(tun_qp->tx_ring[wc.wr_id &
					      (MLX4_NUM_TUNNEL_BUFS - 1)].ah);
				tun_qp->tx_ring[wc.wr_id & (MLX4_NUM_TUNNEL_BUFS - 1)].ah
					= NULL;
				spin_lock(&tun_qp->tx_lock);
				tun_qp->tx_ix_tail++;
				spin_unlock(&tun_qp->tx_lock);
			}
		}
	}
}

static void pv_qp_event_handler(struct ib_event *event, void *qp_context)
{
	struct mlx4_ib_demux_pv_ctx *sqp = qp_context;

	/* It's worse than that! He's dead, Jim! */
	pr_err("Fatal error (%d) on a MAD QP on port %d\n",
	       event->event, sqp->port);
}

static int create_pv_sqp(struct mlx4_ib_demux_pv_ctx *ctx,
			    enum ib_qp_type qp_type, int create_tun)
{
	int i, ret;
	struct mlx4_ib_demux_pv_qp *tun_qp;
	struct mlx4_ib_qp_tunnel_init_attr qp_init_attr;
	struct ib_qp_attr attr;
	int qp_attr_mask_INIT;

	if (qp_type > IB_QPT_GSI)
		return -EINVAL;

	tun_qp = &ctx->qp[qp_type];

	memset(&qp_init_attr, 0, sizeof qp_init_attr);
	qp_init_attr.init_attr.send_cq = ctx->cq;
	qp_init_attr.init_attr.recv_cq = ctx->cq;
	qp_init_attr.init_attr.sq_sig_type = IB_SIGNAL_ALL_WR;
	qp_init_attr.init_attr.cap.max_send_wr = MLX4_NUM_TUNNEL_BUFS;
	qp_init_attr.init_attr.cap.max_recv_wr = MLX4_NUM_TUNNEL_BUFS;
	qp_init_attr.init_attr.cap.max_send_sge = 1;
	qp_init_attr.init_attr.cap.max_recv_sge = 1;
	if (create_tun) {
		qp_init_attr.init_attr.qp_type = IB_QPT_UD;
		qp_init_attr.init_attr.create_flags = (enum ib_qp_create_flags)MLX4_IB_SRIOV_TUNNEL_QP;
		qp_init_attr.port = ctx->port;
		qp_init_attr.slave = ctx->slave;
		qp_init_attr.proxy_qp_type = qp_type;
		qp_attr_mask_INIT = IB_QP_STATE | IB_QP_PKEY_INDEX |
			   IB_QP_QKEY | IB_QP_PORT;
	} else {
		qp_init_attr.init_attr.qp_type = qp_type;
		qp_init_attr.init_attr.create_flags = (enum ib_qp_create_flags)MLX4_IB_SRIOV_SQP;
		qp_attr_mask_INIT = IB_QP_STATE | IB_QP_PKEY_INDEX | IB_QP_QKEY;
	}
	qp_init_attr.init_attr.port_num = ctx->port;
	qp_init_attr.init_attr.qp_context = ctx;
	qp_init_attr.init_attr.event_handler = pv_qp_event_handler;
	tun_qp->qp = ib_create_qp(ctx->pd, &qp_init_attr.init_attr);
	if (IS_ERR(tun_qp->qp)) {
		ret = PTR_ERR(tun_qp->qp);
		tun_qp->qp = NULL;
		pr_err("Couldn't create %s QP (%d)\n",
		       create_tun ? "tunnel" : "special", ret);
		return ret;
	}

	memset(&attr, 0, sizeof attr);
	attr.qp_state = IB_QPS_INIT;
	ret = 0;
	if (create_tun)
		ret = find_slave_port_pkey_ix(to_mdev(ctx->ib_dev), ctx->slave,
					      ctx->port, 0xFFFF, &attr.pkey_index);
	if (ret || !create_tun)
		attr.pkey_index =
			to_mdev(ctx->ib_dev)->pkeys.virt2phys_pkey[ctx->slave][ctx->port - 1][0];
	attr.qkey = IB_QP1_QKEY;
	attr.port_num = ctx->port;
	ret = ib_modify_qp(tun_qp->qp, &attr, qp_attr_mask_INIT);
	if (ret) {
		pr_err("Couldn't change %s qp state to INIT (%d)\n",
		       create_tun ? "tunnel" : "special", ret);
		goto err_qp;
	}
	attr.qp_state = IB_QPS_RTR;
	ret = ib_modify_qp(tun_qp->qp, &attr, IB_QP_STATE);
	if (ret) {
		pr_err("Couldn't change %s qp state to RTR (%d)\n",
		       create_tun ? "tunnel" : "special", ret);
		goto err_qp;
	}
	attr.qp_state = IB_QPS_RTS;
	attr.sq_psn = 0;
	ret = ib_modify_qp(tun_qp->qp, &attr, IB_QP_STATE | IB_QP_SQ_PSN);
	if (ret) {
		pr_err("Couldn't change %s qp state to RTS (%d)\n",
		       create_tun ? "tunnel" : "special", ret);
		goto err_qp;
	}

	for (i = 0; i < MLX4_NUM_TUNNEL_BUFS; i++) {
		ret = mlx4_ib_post_pv_qp_buf(ctx, tun_qp, i);
		if (ret) {
			pr_err(" mlx4_ib_post_pv_buf error"
			       " (err = %d, i = %d)\n", ret, i);
			goto err_qp;
		}
	}
	return 0;

err_qp:
	ib_destroy_qp(tun_qp->qp);
	tun_qp->qp = NULL;
	return ret;
}

/*
 * IB MAD completion callback for real SQPs
 */
static void mlx4_ib_sqp_comp_worker(struct work_struct *work)
{
	struct mlx4_ib_demux_pv_ctx *ctx;
	struct mlx4_ib_demux_pv_qp *sqp;
	struct ib_wc wc;
	struct ib_grh *grh;
	struct ib_mad *mad;

	ctx = container_of(work, struct mlx4_ib_demux_pv_ctx, work);
	ib_req_notify_cq(ctx->cq, IB_CQ_NEXT_COMP);

	while (mlx4_ib_poll_cq(ctx->cq, 1, &wc) == 1) {
		sqp = &ctx->qp[MLX4_TUN_WRID_QPN(wc.wr_id)];
		if (wc.status == IB_WC_SUCCESS) {
			switch (wc.opcode) {
			case IB_WC_SEND:
				ib_destroy_ah(sqp->tx_ring[wc.wr_id &
					      (MLX4_NUM_TUNNEL_BUFS - 1)].ah);
				sqp->tx_ring[wc.wr_id & (MLX4_NUM_TUNNEL_BUFS - 1)].ah
					= NULL;
				spin_lock(&sqp->tx_lock);
				sqp->tx_ix_tail++;
				spin_unlock(&sqp->tx_lock);
				break;
			case IB_WC_RECV:
				mad = (struct ib_mad *) &(((struct mlx4_mad_rcv_buf *)
						(sqp->ring[wc.wr_id &
						(MLX4_NUM_TUNNEL_BUFS - 1)].addr))->payload);
				grh = &(((struct mlx4_mad_rcv_buf *)
						(sqp->ring[wc.wr_id &
						(MLX4_NUM_TUNNEL_BUFS - 1)].addr))->grh);
				mlx4_ib_demux_mad(ctx->ib_dev, ctx->port, &wc, grh, mad);
				if (mlx4_ib_post_pv_qp_buf(ctx, sqp, wc.wr_id &
							   (MLX4_NUM_TUNNEL_BUFS - 1)))
					pr_err("Failed reposting SQP "
					       "buf:%lld\n", (unsigned long long)wc.wr_id);
				break;
			default:
				BUG_ON(1);
				break;
			}
		} else  {
			pr_debug("mlx4_ib: completion error in tunnel: %d."
				 " status = %d, wrid = 0x%llx\n",
				 ctx->slave, wc.status, (unsigned long long)wc.wr_id);
			if (!MLX4_TUN_IS_RECV(wc.wr_id)) {
				ib_destroy_ah(sqp->tx_ring[wc.wr_id &
					      (MLX4_NUM_TUNNEL_BUFS - 1)].ah);
				sqp->tx_ring[wc.wr_id & (MLX4_NUM_TUNNEL_BUFS - 1)].ah
					= NULL;
				spin_lock(&sqp->tx_lock);
				sqp->tx_ix_tail++;
				spin_unlock(&sqp->tx_lock);
			}
		}
	}
}

static int alloc_pv_object(struct mlx4_ib_dev *dev, int slave, int port,
			       struct mlx4_ib_demux_pv_ctx **ret_ctx)
{
	struct mlx4_ib_demux_pv_ctx *ctx;

	*ret_ctx = NULL;
	ctx = kzalloc(sizeof (struct mlx4_ib_demux_pv_ctx), GFP_KERNEL);
	if (!ctx) {
		pr_err("failed allocating pv resource context "
		       "for port %d, slave %d\n", port, slave);
		return -ENOMEM;
	}

	ctx->ib_dev = &dev->ib_dev;
	ctx->port = port;
	ctx->slave = slave;
	*ret_ctx = ctx;
	return 0;
}

static void free_pv_object(struct mlx4_ib_dev *dev, int slave, int port)
{
	if (dev->sriov.demux[port - 1].tun[slave]) {
		kfree(dev->sriov.demux[port - 1].tun[slave]);
		dev->sriov.demux[port - 1].tun[slave] = NULL;
	}
}

static int create_pv_resources(struct ib_device *ibdev, int slave, int port,
			       int create_tun, struct mlx4_ib_demux_pv_ctx *ctx)
{
	int ret, cq_size;

	if (ctx->state != DEMUX_PV_STATE_DOWN)
		return -EEXIST;

	ctx->state = DEMUX_PV_STATE_STARTING;
	/* have QP0 only on port owner, and only if link layer is IB */
	if (ctx->slave == mlx4_master_func_num(to_mdev(ctx->ib_dev)->dev) &&
	    rdma_port_get_link_layer(ibdev, ctx->port) == IB_LINK_LAYER_INFINIBAND)
		ctx->has_smi = 1;

	if (ctx->has_smi) {
		ret = mlx4_ib_alloc_pv_bufs(ctx, IB_QPT_SMI, create_tun);
		if (ret) {
			pr_err("Failed allocating qp0 tunnel bufs (%d)\n", ret);
			goto err_out;
		}
	}

	ret = mlx4_ib_alloc_pv_bufs(ctx, IB_QPT_GSI, create_tun);
	if (ret) {
		pr_err("Failed allocating qp1 tunnel bufs (%d)\n", ret);
		goto err_out_qp0;
	}

	cq_size = 2 * MLX4_NUM_TUNNEL_BUFS;
	if (ctx->has_smi)
		cq_size *= 2;

	ctx->cq = ib_create_cq(ctx->ib_dev, mlx4_ib_tunnel_comp_handler,
			       NULL, ctx, cq_size, 0);
	if (IS_ERR(ctx->cq)) {
		ret = PTR_ERR(ctx->cq);
		pr_err("Couldn't create tunnel CQ (%d)\n", ret);
		goto err_buf;
	}

	ctx->pd = ib_alloc_pd(ctx->ib_dev);
	if (IS_ERR(ctx->pd)) {
		ret = PTR_ERR(ctx->pd);
		pr_err("Couldn't create tunnel PD (%d)\n", ret);
		goto err_cq;
	}

	ctx->mr = ib_get_dma_mr(ctx->pd, IB_ACCESS_LOCAL_WRITE);
	if (IS_ERR(ctx->mr)) {
		ret = PTR_ERR(ctx->mr);
		pr_err("Couldn't get tunnel DMA MR (%d)\n", ret);
		goto err_pd;
	}

	if (ctx->has_smi) {
		ret = create_pv_sqp(ctx, IB_QPT_SMI, create_tun);
		if (ret) {
			pr_err("Couldn't create %s QP0 (%d)\n",
			       create_tun ? "tunnel for" : "",  ret);
			goto err_mr;
		}
	}

	ret = create_pv_sqp(ctx, IB_QPT_GSI, create_tun);
	if (ret) {
		pr_err("Couldn't create %s QP1 (%d)\n",
		       create_tun ? "tunnel for" : "",  ret);
		goto err_qp0;
	}

	if (create_tun)
		INIT_WORK(&ctx->work, mlx4_ib_tunnel_comp_worker);
	else
		INIT_WORK(&ctx->work, mlx4_ib_sqp_comp_worker);

	ctx->wq = to_mdev(ibdev)->sriov.demux[port - 1].wq;

	ret = ib_req_notify_cq(ctx->cq, IB_CQ_NEXT_COMP);
	if (ret) {
		pr_err("Couldn't arm tunnel cq (%d)\n", ret);
		goto err_wq;
	}
	ctx->state = DEMUX_PV_STATE_ACTIVE;
	return 0;

err_wq:
	ctx->wq = NULL;
	ib_destroy_qp(ctx->qp[1].qp);
	ctx->qp[1].qp = NULL;


err_qp0:
	if (ctx->has_smi)
		ib_destroy_qp(ctx->qp[0].qp);
	ctx->qp[0].qp = NULL;

err_mr:
	ib_dereg_mr(ctx->mr);
	ctx->mr = NULL;

err_pd:
	ib_dealloc_pd(ctx->pd);
	ctx->pd = NULL;

err_cq:
	ib_destroy_cq(ctx->cq);
	ctx->cq = NULL;

err_buf:
	mlx4_ib_free_pv_qp_bufs(ctx, IB_QPT_GSI, create_tun);

err_out_qp0:
	if (ctx->has_smi)
		mlx4_ib_free_pv_qp_bufs(ctx, IB_QPT_SMI, create_tun);
err_out:
	ctx->state = DEMUX_PV_STATE_DOWN;
	return ret;
}

static void destroy_pv_resources(struct mlx4_ib_dev *dev, int slave, int port,
				 struct mlx4_ib_demux_pv_ctx *ctx, int flush)
{
	if (!ctx)
		return;
	if (ctx->state > DEMUX_PV_STATE_DOWN) {
		ctx->state = DEMUX_PV_STATE_DOWNING;
		if (flush)
			flush_workqueue(ctx->wq);
		if (ctx->has_smi) {
			ib_destroy_qp(ctx->qp[0].qp);
			ctx->qp[0].qp = NULL;
			mlx4_ib_free_pv_qp_bufs(ctx, IB_QPT_SMI, 1);
		}
		ib_destroy_qp(ctx->qp[1].qp);
		ctx->qp[1].qp = NULL;
		mlx4_ib_free_pv_qp_bufs(ctx, IB_QPT_GSI, 1);
		ib_dereg_mr(ctx->mr);
		ctx->mr = NULL;
		ib_dealloc_pd(ctx->pd);
		ctx->pd = NULL;
		ib_destroy_cq(ctx->cq);
		ctx->cq = NULL;
		ctx->state = DEMUX_PV_STATE_DOWN;
	}
}

static int mlx4_ib_tunnels_update(struct mlx4_ib_dev *dev, int slave,
				  int port, int do_init)
{
	int ret = 0;

	if (!do_init) {
		clean_vf_mcast(&dev->sriov.demux[port - 1], slave);
		/* for master, destroy real sqp resources */
		if (slave == mlx4_master_func_num(dev->dev))
			destroy_pv_resources(dev, slave, port,
					     dev->sriov.sqps[port - 1], 1);
		/* destroy the tunnel qp resources */
		destroy_pv_resources(dev, slave, port,
				     dev->sriov.demux[port - 1].tun[slave], 1);
		return 0;
	}

	/* create the tunnel qp resources */
	ret = create_pv_resources(&dev->ib_dev, slave, port, 1,
				  dev->sriov.demux[port - 1].tun[slave]);

	/* for master, create the real sqp resources */
	if (!ret && slave == mlx4_master_func_num(dev->dev))
		ret = create_pv_resources(&dev->ib_dev, slave, port, 0,
					  dev->sriov.sqps[port - 1]);
	return ret;
}

void mlx4_ib_tunnels_update_work(struct work_struct *work)
{
	struct mlx4_ib_demux_work *dmxw;

	dmxw = container_of(work, struct mlx4_ib_demux_work, work);
	mlx4_ib_tunnels_update(dmxw->dev, dmxw->slave, (int) dmxw->port,
			       dmxw->do_init);
	kfree(dmxw);
	return;
}

static int mlx4_ib_alloc_demux_ctx(struct mlx4_ib_dev *dev,
				       struct mlx4_ib_demux_ctx *ctx,
				       int port)
{
	char name[12];
	int ret = 0;
	int i;

	ctx->tun = kcalloc(dev->dev->caps.sqp_demux,
			   sizeof (struct mlx4_ib_demux_pv_ctx *), GFP_KERNEL);
	if (!ctx->tun)
		return -ENOMEM;

	ctx->dev = dev;
	ctx->port = port;
	ctx->ib_dev = &dev->ib_dev;

	for (i = 0; i < dev->dev->caps.sqp_demux; i++) {
		ret = alloc_pv_object(dev, i, port, &ctx->tun[i]);
		if (ret) {
			ret = -ENOMEM;
			goto err_mcg;
		}
	}

	ret = mlx4_ib_mcg_port_init(ctx);
	if (ret) {
		pr_err("Failed initializing mcg para-virt (%d)\n", ret);
		goto err_mcg;
	}

	snprintf(name, sizeof name, "mlx4_ibt%d", port);
	ctx->wq = create_singlethread_workqueue(name);
	if (!ctx->wq) {
		pr_err("Failed to create tunnelling WQ for port %d\n", port);
		ret = -ENOMEM;
		goto err_wq;
	}

	snprintf(name, sizeof name, "mlx4_ibud%d", port);
	ctx->ud_wq = create_singlethread_workqueue(name);
	if (!ctx->ud_wq) {
		pr_err("Failed to create up/down WQ for port %d\n", port);
		ret = -ENOMEM;
		goto err_udwq;
	}

	return 0;

err_udwq:
	destroy_workqueue(ctx->wq);
	ctx->wq = NULL;

err_wq:
	mlx4_ib_mcg_port_cleanup(ctx, 1);
err_mcg:
	for (i = 0; i < dev->dev->caps.sqp_demux; i++)
		free_pv_object(dev, i, port);
	kfree(ctx->tun);
	ctx->tun = NULL;
	return ret;
}

static void mlx4_ib_free_sqp_ctx(struct mlx4_ib_demux_pv_ctx *sqp_ctx)
{
	if (sqp_ctx->state > DEMUX_PV_STATE_DOWN) {
		sqp_ctx->state = DEMUX_PV_STATE_DOWNING;
		flush_workqueue(sqp_ctx->wq);
		if (sqp_ctx->has_smi) {
			ib_destroy_qp(sqp_ctx->qp[0].qp);
			sqp_ctx->qp[0].qp = NULL;
			mlx4_ib_free_pv_qp_bufs(sqp_ctx, IB_QPT_SMI, 0);
		}
		ib_destroy_qp(sqp_ctx->qp[1].qp);
		sqp_ctx->qp[1].qp = NULL;
		mlx4_ib_free_pv_qp_bufs(sqp_ctx, IB_QPT_GSI, 0);
		ib_dereg_mr(sqp_ctx->mr);
		sqp_ctx->mr = NULL;
		ib_dealloc_pd(sqp_ctx->pd);
		sqp_ctx->pd = NULL;
		ib_destroy_cq(sqp_ctx->cq);
		sqp_ctx->cq = NULL;
		sqp_ctx->state = DEMUX_PV_STATE_DOWN;
	}
}

static void mlx4_ib_free_demux_ctx(struct mlx4_ib_demux_ctx *ctx)
{
	int i;
	if (ctx) {
		struct mlx4_ib_dev *dev = to_mdev(ctx->ib_dev);
		mlx4_ib_mcg_port_cleanup(ctx, 1);
		for (i = 0; i < dev->dev->caps.sqp_demux; i++) {
			if (!ctx->tun[i])
				continue;
			if (ctx->tun[i]->state > DEMUX_PV_STATE_DOWN)
				ctx->tun[i]->state = DEMUX_PV_STATE_DOWNING;
		}
		flush_workqueue(ctx->wq);
		for (i = 0; i < dev->dev->caps.sqp_demux; i++) {
			destroy_pv_resources(dev, i, ctx->port, ctx->tun[i], 0);
			free_pv_object(dev, i, ctx->port);
		}
		kfree(ctx->tun);
		destroy_workqueue(ctx->ud_wq);
		destroy_workqueue(ctx->wq);
	}
}

static void mlx4_ib_master_tunnels(struct mlx4_ib_dev *dev, int do_init)
{
	int i;

	if (!mlx4_is_master(dev->dev))
		return;
	/* initialize or tear down tunnel QPs for the master */
	for (i = 0; i < dev->dev->caps.num_ports; i++)
		mlx4_ib_tunnels_update(dev, mlx4_master_func_num(dev->dev), i + 1, do_init);
	return;
}

int mlx4_ib_init_sriov(struct mlx4_ib_dev *dev)
{
	int i = 0;
	int err;

	if (!mlx4_is_mfunc(dev->dev))
		return 0;

	dev->sriov.is_going_down = 0;
	spin_lock_init(&dev->sriov.going_down_lock);
	mlx4_ib_cm_paravirt_init(dev);

	mlx4_ib_warn(&dev->ib_dev, "multi-function enabled\n");

	if (mlx4_is_slave(dev->dev)) {
		mlx4_ib_warn(&dev->ib_dev, "operating in qp1 tunnel mode\n");
		return 0;
	}

	for (i = 0; i < dev->dev->caps.sqp_demux; i++) {
		if (i == mlx4_master_func_num(dev->dev))
			mlx4_put_slave_node_guid(dev->dev, i, dev->ib_dev.node_guid);
		else
			mlx4_put_slave_node_guid(dev->dev, i, mlx4_ib_gen_node_guid());
	}

	err = mlx4_ib_init_alias_guid_service(dev);
	if (err) {
		mlx4_ib_warn(&dev->ib_dev, "Failed init alias guid process.\n");
		goto paravirt_err;
	}
	err = mlx4_ib_device_register_sysfs(dev);
	if (err) {
		mlx4_ib_warn(&dev->ib_dev, "Failed to register sysfs\n");
		goto sysfs_err;
	}

	mlx4_ib_warn(&dev->ib_dev, "initializing demux service for %d qp1 clients\n",
		     dev->dev->caps.sqp_demux);
	for (i = 0; i < dev->num_ports; i++) {
		union ib_gid gid;
		err = __mlx4_ib_query_gid(&dev->ib_dev, i + 1, 0, &gid, 1);
		if (err)
			goto demux_err;
		dev->sriov.demux[i].guid_cache[0] = gid.global.interface_id;
		err = alloc_pv_object(dev, mlx4_master_func_num(dev->dev), i + 1,
				      &dev->sriov.sqps[i]);
		if (err)
			goto demux_err;
		err = mlx4_ib_alloc_demux_ctx(dev, &dev->sriov.demux[i], i + 1);
		if (err)
			goto demux_err;
	}
	mlx4_ib_master_tunnels(dev, 1);
	return 0;

demux_err:
	while (i > 0) {
		free_pv_object(dev, mlx4_master_func_num(dev->dev), i + 1);
		mlx4_ib_free_demux_ctx(&dev->sriov.demux[i]);
		--i;
	}
	mlx4_ib_device_unregister_sysfs(dev);

sysfs_err:
	mlx4_ib_destroy_alias_guid_service(dev);

paravirt_err:
	mlx4_ib_cm_paravirt_clean(dev, -1);

	return err;
}

void mlx4_ib_close_sriov(struct mlx4_ib_dev *dev)
{
	int i;
	unsigned long flags;

	if (!mlx4_is_mfunc(dev->dev))
		return;

	spin_lock_irqsave(&dev->sriov.going_down_lock, flags);
	dev->sriov.is_going_down = 1;
	spin_unlock_irqrestore(&dev->sriov.going_down_lock, flags);
	if (mlx4_is_master(dev->dev)) {
		for (i = 0; i < dev->num_ports; i++) {
			flush_workqueue(dev->sriov.demux[i].ud_wq);
			mlx4_ib_free_sqp_ctx(dev->sriov.sqps[i]);
			kfree(dev->sriov.sqps[i]);
			dev->sriov.sqps[i] = NULL;
			mlx4_ib_free_demux_ctx(&dev->sriov.demux[i]);
		}

		mlx4_ib_cm_paravirt_clean(dev, -1);
		mlx4_ib_destroy_alias_guid_service(dev);
		mlx4_ib_device_unregister_sysfs(dev);
	}
}
