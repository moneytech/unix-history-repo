/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)ext.h	5.1 (Berkeley) %G%
 */

/*
 * Telnet server variable declarations
 */
extern char	hisopts[256];
extern char	myopts[256];
extern char	hiswants[256];
extern char	mywants[256];
extern char	resp[256];
extern int	linemode;	/* linemode on/off */
#ifdef	LINEMODE
extern int	uselinemode;	/* what linemode to use (on/off) */
extern int	editmode;	/* edit modes in use */
extern int	useeditmode;	/* edit modes to use */
extern int	alwayslinemode;	/* command line option */
# ifdef	KLUDGELINEMODE
extern int	lmodetype;	/* Client support for linemode */
# endif	/* KLUDGELINEMODE */
#endif	/* LINEMODE */
extern int	flowmode;	/* current flow control state */

extern slcfun	slctab[NSLC + 1];	/* slc mapping table */

char	*terminaltype;

/*
 * I/O data buffers, pointers, and counters.
 */
extern char	ptyobuf[BUFSIZ+NETSLOP], *pfrontp, *pbackp;

extern char	netibuf[BUFSIZ], *netip;

extern char	netobuf[BUFSIZ+NETSLOP], *nfrontp, *nbackp;
extern char	*neturg;		/* one past last bye of urgent data */

extern int	pcc, ncc;

extern int	pty, net;
extern char	*line;
extern int	SYNCHing;		/* we are in TELNET SYNCH mode */

/*
 * The following are some clocks used to decide how to interpret
 * the relationship between various variables.
 */

extern struct {
    int
	system,			/* what the current time is */
	echotoggle,		/* last time user entered echo character */
	modenegotiated,		/* last time operating mode negotiated */
	didnetreceive,		/* last time we read data from network */
	ttypesubopt,		/* ttype subopt is received */
	tspeedsubopt,		/* tspeed subopt is received */
	baseline,		/* time started to do timed action */
	gotDM;			/* when did we last see a data mark */
} clocks;


#ifdef CRAY2
extern int	needtermstat;
#endif

#ifndef	CRAY
#define DEFAULT_IM	"\r\n\r\n4.3 BSD UNIX (%h) (%t)\r\n\r\r\n\r"
#else
#define DEFAULT_IM	"\r\n\r\nCray UNICOS (%h) (%t)\r\n\r\r\n\r"
#endif
