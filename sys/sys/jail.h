/*-
 * Copyright (c) 1999 Poul-Henning Kamp.
 * Copyright (c) 2009 James Gritton.
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef _SYS_JAIL_H_
#define _SYS_JAIL_H_

#ifdef _KERNEL
struct jail_v0 {
	u_int32_t	version;
	char		*path;
	char		*hostname;
	u_int32_t	ip_number;
};
#endif

struct jail {
	uint32_t	version;
	char		*path;
	char		*hostname;
	char		*jailname;
	uint32_t	ip4s;
	uint32_t	ip6s;
	struct in_addr	*ip4;
	struct in6_addr	*ip6;
};
#define	JAIL_API_VERSION	2

/*
 * For all xprison structs, always keep the pr_version an int and
 * the first variable so userspace can easily distinguish them.
 */
#ifndef _KERNEL
struct xprison_v1 {
	int		 pr_version;
	int		 pr_id;
	char		 pr_path[MAXPATHLEN];
	char		 pr_host[MAXHOSTNAMELEN];
	u_int32_t	 pr_ip;
};
#endif

struct xprison {
	int		 pr_version;
	int		 pr_id;
	int		 pr_state;
	cpusetid_t	 pr_cpusetid;
	char		 pr_path[MAXPATHLEN];
	char		 pr_host[MAXHOSTNAMELEN];
	char		 pr_name[MAXHOSTNAMELEN];
	uint32_t	 pr_ip4s;
	uint32_t	 pr_ip6s;
#if 0
	/*
	 * sizeof(xprison) will be malloced + size needed for all
	 * IPv4 and IPv6 addesses. Offsets are based numbers of addresses.
	 */
	struct in_addr	 pr_ip4[];
	struct in6_addr	 pr_ip6[];
#endif
};
#define	XPRISON_VERSION		3

#define	PRISON_STATE_INVALID	0
#define	PRISON_STATE_ALIVE	1
#define	PRISON_STATE_DYING	2

/*
 * Flags for jail_set and jail_get.
 */
#define	JAIL_CREATE	0x01	/* Create jail if it doesn't exist */
#define	JAIL_UPDATE	0x02	/* Update parameters of existing jail */
#define	JAIL_ATTACH	0x04	/* Attach to jail upon creation */
#define	JAIL_DYING	0x08	/* Allow getting a dying jail */
#define	JAIL_SET_MASK	0x0f
#define	JAIL_GET_MASK	0x08

#ifndef _KERNEL

struct iovec;

int jail(struct jail *);
int jail_set(struct iovec *, unsigned int, int);
int jail_get(struct iovec *, unsigned int, int);
int jail_attach(int);
int jail_remove(int);

#else /* _KERNEL */

#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/_task.h>

#define JAIL_MAX	999999

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_PRISON);
#endif
#endif /* _KERNEL */

#if defined(_KERNEL) || defined(_WANT_PRISON)

#include <sys/osd.h>

#define	HOSTUUIDLEN	64

/*
 * This structure describes a prison.  It is pointed to by all struct
 * ucreds's of the inmates.  pr_ref keeps track of them and is used to
 * delete the struture when the last inmate is dead.
 *
 * Lock key:
 *   (a) allprison_lock
 *   (p) locked by pr_mtx
 *   (c) set only during creation before the structure is shared, no mutex
 *       required to read
 *   (d) set only during destruction of jail, no mutex needed
 */
struct prison {
	TAILQ_ENTRY(prison) pr_list;			/* (a) all prisons */
	int		 pr_id;				/* (c) prison id */
	int		 pr_ref;			/* (p) refcount */
	int		 pr_uref;			/* (p) user (alive) refcount */
	unsigned	 pr_flags;			/* (p) PR_* flags */
	char		 pr_path[MAXPATHLEN];		/* (c) chroot path */
	struct cpuset	*pr_cpuset;			/* (p) cpuset */
	struct vnode	*pr_root;			/* (c) vnode to rdir */
	char		 pr_hostname[MAXHOSTNAMELEN];	/* (p) jail hostname */
	char		 pr_name[MAXHOSTNAMELEN];	/* (p) admin jail name */
	struct prison	*pr_parent;			/* (c) containing jail */
	int		 pr_securelevel;		/* (p) securelevel */
	struct task	 pr_task;			/* (d) destroy task */
	struct mtx	 pr_mtx;
	struct osd	 pr_osd;			/* (p) additional data */
	int		 pr_ip4s;			/* (p) number of v4 IPs */
	struct in_addr	*pr_ip4;			/* (p) v4 IPs of jail */
	int		 pr_ip6s;			/* (p) number of v6 IPs */
	struct in6_addr	*pr_ip6;			/* (p) v6 IPs of jail */
	LIST_HEAD(, prison) pr_children;		/* (a) list of child jails */
	LIST_ENTRY(prison) pr_sibling;			/* (a) next in parent's list */
	int		 pr_childcount;			/* (a) number of child jails */
	unsigned	 pr_allow;			/* (p) PR_ALLOW_* flags */
	int		 pr_enforce_statfs;		/* (p) statfs permission */
	char		 pr_domainname[MAXHOSTNAMELEN];	/* (p) jail domainname */
	char		 pr_hostuuid[HOSTUUIDLEN];	/* (p) jail hostuuid */
	unsigned long	 pr_hostid;			/* (p) jail hostid */
	struct vnet	*pr_vnet;			/* (c) network stack */
	int		 pr_childmax;			/* (p) maximum child jails */
};
#endif /* _KERNEL || _WANT_PRISON */

