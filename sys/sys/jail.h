/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#ifndef _SYS_JAIL_H_
#define _SYS_JAIL_H_

struct jail {
	u_int32_t	version;
	char		*path;
	char		*hostname;
	u_int32_t	ip_number;
};

#ifndef _KERNEL

int jail __P((struct jail *));

#else /* _KERNEL */

#include <sys/_lock.h>
#include <sys/_mutex.h>

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_PRISON);
#endif

/*
 * This structure describes a prison.  It is pointed to by all struct
 * ucreds's of the inmates.  pr_ref keeps track of them and is used to
 * delete the struture when the last inmate is dead.
 *
 * Lock key:
 *   (p) locked by pr_mutex
 *   (c) set only during creation before the structure is shared, no mutex
 *       required to read
 */
struct mtx;
struct prison {
	int		 pr_ref;			/* (p) refcount */
	char 		 pr_host[MAXHOSTNAMELEN];	/* (p) jail hostname */
	u_int32_t	 pr_ip;				/* (c) ip addr host */
	void		*pr_linux;			/* (p) linux abi */
	int		 pr_securelevel;		/* (p) securelevel */
	struct mtx	 pr_mtx;
};

/*
 * Sysctl-set variables that determine global jail policy
 *
 * XXX MIB entries will need to be protected by a mutex.
 */
extern int	jail_set_hostname_allowed;
extern int	jail_socket_unixiproute_only;
extern int	jail_sysvipc_allowed;

/*
 * Kernel support functions for jail().
 */
struct ucred;
struct sockaddr;
int jailed __P((struct ucred *cred));
void getcredhostname __P((struct ucred *cred, char *, size_t));
int prison_check __P((struct ucred *cred1, struct ucred *cred2));
void prison_free __P((struct prison *pr));
u_int32_t prison_getip __P((struct ucred *cred));
void prison_hold __P((struct prison *pr));
int prison_if __P((struct ucred *cred, struct sockaddr *sa));
int prison_ip __P((struct ucred *cred, int flag, u_int32_t *ip));
void prison_remote_ip __P((struct ucred *cred, int flags, u_int32_t *ip));

#endif /* !_KERNEL */
#endif /* !_SYS_JAIL_H_ */
