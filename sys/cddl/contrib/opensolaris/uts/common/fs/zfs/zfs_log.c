/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/zfs_znode.h>
#include <sys/zfs_dir.h>
#include <sys/zil.h>
#include <sys/zil_impl.h>
#include <sys/byteorder.h>
#include <sys/policy.h>
#include <sys/stat.h>
#include <sys/acl.h>
#include <sys/dmu.h>
#include <sys/spa.h>
#include <sys/zfs_fuid.h>

/*
 * All the functions in this file are used to construct the log entries
 * to record transactions. They allocate * an intent log transaction
 * structure (itx_t) and save within it all the information necessary to
 * possibly replay the transaction. The itx is then assigned a sequence
 * number and inserted in the in-memory list anchored in the zilog.
 */

int
zfs_log_create_txtype(zil_create_t type, vsecattr_t *vsecp, vattr_t *vap)
{
	int isxvattr = (vap->va_mask & AT_XVATTR);
	switch (type) {
	case Z_FILE:
		if (vsecp == NULL && !isxvattr)
			return (TX_CREATE);
		if (vsecp && isxvattr)
			return (TX_CREATE_ACL_ATTR);
		if (vsecp)
			return (TX_CREATE_ACL);
		else
			return (TX_CREATE_ATTR);
		/*NOTREACHED*/
	case Z_DIR:
		if (vsecp == NULL && !isxvattr)
			return (TX_MKDIR);
		if (vsecp && isxvattr)
			return (TX_MKDIR_ACL_ATTR);
		if (vsecp)
			return (TX_MKDIR_ACL);
		else
			return (TX_MKDIR_ATTR);
	case Z_XATTRDIR:
		return (TX_MKXATTR);
	}
	ASSERT(0);
	return (TX_MAX_TYPE);
}

/*
 * build up the log data necessary for logging xvattr_t
 * First lr_attr_t is initialized.  following the lr_attr_t
 * is the mapsize and attribute bitmap copied from the xvattr_t.
 * Following the bitmap and bitmapsize two 64 bit words are reserved
 * for the create time which may be set.  Following the create time
 * records a single 64 bit integer which has the bits to set on
 * replay for the xvattr.
 */
static void
zfs_log_xvattr(lr_attr_t *lrattr, xvattr_t *xvap)
{
	uint32_t	*bitmap;
	uint64_t	*attrs;
	uint64_t	*crtime;
	xoptattr_t	*xoap;
	void		*scanstamp;
	int		i;

	xoap = xva_getxoptattr(xvap);
	ASSERT(xoap);

	lrattr->lr_attr_masksize = xvap->xva_mapsize;
	bitmap = &lrattr->lr_attr_bitmap;
	for (i = 0; i != xvap->xva_mapsize; i++, bitmap++) {
		*bitmap = xvap->xva_reqattrmap[i];
	}

	/* Now pack the attributes up in a single uint64_t */
	attrs = (uint64_t *)bitmap;
	crtime = attrs + 1;
	scanstamp = (caddr_t)(crtime + 2);
	*attrs = 0;
	if (XVA_ISSET_REQ(xvap, XAT_READONLY))
		*attrs |= (xoap->xoa_readonly == 0) ? 0 :
		    XAT0_READONLY;
	if (XVA_ISSET_REQ(xvap, XAT_HIDDEN))
		*attrs |= (xoap->xoa_hidden == 0) ? 0 :
		    XAT0_HIDDEN;
	if (XVA_ISSET_REQ(xvap, XAT_SYSTEM))
		*attrs |= (xoap->xoa_system == 0) ? 0 :
		    XAT0_SYSTEM;
	if (XVA_ISSET_REQ(xvap, XAT_ARCHIVE))
		*attrs |= (xoap->xoa_archive == 0) ? 0 :
		    XAT0_ARCHIVE;
	if (XVA_ISSET_REQ(xvap, XAT_IMMUTABLE))
		*attrs |= (xoap->xoa_immutable == 0) ? 0 :
		    XAT0_IMMUTABLE;
	if (XVA_ISSET_REQ(xvap, XAT_NOUNLINK))
		*attrs |= (xoap->xoa_nounlink == 0) ? 0 :
		    XAT0_NOUNLINK;
	if (XVA_ISSET_REQ(xvap, XAT_APPENDONLY))
		*attrs |= (xoap->xoa_appendonly == 0) ? 0 :
		    XAT0_APPENDONLY;
	if (XVA_ISSET_REQ(xvap, XAT_OPAQUE))
		*attrs |= (xoap->xoa_opaque == 0) ? 0 :
		    XAT0_APPENDONLY;
	if (XVA_ISSET_REQ(xvap, XAT_NODUMP))
		*attrs |= (xoap->xoa_nodump == 0) ? 0 :
		    XAT0_NODUMP;
	if (XVA_ISSET_REQ(xvap, XAT_AV_QUARANTINED))
		*attrs |= (xoap->xoa_av_quarantined == 0) ? 0 :
		    XAT0_AV_QUARANTINED;
	if (XVA_ISSET_REQ(xvap, XAT_AV_MODIFIED))
		*attrs |= (xoap->xoa_av_modified == 0) ? 0 :
		    XAT0_AV_MODIFIED;
	if (XVA_ISSET_REQ(xvap, XAT_CREATETIME))
		ZFS_TIME_ENCODE(&xoap->xoa_createtime, crtime);
	if (XVA_ISSET_REQ(xvap, XAT_AV_SCANSTAMP))
		bcopy(xoap->xoa_av_scanstamp, scanstamp, AV_SCANSTAMP_SZ);
}

