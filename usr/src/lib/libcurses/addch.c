/*
 * Copyright (c) 1981 Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)addch.c	5.9 (Berkeley) %G%";
#endif	/* not lint */

#include <curses.h>

/*
 * waddch --
 *	Add the character to the current position in the given window.
 *
 */

int
waddch(win, ch)
	WINDOW *win;
	int ch;
{
	static __LDATA buf;

	buf.ch = ch;
	buf.attr = 0;
	__waddch(win, &buf);
}

int
__waddch(win, dp)
	WINDOW *win;
	__LDATA *dp;
{
	static char buf[2];

	buf[0] = dp->ch;
	return (__waddbytes(win, buf, 1, dp->attr & __STANDOUT));
}
