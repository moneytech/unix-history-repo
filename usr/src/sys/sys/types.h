/*	types.h	4.5	82/09/08	*/

/*
 * Basic system types and major/minor device constructing/busting macros.
 */

/* major part of a device */
#define	major(x)	((int)(((unsigned)(x)>>8)&0377))

/* minor part of a device */
#define	minor(x)	((int)((x)&0377))

/* make a device number */
#define	makedev(x,y)	((dev_t)(((x)<<8) | (y)))

typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;

/* SHOULD USE long RATHER THAN int HERE BUT IT WOULD GIVE LINT ON THE KERNEL */
/* GASTRIC DISTRESS AND DON'T HAVE TIME TO FIX THAT JUST NOW */
typedef	struct	_physadr { int r[1]; } *physadr;
typedef	int	daddr_t;
typedef	char *	caddr_t;
typedef	u_long	ino_t;
typedef	int	swblk_t;
typedef	int	size_t;
typedef	int	time_t;
typedef	struct	label_t	{
	int	val[14];
} label_t;
typedef	short	dev_t;
typedef	int	off_t;
typedef	long	portid_t;

typedef	struct	fd_set { int fds_bits[1]; } fd_set;