static void *
zfs_log_fuid_ids(zfs_fuid_info_t *fuidp, void *start)
{
	zfs_fuid_t *zfuid;
	uint64_t *fuidloc = start;

	/* First copy in the ACE FUIDs */
	for (zfuid = list_head(&fuidp->z_fuids); zfuid;
	    zfuid = list_next(&fuidp->z_fuids, zfuid)) {
		*fuidloc++ = zfuid->z_logfuid;
	}
	return (fuidloc);
}


static void *
zfs_log_fuid_domains(zfs_fuid_info_t *fuidp, void *start)
{
	zfs_fuid_domain_t *zdomain;

	/* now copy in the domain info, if any */
	if (fuidp->z_domain_str_sz != 0) {
		for (zdomain = list_head(&fuidp->z_domains); zdomain;
		    zdomain = list_next(&fuidp->z_domains, zdomain)) {
			bcopy((void *)zdomain->z_domain, start,
			    strlen(zdomain->z_domain) + 1);
			start = (caddr_t)start +
			    strlen(zdomain->z_domain) + 1;
		}
	}
	return (start);
}

/*
 * zfs_log_create() is used to handle TX_CREATE, TX_CREATE_ATTR, TX_MKDIR,
 * TX_MKDIR_ATTR and TX_MKXATTR
 * transactions.
 *
 * TX_CREATE and TX_MKDIR are standard creates, but they may have FUID
 * domain information appended prior to the name.  In this case the
 * uid/gid in the log record will be a log centric FUID.
 *
 * TX_CREATE_ACL_ATTR and TX_MKDIR_ACL_ATTR handle special creates that
 * may contain attributes, ACL and optional fuid information.
 *
 * TX_CREATE_ACL and TX_MKDIR_ACL handle special creates that specify
 * and ACL and normal users/groups in the ACEs.
 *
 * There may be an optional xvattr attribute information similar
 * to zfs_log_setattr.
 *
 * Also, after the file name "domain" strings may be appended.
 */
