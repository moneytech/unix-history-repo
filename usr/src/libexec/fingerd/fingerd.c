/*
 * Copyright (c) 1983 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)fingerd.c	5.9 (Berkeley) %G%";
#endif /* not lint */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <syslog.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "pathnames.h"

void err __P((const char *, ...));

int
main(int argc, char **argv)
{
	register FILE *fp;
	register int ch;
	register char *lp;
	int p[2];
#define	ENTRIES	50
	char **ap, *av[ENTRIES + 1], **comp, line[1024];
	char *prog = _PATH_FINGER;
	struct hostent *hp;
	struct sockaddr_in sin;
	int sval = sizeof(sin);
	int secure=0, logging=0;
	extern char *optarg;

	openlog("fingerd", LOG_PID|LOG_CONS, LOG_DAEMON);
	while ((ch=getopt(argc, argv, "slp:")) != EOF)
		switch (ch) {
		case 's':
			secure++;
			break;
		case 'l':
			logging++;
			break;
		case 'p':
			prog = optarg;
			break;
		case '?':
		default:
			syslog(LOG_ERR, "unknown option: %c", ch);
		}

	if (logging) {
		if (getpeername(0, (struct sockaddr *)&sin, &sval) < 0)
			err("getpeername: %s", strerror(errno));
		if (hp = gethostbyaddr((char *)&sin.sin_addr.s_addr,
				sizeof(sin.sin_addr.s_addr), AF_INET))
			lp = hp->h_name;
		else
			lp = inet_ntoa(sin.sin_addr);
		syslog(LOG_NOTICE, "query from %s", lp);
	}

	if (!fgets(line, sizeof(line), stdin))
		exit(1);
	
	comp = &av[1];
	for (lp = line, ap = &av[2];;) {
		*ap = strtok(lp, " \t\r\n");
		if (!*ap) {
			if (secure && ap == &av[2]) {
				puts("must provide username\r\n");
				exit(1);
			}
			break;
		}
		if (secure && strchr(*ap, '@')) {
			puts("fowarding service denied\r\n");
			exit(1);
		}

		/* RFC742: "/[Ww]" == "-l" */
		if ((*ap)[0] == '/' && ((*ap)[1] == 'W' || (*ap)[1] == 'w')) {
			av[1] = "-l";
			comp = &av[0];
		}
		else if (++ap == av + ENTRIES)
			break;
		lp = NULL;
	}

	if (lp = strrchr(prog, '/'))
		*comp = ++lp;
	else
		*comp = prog;
	if (pipe(p) < 0)
		err("pipe: %s", strerror(errno));

	switch(vfork()) {
	case 0:
		(void)close(p[0]);
		if (p[1] != 1) {
			(void)dup2(p[1], 1);
			(void)close(p[1]);
		}
		execv(prog, comp);
		err("execv: %s: %s", prog, strerror(errno));
		_exit(1);
	case -1:
		err("fork: %s", strerror(errno));
	}
	(void)close(p[1]);
	if (!(fp = fdopen(p[0], "r")))
		err("fdopen: %s", strerror(errno));
	while ((ch = getc(fp)) != EOF) {
		if (ch == '\n')
			putchar('\r');
		putchar(ch);
	}
	exit(0);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);
	exit(1);
	/* NOTREACHED */
}
