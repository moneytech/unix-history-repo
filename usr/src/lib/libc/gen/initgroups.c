/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)initgroups.c	5.7 (Berkeley) %G%";
#endif /* LIBC_SCCS and not lint */

/*
 * initgroups
 */
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>

struct group *getgrent();

int
initgroups(uname, agroup)
	const char *uname;
	int agroup;
{
	int groups[NGROUPS], ngroups = 0;
	register struct group *grp;
	register int i;

	/*
	 * If installing primary group, duplicate it;
	 * the first element of groups is the effective gid
	 * and will be overwritten when a setgid file is executed.
	 */
	if (agroup >= 0) {
		groups[ngroups++] = agroup;
		groups[ngroups++] = agroup;
	}
	setgrent();
	while (grp = getgrent()) {
		if (grp->gr_gid == agroup)
			continue;
		for (i = 0; grp->gr_mem[i]; i++)
			if (!strcmp(grp->gr_mem[i], uname)) {
				if (ngroups == NGROUPS) {
fprintf(stderr, "initgroups: %s is in too many groups\n", uname);
					goto toomany;
				}
				groups[ngroups++] = grp->gr_gid;
			}
	}
toomany:
	endgrent();
	if (setgroups(ngroups, groups) < 0) {
		perror("setgroups");
		return (-1);
	}
	return (0);
}