void
zfs_log_create(zilog_t *zilog, dmu_tx_t *tx, uint64_t txtype,
    znode_t *dzp, znode_t *zp, char *name, vsecattr_t *vsecp,
    zfs_fuid_info_t *fuidp, vattr_t *vap)
{
	itx_t *itx;
	uint64_t seq;
	lr_create_t *lr;
	lr_acl_create_t *lracl;
	size_t aclsize;
	size_t xvatsize = 0;
	size_t txsize;
	xvattr_t *xvap = (xvattr_t *)vap;
	void *end;
	size_t lrsize;
	size_t namesize = strlen(name) + 1;
	size_t fuidsz = 0;

	if (zilog == NULL)
		return;

	/*
	 * If we have FUIDs present then add in space for
	 * domains and ACE fuid's if any.
	 */
	if (fuidp) {
		fuidsz += fuidp->z_domain_str_sz;
		fuidsz += fuidp->z_fuid_cnt * sizeof (uint64_t);
	}

	if (vap->va_mask & AT_XVATTR)
		xvatsize = ZIL_XVAT_SIZE(xvap->xva_mapsize);

	if ((int)txtype == TX_CREATE_ATTR || (int)txtype == TX_MKDIR_ATTR ||
	    (int)txtype == TX_CREATE || (int)txtype == TX_MKDIR ||
	    (int)txtype == TX_MKXATTR) {
		txsize = sizeof (*lr) + namesize + fuidsz + xvatsize;
		lrsize = sizeof (*lr);
	} else {
		aclsize = (vsecp) ? vsecp->vsa_aclentsz : 0;
		txsize =
		    sizeof (lr_acl_create_t) + namesize + fuidsz +
		    ZIL_ACE_LENGTH(aclsize) + xvatsize;
		lrsize = sizeof (lr_acl_create_t);
	}

	itx = zil_itx_create(txtype, txsize);

	lr = (lr_create_t *)&itx->itx_lr;
	lr->lr_doid = dzp->z_id;
	lr->lr_foid = zp->z_id;
	lr->lr_mode = zp->z_phys->zp_mode;
	if (!IS_EPHEMERAL(zp->z_phys->zp_uid)) {
		lr->lr_uid = (uint64_t)zp->z_phys->zp_uid;
	} else {
		lr->lr_uid = fuidp->z_fuid_owner;
	}
	if (!IS_EPHEMERAL(zp->z_phys->zp_gid)) {
		lr->lr_gid = (uint64_t)zp->z_phys->zp_gid;
	} else {
		lr->lr_gid = fuidp->z_fuid_group;
	}
	lr->lr_gen = zp->z_phys->zp_gen;
	lr->lr_crtime[0] = zp->z_phys->zp_crtime[0];
	lr->lr_crtime[1] = zp->z_phys->zp_crtime[1];
	lr->lr_rdev = zp->z_phys->zp_rdev;

	/*
	 * Fill in xvattr info if any
	 */
	if (vap->va_mask & AT_XVATTR) {
		zfs_log_xvattr((lr_attr_t *)((caddr_t)lr + lrsize), xvap);
		end = (caddr_t)lr + lrsize + xvatsize;
	} else {
		end = (caddr_t)lr + lrsize;
	}

	/* Now fill in any ACL info */

	if (vsecp) {
		lracl = (lr_acl_create_t *)&itx->itx_lr;
		lracl->lr_aclcnt = vsecp->vsa_aclcnt;
		lracl->lr_acl_bytes = aclsize;
		lracl->lr_domcnt = fuidp ? fuidp->z_domain_cnt : 0;
		lracl->lr_fuidcnt  = fuidp ? fuidp->z_fuid_cnt : 0;
		if (vsecp->vsa_aclflags & VSA_ACE_ACLFLAGS)
			lracl->lr_acl_flags = (uint64_t)vsecp->vsa_aclflags;
		else
			lracl->lr_acl_flags = 0;

		bcopy(vsecp->vsa_aclentp, end, aclsize);
		end = (caddr_t)end + ZIL_ACE_LENGTH(aclsize);
	}

	/* drop in FUID info */
	if (fuidp) {
		end = zfs_log_fuid_ids(fuidp, end);
		end = zfs_log_fuid_domains(fuidp, end);
	}
	/*
	 * Now place file name in log record
	 */
	bcopy(name, end, namesize);

	seq = zil_itx_assign(zilog, itx, tx);
	dzp->z_last_itx = seq;
	zp->z_last_itx = seq;
}

/*
 * zfs_log_remove() handles both TX_REMOVE and TX_RMDIR transactions.
 */
void
zfs_log_remove(zilog_t *zilog, dmu_tx_t *tx, uint64_t txtype,
	znode_t *dzp, char *name)
{
	itx_t *itx;
	uint64_t seq;
	lr_remove_t *lr;
	size_t namesize = strlen(name) + 1;

	if (zilog == NULL)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr) + namesize);
	lr = (lr_remove_t *)&itx->itx_lr;
	lr->lr_doid = dzp->z_id;
	bcopy(name, (char *)(lr + 1), namesize);

	seq = zil_itx_assign(zilog, itx, tx);
	dzp->z_last_itx = seq;
}

/*
 * zfs_log_link() handles TX_LINK transactions.
 */
