/*
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)nqnfs.h	7.4 (Berkeley) %G%
 */

/*
 * Definitions for NQNFS (Not Quite NFS) cache consistency protocol.
 */

/* Tunable constants */
#define	NQ_CLOCKSKEW	3	/* Clock skew factor (sec) */
#define	NQ_WRITESLACK	5	/* Delay for write cache flushing */
#define	NQ_MAXLEASE	30	/* Max lease duration (sec) */
#define	NQ_MINLEASE	2	/* Min lease duration (sec) */
#define	NQ_DEFLEASE	10	/* Default lease duration (sec) */
#define	NQ_RENEWAL	3	/* Time before expiry (sec) to renew */
#define	NQ_TRYLATERDEL	15	/* Initial try later delay (sec) */
#define	NQ_MAXNUMLEASE	2048	/* Upper bound on number of server leases */
#define	NQ_DEADTHRESH	5	/* Default nm_deadthresh */
#define	NQ_NEVERDEAD	9	/* Greater than max. nm_timeouts */
#define	NQLCHSZ		256	/* Server hash table size */

#define	NQNFS_PROG	300105	/* As assigned by Sun */
#define	NQNFS_VER1	1
#define	NQNFS_EVICTSIZ	156	/* Size of eviction request in bytes */

/*
 * Definitions used for saving the "last lease expires" time in Non-volatile
 * RAM on the server. The default definitions below assume that NOVRAM is not
 * available.
 */
#define	NQSTORENOVRAM(t)
#define	NQLOADNOVRAM(t)

/*
 * Defn and structs used on the server to maintain state for current leases.
 * The list of host(s) that hold the lease are kept as nqhost structures.
 * The first one lives in nqlease and any others are held in a linked
 * list of nqm structures hanging off of nqlease.
 *
 * Each nqlease structure is chained into two lists. The first is a list
 * ordered by increasing expiry time for nqsrv_timer() and the second is a chain
 * hashed on lc_fh.
 */
#define	LC_MOREHOSTSIZ	10

struct nqhost {
	union {
		struct {
			u_short udp_flag;
			u_short	udp_port;
			union nethostaddr udp_haddr;
		} un_udp;
		struct {
			u_short connless_flag;
			u_short connless_spare;
			union nethostaddr connless_haddr;
		} un_connless;
		struct {
			u_short conn_flag;
			u_short conn_spare;
			struct nfssvc_sock *conn_slp;
		} un_conn;
	} lph_un;
};
#define	lph_flag	lph_un.un_udp.udp_flag
#define	lph_port	lph_un.un_udp.udp_port
#define	lph_haddr	lph_un.un_udp.udp_haddr
#define	lph_inetaddr	lph_un.un_udp.udp_haddr.had_inetaddr
#define	lph_claddr	lph_un.un_connless.connless_haddr
#define	lph_nam		lph_un.un_connless.connless_haddr.had_nam
#define	lph_slp		lph_un.un_conn.conn_slp

struct nqlease {
	struct nqlease *lc_chain1[2];	/* Timer queue list (must be first) */
	struct nqlease *lc_fhnext;	/* Fhandle hash list */
	struct nqlease **lc_fhprev;
	time_t		lc_expiry;	/* Expiry time (sec) */
	struct nqhost	lc_host;	/* Host that got lease */
	struct nqm	*lc_morehosts;	/* Other hosts that share read lease */
	fsid_t		lc_fsid;	/* Fhandle */
	char		lc_fiddata[MAXFIDSZ];
	struct vnode	*lc_vp;		/* Soft reference to associated vnode */
};
#define	lc_flag		lc_host.lph_un.un_udp.udp_flag

/* lc_flag bits */
#define	LC_VALID	0x0001	/* Host address valid */
#define	LC_WRITE	0x0002	/* Write cache */
#define	LC_NONCACHABLE	0x0004	/* Non-cachable lease */
#define	LC_LOCKED	0x0008	/* Locked */
#define	LC_WANTED	0x0010	/* Lock wanted */
#define	LC_EXPIREDWANTED 0x0020	/* Want lease when expired */
#define	LC_UDP		0x0040	/* Host address for udp socket */
#define	LC_CLTP		0x0080	/* Host address for other connectionless */
#define	LC_LOCAL	0x0100	/* Host is server */
#define	LC_VACATED	0x0200	/* Host has vacated lease */
#define	LC_WRITTEN	0x0400	/* Recently wrote to the leased file */
#define	LC_SREF		0x0800	/* Holds a nfssvc_sock reference */

struct nqm {
	struct nqm	*lpm_next;
	struct nqhost	lpm_hosts[LC_MOREHOSTSIZ];
};

/*
 * Flag bits for flags argument to nqsrv_getlease.
 */
#define	NQL_READ	LEASE_READ	/* Read Request */
#define	NQL_WRITE	LEASE_WRITE	/* Write Request */
#define	NQL_CHECK	0x4		/* Check for lease */
#define	NQL_NOVAL	0xffffffff	/* Invalid */

/*
 * Special value for slp for local server calls.
 */
#define	NQLOCALSLP	((struct nfssvc_sock *) -1)

/*
 * Server side macros.
 */
#define	nqsrv_getl(v, l) \
		(void) nqsrv_getlease((v), &nfsd->nd_duration, \
		 ((nfsd->nd_nqlflag != 0 && nfsd->nd_nqlflag != NQL_NOVAL) ? nfsd->nd_nqlflag : \
		 ((l) | NQL_CHECK)), \
		 nfsd, nam, &cache, &frev, cred)

/*
 * Client side macros that check for a valid lease.
 */
#define	NQNFS_CKINVALID(v, n, f) \
 ((time.tv_sec > (n)->n_expiry && \
 VFSTONFS((v)->v_mount)->nm_timeouts < VFSTONFS((v)->v_mount)->nm_deadthresh) \
  || ((f) == NQL_WRITE && ((n)->n_flag & NQNFSWRITE) == 0))

#define	NQNFS_CKCACHABLE(v, f) \
 ((time.tv_sec <= VTONFS(v)->n_expiry || \
  VFSTONFS((v)->v_mount)->nm_timeouts >= VFSTONFS((v)->v_mount)->nm_deadthresh) \
   && (VTONFS(v)->n_flag & NQNFSNONCACHE) == 0 && \
   ((f) == NQL_READ || (VTONFS(v)->n_flag & NQNFSWRITE)))

#define	NQNFS_NEEDLEASE(v, p) \
		(time.tv_sec > VTONFS(v)->n_expiry ? \
		 ((VTONFS(v)->n_flag & NQNFSEVICTED) ? 0 : nqnfs_piggy[p]) : \
		 (((time.tv_sec + NQ_RENEWAL) > VTONFS(v)->n_expiry && \
		   nqnfs_piggy[p]) ? \
		   ((VTONFS(v)->n_flag & NQNFSWRITE) ? \
		    NQL_WRITE : nqnfs_piggy[p]) : 0))

/*
 * List head for timer queue.
 */
extern union nqsrvthead {
	union	nqsrvthead *th_head[2];
	struct	nqlease *th_chain[2];
} nqthead;
extern struct nqlease **nqfhead;
extern u_long nqfheadhash;

/*
 * Nqnfs return status numbers.
 */
#define	NQNFS_EXPIRED	500
#define	NQNFS_TRYLATER	501
#define NQNFS_AUTHERR	502