#ifdef _KERNEL
/* Flag bits set via options */
#define	PR_PERSIST	0x00000001	/* Can exist without processes */
#define	PR_HOST		0x00000002	/* Virtualize hostname et al */
#define	PR_IP4_USER	0x00000004	/* Virtualize IPv4 addresses */
#define	PR_IP6_USER	0x00000008	/* Virtualize IPv6 addresses */
#define	PR_VNET		0x00000010	/* Virtual network stack */

/* Internal flag bits */
#define	PR_REMOVE	0x01000000	/* In process of being removed */
#define	PR_IP4		0x02000000	/* IPv4 virtualized by this jail or */
					/*  an ancestor			    */
#define	PR_IP6		0x04000000	/* IPv6 virtualized by this jail or */
					/*  an ancestor			    */

/* Flags for pr_allow */
#define	PR_ALLOW_SET_HOSTNAME		0x0001
#define	PR_ALLOW_SYSVIPC		0x0002
#define	PR_ALLOW_RAW_SOCKETS		0x0004
#define	PR_ALLOW_CHFLAGS		0x0008
#define	PR_ALLOW_MOUNT			0x0010
#define	PR_ALLOW_QUOTAS			0x0020
#define	PR_ALLOW_SOCKET_AF		0x0040
#define	PR_ALLOW_ALL			0x007f

/*
 * OSD methods
 */
#define	PR_METHOD_CREATE	0
#define	PR_METHOD_GET		1
#define	PR_METHOD_SET		2
#define	PR_METHOD_CHECK		3
#define	PR_METHOD_ATTACH	4
#define	PR_MAXMETHOD		5

/*
 * Lock/unlock a prison.
 * XXX These exist not so much for general convenience, but to be useable in
 *     the FOREACH_PRISON_DESCENDANT_LOCKED macro which can't handle them in
 *     non-function form as currently defined.
 */
static __inline void
prison_lock(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
}

static __inline void
prison_unlock(struct prison *pr)
{

	mtx_unlock(&pr->pr_mtx);
}

/* Traverse a prison's immediate children. */
#define	FOREACH_PRISON_CHILD(ppr, cpr)					\
	LIST_FOREACH(cpr, &(ppr)->pr_children, pr_sibling)

/*
 * Preorder traversal of all of a prison's descendants.
 * This ugly loop allows the macro to be followed by a single block
 * as expected in a looping primitive.
 */
#define	FOREACH_PRISON_DESCENDANT(ppr, cpr, descend)			\
	for ((cpr) = (ppr), (descend) = 1;				\
	    ((cpr) = (((descend) && !LIST_EMPTY(&(cpr)->pr_children))	\
	      ? LIST_FIRST(&(cpr)->pr_children)				\
	      : ((cpr) == (ppr)						\
		 ? NULL							\
		 : (((descend) = LIST_NEXT(cpr, pr_sibling) != NULL)	\
		    ? LIST_NEXT(cpr, pr_sibling)			\
		    : (cpr)->pr_parent))));)				\
		if (!(descend))						\
			;						\
		else

/*
 * As above, but lock descendants on the way down and unlock on the way up.
 */
#define	FOREACH_PRISON_DESCENDANT_LOCKED(ppr, cpr, descend)		\
	for ((cpr) = (ppr), (descend) = 1;				\
	    ((cpr) = (((descend) && !LIST_EMPTY(&(cpr)->pr_children))	\
	      ? LIST_FIRST(&(cpr)->pr_children)				\
	      : ((cpr) == (ppr)						\
		 ? NULL							\
		 : ((prison_unlock(cpr),				\
		    (descend) = LIST_NEXT(cpr, pr_sibling) != NULL)	\
		    ? LIST_NEXT(cpr, pr_sibling)			\
		    : (cpr)->pr_parent))));)				\
		if ((descend) ? (prison_lock(cpr), 0) : 1)		\
			;						\
		else