void
zfs_log_link(zilog_t *zilog, dmu_tx_t *tx, uint64_t txtype,
	znode_t *dzp, znode_t *zp, char *name)
{
	itx_t *itx;
	uint64_t seq;
	lr_link_t *lr;
	size_t namesize = strlen(name) + 1;

	if (zilog == NULL)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr) + namesize);
	lr = (lr_link_t *)&itx->itx_lr;
	lr->lr_doid = dzp->z_id;
	lr->lr_link_obj = zp->z_id;
	bcopy(name, (char *)(lr + 1), namesize);

	seq = zil_itx_assign(zilog, itx, tx);
	dzp->z_last_itx = seq;
	zp->z_last_itx = seq;
}

/*
 * zfs_log_symlink() handles TX_SYMLINK transactions.
 */
void
zfs_log_symlink(zilog_t *zilog, dmu_tx_t *tx, uint64_t txtype,
    znode_t *dzp, znode_t *zp, char *name, char *link)
{
	itx_t *itx;
	uint64_t seq;
	lr_create_t *lr;
	size_t namesize = strlen(name) + 1;
	size_t linksize = strlen(link) + 1;

	if (zilog == NULL)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr) + namesize + linksize);
	lr = (lr_create_t *)&itx->itx_lr;
	lr->lr_doid = dzp->z_id;
	lr->lr_foid = zp->z_id;
	lr->lr_mode = zp->z_phys->zp_mode;
	lr->lr_uid = zp->z_phys->zp_uid;
	lr->lr_gid = zp->z_phys->zp_gid;
	lr->lr_gen = zp->z_phys->zp_gen;
	lr->lr_crtime[0] = zp->z_phys->zp_crtime[0];
	lr->lr_crtime[1] = zp->z_phys->zp_crtime[1];
	bcopy(name, (char *)(lr + 1), namesize);
	bcopy(link, (char *)(lr + 1) + namesize, linksize);

	seq = zil_itx_assign(zilog, itx, tx);
	dzp->z_last_itx = seq;
	zp->z_last_itx = seq;
}

/*
 * zfs_log_rename() handles TX_RENAME transactions.
 */
void
zfs_log_rename(zilog_t *zilog, dmu_tx_t *tx, uint64_t txtype,
	znode_t *sdzp, char *sname, znode_t *tdzp, char *dname, znode_t *szp)
{
	itx_t *itx;
	uint64_t seq;
	lr_rename_t *lr;
	size_t snamesize = strlen(sname) + 1;
	size_t dnamesize = strlen(dname) + 1;

	if (zilog == NULL)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr) + snamesize + dnamesize);
	lr = (lr_rename_t *)&itx->itx_lr;
	lr->lr_sdoid = sdzp->z_id;
	lr->lr_tdoid = tdzp->z_id;
	bcopy(sname, (char *)(lr + 1), snamesize);
	bcopy(dname, (char *)(lr + 1) + snamesize, dnamesize);

	seq = zil_itx_assign(zilog, itx, tx);
	sdzp->z_last_itx = seq;
	tdzp->z_last_itx = seq;
	szp->z_last_itx = seq;
}

/*
 * zfs_log_write() handles TX_WRITE transactions.
 */
ssize_t zfs_immediate_write_sz = 32768;

#define	ZIL_MAX_LOG_DATA (SPA_MAXBLOCKSIZE - sizeof (zil_trailer_t) - \
    sizeof (lr_write_t))

