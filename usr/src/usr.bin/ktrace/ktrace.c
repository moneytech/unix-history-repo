/*
 * Copyright (c) 1988 The Regents of the University of California.
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
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ktrace.c	1.4 (Berkeley) %G%";
#endif /* not lint */

#include "ktrace.h"

#define USAGE \
 "usage: ktrace [-acid] [-f trfile] [-t trops] [-p pid] [-g pgid]\n\
	trops: c = syscalls, n = namei, g = generic-i/o, a = everything\n\
	ktrace -C (clear everthing)\n"
	

char	*tracefile = DEF_TRACEFILE;
int	append, clear, descend, inherit;

main(argc, argv)
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	int facs = ALL_FACS;
	int ops = 0;
	int pid = 0;
	int ch;

	while ((ch = getopt(argc,argv,"Cacdp:g:if:t:")) != EOF)
		switch((char)ch) {
		case 'C':
			clear = 2;
			break;
		case 'c':
			clear = 1;
			break;
		case 'd':
			ops |= KTRFLAG_DESCEND;
			break;
		case 't':
			facs = getfacs(optarg);
			if (facs < 0) {
				fprintf(stderr, 
				    "ktrace: unknown facility in %s\n",
			 	     optarg);
				exit(1);
			}
			break;
		case 'p':
			pid = atoi(optarg);
			break;
		case 'g':
			pid = -atoi(optarg);
			break;
		case 'i':
			inherit++;
			break;
		case 'f':
			tracefile = optarg;
			break;
		case 'a':
			append++;
			break;
		default:
			fprintf(stderr,"usage: \n",*argv);
			exit(-1);
		}
	argv += optind, argc -= optind;
	
	if (inherit)
		facs |= KTRFAC_INHERIT;
	if (clear) {			/* untrace something */
		if (clear == 2) {	/* -C */
			ops = KTROP_CLEAR | KTRFLAG_DESCEND;
			pid = 1;
		} else {
			ops |= pid ? KTROP_CLEAR : KTROP_CLEARFILE;
		}
		if (ktrace(tracefile, ops, facs, pid) < 0) {
			perror("ktrace");
			exit(1);
		}
		exit(0);
	}

	if (pid == 0 && !*argv) {	/* nothing to trace */
		fprintf(stderr, USAGE);
		exit(1);
	}
			
	close(open(tracefile, O_WRONLY | O_CREAT, 0666));
	if (!append)
		close(open(tracefile, O_WRONLY | O_TRUNC));
	if (!*argv) {
		if (ktrace(tracefile, ops, facs, pid) < 0) {
			perror("ktrace");
			exit(1);
		}
	} else {
		pid = getpid();
		if (ktrace(tracefile, ops, facs, pid) < 0) {
			perror("ktrace");
			exit(1);
		}
		execvp(argv[0], &argv[0]);
		perror("ktrace: exec failed");
	}
	exit(0);
}
