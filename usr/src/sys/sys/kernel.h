/*	kernel.h	4.5	82/10/20	*/

/*
 * Global variables for the kernel
 */

/* 1.1 */
long	hostid;
char	hostname[32];
int	hostnamelen;

/* 1.2 */
struct	timeval boottime;
struct	timeval time;
struct	timezone tz;			/* XXX */
int	hz;
int	tick;
int	lbolt;				/* awoken once a second */
int	realitexpire();

#ifdef GPROF
extern	int profiling;
extern	char *s_lowpc;
extern	u_long s_textsize;
extern	u_short *kcount;
#endif