void
zfs_log_write(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *zp, offset_t off, ssize_t resid, int ioflag)
{
	itx_wr_state_t write_state;
	boolean_t slogging;
	uintptr_t fsync_cnt;

	if (zilog == NULL || zp->z_unlinked)
		return;

	/*
	 * Writes are handled in three different ways:
	 *
	 * WR_INDIRECT:
	 *    In this mode, if we need to commit the write later, then the block
	 *    is immediately written into the file system (using dmu_sync),
	 *    and a pointer to the block is put into the log record.
	 *    When the txg commits the block is linked in.
	 *    This saves additionally writing the data into the log record.
	 *    There are a few requirements for this to occur:
	 *	- write is greater than zfs_immediate_write_sz
	 *	- not using slogs (as slogs are assumed to always be faster
	 *	  than writing into the main pool)
	 *	- the write occupies only one block
	 * WR_COPIED:
	 *    If we know we'll immediately be committing the
	 *    transaction (FSYNC or FDSYNC), the we allocate a larger
	 *    log record here for the data and copy the data in.
	 * WR_NEED_COPY:
	 *    Otherwise we don't allocate a buffer, and *if* we need to
	 *    flush the write later then a buffer is allocated and
	 *    we retrieve the data using the dmu.
	 */
	slogging = spa_has_slogs(zilog->zl_spa);
	if (resid > zfs_immediate_write_sz && !slogging && resid <= zp->z_blksz)
		write_state = WR_INDIRECT;
	else if (ioflag & (FSYNC | FDSYNC))
		write_state = WR_COPIED;
	else
		write_state = WR_NEED_COPY;

	if ((fsync_cnt = (uintptr_t)tsd_get(zfs_fsyncer_key)) != 0) {
		(void) tsd_set(zfs_fsyncer_key, (void *)(fsync_cnt - 1));
	}

	while (resid) {
		itx_t *itx;
		lr_write_t *lr;
		ssize_t len;

		/*
		 * If the write would overflow the largest block then split it.
		 */
		if (write_state != WR_INDIRECT && resid > ZIL_MAX_LOG_DATA)
			len = SPA_MAXBLOCKSIZE >> 1;
		else
			len = resid;

		itx = zil_itx_create(txtype, sizeof (*lr) +
		    (write_state == WR_COPIED ? len : 0));
		lr = (lr_write_t *)&itx->itx_lr;
		if (write_state == WR_COPIED && dmu_read(zp->z_zfsvfs->z_os,
		    zp->z_id, off, len, lr + 1) != 0) {
			kmem_free(itx, offsetof(itx_t, itx_lr) +
			    itx->itx_lr.lrc_reclen);
			itx = zil_itx_create(txtype, sizeof (*lr));
			lr = (lr_write_t *)&itx->itx_lr;
			write_state = WR_NEED_COPY;
		}

		itx->itx_wr_state = write_state;
		if (write_state == WR_NEED_COPY)
			itx->itx_sod += len;
		lr->lr_foid = zp->z_id;
		lr->lr_offset = off;
		lr->lr_length = len;
		lr->lr_blkoff = 0;
		BP_ZERO(&lr->lr_blkptr);

		itx->itx_private = zp->z_zfsvfs;

		if ((zp->z_sync_cnt != 0) || (fsync_cnt != 0) ||
		    (ioflag & (FSYNC | FDSYNC)))
			itx->itx_sync = B_TRUE;
		else
			itx->itx_sync = B_FALSE;

		zp->z_last_itx = zil_itx_assign(zilog, itx, tx);

		off += len;
		resid -= len;
	}
}

/*
 * zfs_log_truncate() handles TX_TRUNCATE transactions.
 */
void
zfs_log_truncate(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *zp, uint64_t off, uint64_t len)
{
	itx_t *itx;
	uint64_t seq;
	lr_truncate_t *lr;

	if (zilog == NULL || zp->z_unlinked)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr));
	lr = (lr_truncate_t *)&itx->itx_lr;
	lr->lr_foid = zp->z_id;
	lr->lr_offset = off;
	lr->lr_length = len;

	itx->itx_sync = (zp->z_sync_cnt != 0);
	seq = zil_itx_assign(zilog, itx, tx);
	zp->z_last_itx = seq;
}

/*
 * zfs_log_setattr() handles TX_SETATTR transactions.
 */
