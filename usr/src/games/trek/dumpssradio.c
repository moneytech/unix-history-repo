/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)dumpssradio.c	8.1 (Berkeley) %G%";
#endif /* not lint */

# include	"trek.h"

/**
 **	output hidden distress calls
 **/

dumpssradio()
{
	register struct event	*e;
	register int		j;
	register int		chkrest;

	chkrest = 0;
	for (j = 0; j < MAXEVENTS; j++)
	{
		e = &Event[j];
		/* if it is not hidden, then just ignore it */
		if ((e->evcode & E_HIDDEN) == 0)
			continue;
		if (e->evcode & E_GHOST)
		{
			unschedule(e);
			printf("Starsystem %s in quadrant %d,%d is no longer distressed\n",
				systemname(e), e->x, e->y);
			continue;
		}

		switch (e->evcode)
		{

		  case E_KDESB:
			printf("Starbase in quadrant %d,%d is under attack\n",
				e->x, e->y);
			chkrest++;
			break;

		  case E_ENSLV:
		  case E_REPRO:
			printf("Starsystem %s in quadrant %d,%d is distressed\n",
				systemname(e), e->x, e->y);
			chkrest++;
			break;

		}
	}

	return (chkrest);
}
