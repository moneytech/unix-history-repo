/*
 * Copyright (C) 2011 Luigi Rizzo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $FreeBSD$
 * $Id: if_re_netmap.h 9802 2011-12-02 18:42:37Z luigi $
 *
 * netmap support for if_re
 */

#include <net/netmap.h>
#include <sys/selinfo.h>
#include <vm/vm.h>
#include <vm/pmap.h>    /* vtophys ? */
#include <dev/netmap/netmap_kern.h>

static int re_netmap_reg(struct ifnet *, int onoff);
static int re_netmap_txsync(void *, u_int, int);
static int re_netmap_rxsync(void *, u_int, int);
static void re_netmap_lock_wrapper(void *, int, u_int);

static void
re_netmap_attach(struct rl_softc *sc)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = sc->rl_ifp;
	na.separate_locks = 0;
	na.num_tx_desc = sc->rl_ldata.rl_tx_desc_cnt;
	na.num_rx_desc = sc->rl_ldata.rl_rx_desc_cnt;
	na.nm_txsync = re_netmap_txsync;
	na.nm_rxsync = re_netmap_rxsync;
	na.nm_lock = re_netmap_lock_wrapper;
	na.nm_register = re_netmap_reg;
	na.buff_size = NETMAP_BUF_SIZE;
	netmap_attach(&na, 1);
}


/*
 * wrapper to export locks to the generic code
 * We should not use the tx/rx locks
 */
static void
re_netmap_lock_wrapper(void *_a, int what, u_int queueid)
{
	struct rl_softc *adapter = _a;

	switch (what) {
	case NETMAP_CORE_LOCK:
		RL_LOCK(adapter);
		break;
	case NETMAP_CORE_UNLOCK:
		RL_UNLOCK(adapter);
		break;

	case NETMAP_TX_LOCK:
	case NETMAP_RX_LOCK:
	case NETMAP_TX_UNLOCK:
	case NETMAP_RX_UNLOCK:
		D("invalid lock call %d, no tx/rx locks here", what);
		break;
	}
}


/*
 * support for netmap register/unregisted. We are already under core lock.
 * only called on the first register or the last unregister.
 */
static int
re_netmap_reg(struct ifnet *ifp, int onoff)
{
	struct rl_softc *adapter = ifp->if_softc;
	struct netmap_adapter *na = NA(ifp);
	int error = 0;

	if (na == NULL)
		return EINVAL;
	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	re_stop(adapter);

	if (onoff) {
		ifp->if_capenable |= IFCAP_NETMAP;

		/* save if_transmit to restore it later */
		na->if_transmit = ifp->if_transmit;
		ifp->if_transmit = netmap_start;

		re_init_locked(adapter);

		if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) == 0) {
			error = ENOMEM;
			goto fail;
		}
	} else {
fail:
		/* restore if_transmit */
		ifp->if_transmit = na->if_transmit;
		ifp->if_capenable &= ~IFCAP_NETMAP;
		re_init_locked(adapter);	/* also enables intr */
	}
	return (error);
}


/*
 * Reconcile kernel and user view of the transmit ring.
 */