/*
 * As above, but also keep track of the level descended to.
 */
#define	FOREACH_PRISON_DESCENDANT_LOCKED_LEVEL(ppr, cpr, descend, level)\
	for ((cpr) = (ppr), (descend) = 1, (level) = 0;			\
	    ((cpr) = (((descend) && !LIST_EMPTY(&(cpr)->pr_children))	\
	      ? (level++, LIST_FIRST(&(cpr)->pr_children))		\
	      : ((cpr) == (ppr)						\
		 ? NULL							\
		 : ((prison_unlock(cpr),				\
		    (descend) = LIST_NEXT(cpr, pr_sibling) != NULL)	\
		    ? LIST_NEXT(cpr, pr_sibling)			\
		    : (level--, (cpr)->pr_parent)))));)			\
		if ((descend) ? (prison_lock(cpr), 0) : 1)		\
			;						\
		else

/*
 * Attributes of the physical system, and the root of the jail tree.
 */
extern struct	prison prison0;

TAILQ_HEAD(prisonlist, prison);
extern struct	prisonlist allprison;
extern struct	sx allprison_lock;

/*
 * Sysctls to describe jail parameters.
 */
SYSCTL_DECL(_security_jail_param);

#define	SYSCTL_JAIL_PARAM(module, param, type, fmt, descr)		\
    SYSCTL_PROC(_security_jail_param ## module, OID_AUTO, param,	\
	(type) | CTLFLAG_MPSAFE, NULL, 0, sysctl_jail_param, fmt, descr)
#define	SYSCTL_JAIL_PARAM_STRING(module, param, access, len, descr)	\
    SYSCTL_PROC(_security_jail_param ## module, OID_AUTO, param,	\
	CTLTYPE_STRING | CTLFLAG_MPSAFE | (access), NULL, len,		\
	sysctl_jail_param, "A", descr)
#define	SYSCTL_JAIL_PARAM_STRUCT(module, param, access, len, fmt, descr)\
    SYSCTL_PROC(_security_jail_param ## module, OID_AUTO, param,	\
	CTLTYPE_STRUCT | CTLFLAG_MPSAFE | (access), NULL, len,		\
	sysctl_jail_param, fmt, descr)
#define	SYSCTL_JAIL_PARAM_NODE(module, descr)				\
    SYSCTL_NODE(_security_jail_param, OID_AUTO, module, CTLFLAG_RW, 0, descr)

/*
 * Kernel support functions for jail().
 */
struct ucred;
struct mount;
struct sockaddr;
struct statfs;
int jailed(struct ucred *cred);
void getcredhostname(struct ucred *, char *, size_t);
void getcreddomainname(struct ucred *, char *, size_t);
void getcredhostuuid(struct ucred *, char *, size_t);
void getcredhostid(struct ucred *, unsigned long *);
int prison_allow(struct ucred *, unsigned);
int prison_check(struct ucred *cred1, struct ucred *cred2);
int prison_canseemount(struct ucred *cred, struct mount *mp);
void prison_enforce_statfs(struct ucred *cred, struct mount *mp,
    struct statfs *sp);
struct prison *prison_find(int prid);
struct prison *prison_find_child(struct prison *, int);
struct prison *prison_find_name(struct prison *, const char *);
int prison_flag(struct ucred *, unsigned);
void prison_free(struct prison *pr);
void prison_free_locked(struct prison *pr);
void prison_hold(struct prison *pr);
void prison_hold_locked(struct prison *pr);
void prison_proc_hold(struct prison *);
void prison_proc_free(struct prison *);
int prison_ischild(struct prison *, struct prison *);
int prison_equal_ip4(struct prison *, struct prison *);
int prison_get_ip4(struct ucred *cred, struct in_addr *ia);
int prison_local_ip4(struct ucred *cred, struct in_addr *ia);
int prison_remote_ip4(struct ucred *cred, struct in_addr *ia);
int prison_check_ip4(struct ucred *cred, struct in_addr *ia);
#ifdef INET6
int prison_equal_ip6(struct prison *, struct prison *);
int prison_get_ip6(struct ucred *, struct in6_addr *);
int prison_local_ip6(struct ucred *, struct in6_addr *, int);
int prison_remote_ip6(struct ucred *, struct in6_addr *);
int prison_check_ip6(struct ucred *, struct in6_addr *);
#endif
int prison_check_af(struct ucred *cred, int af);
int prison_if(struct ucred *cred, struct sockaddr *sa);
char *prison_name(struct prison *, struct prison *);
int prison_priv_check(struct ucred *cred, int priv);
int sysctl_jail_param(struct sysctl_oid *, void *, int , struct sysctl_req *);

#endif /* _KERNEL */
#endif /* !_SYS_JAIL_H_ */