void
zfs_log_setattr(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *zp, vattr_t *vap, uint_t mask_applied, zfs_fuid_info_t *fuidp)
{
	itx_t		*itx;
	uint64_t	seq;
	lr_setattr_t	*lr;
	xvattr_t	*xvap = (xvattr_t *)vap;
	size_t		recsize = sizeof (lr_setattr_t);
	void		*start;


	if (zilog == NULL || zp->z_unlinked)
		return;

	/*
	 * If XVATTR set, then log record size needs to allow
	 * for lr_attr_t + xvattr mask, mapsize and create time
	 * plus actual attribute values
	 */
	if (vap->va_mask & AT_XVATTR)
		recsize = sizeof (*lr) + ZIL_XVAT_SIZE(xvap->xva_mapsize);

	if (fuidp)
		recsize += fuidp->z_domain_str_sz;

	itx = zil_itx_create(txtype, recsize);
	lr = (lr_setattr_t *)&itx->itx_lr;
	lr->lr_foid = zp->z_id;
	lr->lr_mask = (uint64_t)mask_applied;
	lr->lr_mode = (uint64_t)vap->va_mode;
	if ((mask_applied & AT_UID) && IS_EPHEMERAL(vap->va_uid))
		lr->lr_uid = fuidp->z_fuid_owner;
	else
		lr->lr_uid = (uint64_t)vap->va_uid;

	if ((mask_applied & AT_GID) && IS_EPHEMERAL(vap->va_gid))
		lr->lr_gid = fuidp->z_fuid_group;
	else
		lr->lr_gid = (uint64_t)vap->va_gid;

	lr->lr_size = (uint64_t)vap->va_size;
	ZFS_TIME_ENCODE(&vap->va_atime, lr->lr_atime);
	ZFS_TIME_ENCODE(&vap->va_mtime, lr->lr_mtime);
	start = (lr_setattr_t *)(lr + 1);
	if (vap->va_mask & AT_XVATTR) {
		zfs_log_xvattr((lr_attr_t *)start, xvap);
		start = (caddr_t)start + ZIL_XVAT_SIZE(xvap->xva_mapsize);
	}

	/*
	 * Now stick on domain information if any on end
	 */

	if (fuidp)
		(void) zfs_log_fuid_domains(fuidp, start);

	itx->itx_sync = (zp->z_sync_cnt != 0);
	seq = zil_itx_assign(zilog, itx, tx);
	zp->z_last_itx = seq;
}

/*
 * zfs_log_acl() handles TX_ACL transactions.
 */
void
zfs_log_acl(zilog_t *zilog, dmu_tx_t *tx, znode_t *zp,
    vsecattr_t *vsecp, zfs_fuid_info_t *fuidp)
{
	itx_t *itx;
	uint64_t seq;
	lr_acl_v0_t *lrv0;
	lr_acl_t *lr;
	int txtype;
	int lrsize;
	size_t txsize;
	size_t aclbytes = vsecp->vsa_aclentsz;

	if (zilog == NULL || zp->z_unlinked)
		return;

	txtype = (zp->z_zfsvfs->z_version < ZPL_VERSION_FUID) ?
	    TX_ACL_V0 : TX_ACL;

	if (txtype == TX_ACL)
		lrsize = sizeof (*lr);
	else
		lrsize = sizeof (*lrv0);

	txsize = lrsize +
	    ((txtype == TX_ACL) ? ZIL_ACE_LENGTH(aclbytes) : aclbytes) +
	    (fuidp ? fuidp->z_domain_str_sz : 0) +
	    sizeof (uint64_t) * (fuidp ? fuidp->z_fuid_cnt : 0);

	itx = zil_itx_create(txtype, txsize);

	lr = (lr_acl_t *)&itx->itx_lr;
	lr->lr_foid = zp->z_id;
	if (txtype == TX_ACL) {
		lr->lr_acl_bytes = aclbytes;
		lr->lr_domcnt = fuidp ? fuidp->z_domain_cnt : 0;
		lr->lr_fuidcnt = fuidp ? fuidp->z_fuid_cnt : 0;
		if (vsecp->vsa_mask & VSA_ACE_ACLFLAGS)
			lr->lr_acl_flags = (uint64_t)vsecp->vsa_aclflags;
		else
			lr->lr_acl_flags = 0;
	}
	lr->lr_aclcnt = (uint64_t)vsecp->vsa_aclcnt;

	if (txtype == TX_ACL_V0) {
		lrv0 = (lr_acl_v0_t *)lr;
		bcopy(vsecp->vsa_aclentp, (ace_t *)(lrv0 + 1), aclbytes);
	} else {
		void *start = (ace_t *)(lr + 1);

		bcopy(vsecp->vsa_aclentp, start, aclbytes);

		start = (caddr_t)start + ZIL_ACE_LENGTH(aclbytes);

		if (fuidp) {
			start = zfs_log_fuid_ids(fuidp, start);
			(void) zfs_log_fuid_domains(fuidp, start);
		}
	}

	itx->itx_sync = (zp->z_sync_cnt != 0);
	seq = zil_itx_assign(zilog, itx, tx);
	zp->z_last_itx = seq;
}