static int
re_netmap_txsync(void *a, u_int ring_nr, int do_lock)
{
	struct rl_softc *sc = a;
	struct rl_txdesc *txd = sc->rl_ldata.rl_tx_desc;
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_kring *kring = &na->tx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, k, l, n, lim = kring->nkr_num_slots - 1;

	k = ring->cur;
	if (k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		RL_LOCK(sc);

	/* Sync the TX descriptor list */
	bus_dmamap_sync(sc->rl_ldata.rl_tx_list_tag,
            sc->rl_ldata.rl_tx_list_map,
            BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/* XXX move after the transmissions */
	/* record completed transmissions */
        for (n = 0, l = sc->rl_ldata.rl_tx_considx;
	    l != sc->rl_ldata.rl_tx_prodidx;
	    n++, l = RL_TX_DESC_NXT(sc, l)) {
		uint32_t cmdstat =
			le32toh(sc->rl_ldata.rl_tx_list[l].rl_cmdstat);
		if (cmdstat & RL_TDESC_STAT_OWN)
			break;
	}
	if (n > 0) {
		sc->rl_ldata.rl_tx_considx = l;
		sc->rl_ldata.rl_tx_free += n;
		kring->nr_hwavail += n;
	}

	/* update avail to what the hardware knows */
	ring->avail = kring->nr_hwavail;
	
	j = kring->nr_hwcur;
	if (j != k) {	/* we have new packets to send */
		n = 0;
		l = sc->rl_ldata.rl_tx_prodidx;
		while (j != k) {
			struct netmap_slot *slot = &ring->slot[j];
			struct rl_desc *desc = &sc->rl_ldata.rl_tx_list[l];
			int cmd = slot->len | RL_TDESC_CMD_EOF |
				RL_TDESC_CMD_OWN | RL_TDESC_CMD_SOF ;
			void *addr = NMB(slot);
			int len = slot->len;

			if (addr == netmap_buffer_base || len > NETMAP_BUF_SIZE) {
				if (do_lock)
					RL_UNLOCK(sc);
				// XXX what about prodidx ?
				return netmap_ring_reinit(kring);
			}
			
			if (l == lim)	/* mark end of ring */
				cmd |= RL_TDESC_CMD_EOR;

			if (slot->flags & NS_BUF_CHANGED) {
				uint64_t paddr = vtophys(addr);
				desc->rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
				desc->rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
				/* buffer has changed, unload and reload map */
				netmap_reload_map(sc->rl_ldata.rl_tx_mtag,
					txd[l].tx_dmamap, addr, na->buff_size);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			slot->flags &= ~NS_REPORT;
			desc->rl_cmdstat = htole32(cmd);
			bus_dmamap_sync(sc->rl_ldata.rl_tx_mtag,
				txd[l].tx_dmamap, BUS_DMASYNC_PREWRITE);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
			n++;
		}
		sc->rl_ldata.rl_tx_prodidx = l;
		kring->nr_hwcur = k;

		/* decrease avail by number of sent packets */
		ring->avail -= n;
		kring->nr_hwavail = ring->avail;

		bus_dmamap_sync(sc->rl_ldata.rl_tx_list_tag,
		    sc->rl_ldata.rl_tx_list_map,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

		/* start ? */
		CSR_WRITE_1(sc, sc->rl_txstart, RL_TXSTART_START);
	}
	if (do_lock)
		RL_UNLOCK(sc);
	return 0;
}


/*
 * Reconcile kernel and user view of the receive ring.
 */
static int
re_netmap_rxsync(void *a, u_int ring_nr, int do_lock)
{
	struct rl_softc *sc = a;
	struct rl_rxdesc *rxd = sc->rl_ldata.rl_rx_desc;
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_kring *kring = &na->rx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, k, l, n, lim = kring->nkr_num_slots - 1;

	k = ring->cur;
	if (k > lim)
		return netmap_ring_reinit(kring);

	if (do_lock)
		RL_LOCK(sc);
	/* XXX check sync modes */
	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * The device uses all the buffers in the ring, so we need
	 * another termination condition in addition to RL_RDESC_STAT_OWN
	 * cleared (all buffers could have it cleared. The easiest one
	 * is to limit the amount of data reported up to 'lim'
	 */
	l = sc->rl_ldata.rl_rx_prodidx; /* next pkt to check */
	j = l + kring->nkr_hwofs;
	for (n = kring->nr_hwavail; n < lim ; n++) {
		struct rl_desc *cur_rx = &sc->rl_ldata.rl_rx_list[l];
		uint32_t rxstat = le32toh(cur_rx->rl_cmdstat);
		uint32_t total_len;

		if ((rxstat & RL_RDESC_STAT_OWN) != 0)
			break;
		total_len = rxstat & sc->rl_rxlenmask;
		/* XXX subtract crc */
		total_len = (total_len < 4) ? 0 : total_len - 4;
		kring->ring->slot[j].len = total_len;
		/*  sync was in re_newbuf() */
		bus_dmamap_sync(sc->rl_ldata.rl_rx_mtag,
		    rxd[l].rx_dmamap, BUS_DMASYNC_POSTREAD);
		j = (j == lim) ? 0 : j + 1;
		l = (l == lim) ? 0 : l + 1;
	}
	if (n != kring->nr_hwavail) {
		sc->rl_ldata.rl_rx_prodidx = l;
		sc->rl_ifp->if_ipackets += n - kring->nr_hwavail;
		kring->nr_hwavail = n;
	}

	/* skip past packets that userspace has already processed,
	 * making them available for reception.
	 * advance nr_hwcur and issue a bus_dmamap_sync on the
	 * buffers so it is safe to write to them.
	 * Also increase nr_hwavail
	 */
	j = kring->nr_hwcur;
	if (j != k) {	/* userspace has read some packets. */
		n = 0;
		l = kring->nr_hwcur - kring->nkr_hwofs;
		if (l < 0)
			l += lim + 1;
		while (j != k) {
			struct netmap_slot *slot = ring->slot + j;
			struct rl_desc *desc = &sc->rl_ldata.rl_rx_list[l];
			int cmd = na->buff_size | RL_RDESC_CMD_OWN;
			void *addr = NMB(slot);

			if (addr == netmap_buffer_base) { /* bad buf */
				if (do_lock)
					RL_UNLOCK(sc);
				return netmap_ring_reinit(kring);
			}

			if (l == lim)	/* mark end of ring */
				cmd |= RL_RDESC_CMD_EOR;

			desc->rl_cmdstat = htole32(cmd);
			slot->flags &= ~NS_REPORT;
			if (slot->flags & NS_BUF_CHANGED) {
				uint64_t paddr = vtophys(addr);
				desc->rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
				desc->rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
				netmap_reload_map(sc->rl_ldata.rl_rx_mtag,
					rxd[l].rx_dmamap, addr, na->buff_size);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			bus_dmamap_sync(sc->rl_ldata.rl_rx_mtag,
				rxd[l].rx_dmamap, BUS_DMASYNC_PREREAD);
			j = (j == lim) ? 0 : j + 1;
			l = (l == lim) ? 0 : l + 1;
			n++;
		}
		kring->nr_hwavail -= n;
		kring->nr_hwcur = k;
		/* Flush the RX DMA ring */

		bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
		    sc->rl_ldata.rl_rx_list_map,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	}
	/* tell userspace that there are new packets */
	ring->avail = kring->nr_hwavail;
	if (do_lock)
		RL_UNLOCK(sc);
	return 0;
}

/*
 * Additional routines to init the tx and rx rings.
 * In other drivers we do that inline in the main code.
 */
static void
re_netmap_tx_init(struct rl_softc *sc)
{   
	struct rl_txdesc *txd;
	struct rl_desc *desc;
	int i, n;
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_slot *slot = netmap_reset(na, NR_TX, 0, 0);

	/* slot is NULL if we are not in netmap mode */
	if (!slot)
		return;
	/* in netmap mode, overwrite addresses and maps */
	txd = sc->rl_ldata.rl_tx_desc;
	desc = sc->rl_ldata.rl_tx_list;
	n = sc->rl_ldata.rl_tx_desc_cnt;

	/* l points in the netmap ring, i points in the NIC ring */
	for (i = 0; i < n; i++) {
		void *addr;
		uint64_t paddr;
		struct netmap_kring *kring = &na->tx_rings[0];
		int l = i + kring->nkr_hwofs;

		if (l >= n)
			l -= n;

		addr = NMB(slot + l);
		paddr = vtophys(addr);
		desc[i].rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
		desc[i].rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
		netmap_load_map(sc->rl_ldata.rl_tx_mtag,
			txd[i].tx_dmamap, addr, na->buff_size);
	}
}

static void
re_netmap_rx_init(struct rl_softc *sc)
{
	struct netmap_adapter *na = NA(sc->rl_ifp);
	struct netmap_slot *slot = netmap_reset(na, NR_RX, 0, 0);
	struct rl_desc *desc = sc->rl_ldata.rl_rx_list;
	uint32_t cmdstat;
	int i, n;

	if (!slot)
		return;
	n = sc->rl_ldata.rl_rx_desc_cnt;
	for (i = 0; i < n; i++) {
		void *addr;
		uint64_t paddr;
		struct netmap_kring *kring = &na->rx_rings[0];
		int l = i + kring->nkr_hwofs;

		if (l >= n)
			l -= n;

		addr = NMB(slot + l);
		paddr = vtophys(addr);
		desc[i].rl_bufaddr_lo = htole32(RL_ADDR_LO(paddr));
		desc[i].rl_bufaddr_hi = htole32(RL_ADDR_HI(paddr));
		cmdstat = na->buff_size;
		if (i == n - 1)
			cmdstat |= RL_RDESC_CMD_EOR;
		/*
		 * userspace knows that hwavail packets were ready before the
		 * reset, so we need to tell the NIC that last hwavail
		 * descriptors of the ring are still owned by the driver.
		 */
		if (i < n - 1 - kring->nr_hwavail) // XXX + 1 ?
			cmdstat |= RL_RDESC_CMD_OWN;
		desc[i].rl_cmdstat = htole32(cmdstat);

		netmap_reload_map(sc->rl_ldata.rl_rx_mtag,
			sc->rl_ldata.rl_rx_desc[i].rx_dmamap,
			addr, na->buff_size);
	}
}
