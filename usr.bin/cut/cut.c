/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam S. Moskowitz of Menlo Consulting and Marciano Pitargue.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
static const char sccsid[] = "@(#)cut.c	8.3 (Berkeley) 5/4/95";
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	bflag;
int	cflag;
char	dchar;
int	dflag;
int	fflag;
int	nflag;
int	sflag;

void	b_n_cut(FILE *, const char *);
void	c_cut(FILE *, const char *);
void	f_cut(FILE *, const char *);
void	get_list(char *);
void	needpos(size_t);
static 	void usage(void);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FILE *fp;
	void (*fcn)(FILE *, const char *);
	int ch, rval;

	setlocale(LC_ALL, "");

	fcn = NULL;
	dchar = '\t';			/* default delimiter is \t */

	/*
	 * Since we don't support multi-byte characters, the -c and -b
	 * options are equivalent.
	 */
	while ((ch = getopt(argc, argv, "b:c:d:f:sn")) != -1)
		switch(ch) {
		case 'b':
			fcn = c_cut;
			get_list(optarg);
			bflag = 1;
			break;
		case 'c':
			fcn = c_cut;
			get_list(optarg);
			cflag = 1;
			break;
		case 'd':
			dchar = *optarg;
			dflag = 1;
			break;
		case 'f':
			get_list(optarg);
			fcn = f_cut;
			fflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (fflag) {
		if (bflag || cflag || nflag)
			usage();
	} else if (!(bflag || cflag) || dflag || sflag)
		usage();
	else if (!bflag && nflag)
		usage();

	if (nflag)
		fcn = b_n_cut;

	rval = 0;
	if (*argv)
		for (; *argv; ++argv) {
			if (strcmp(*argv, "-") == 0)
				fcn(stdin, "stdin");
			else {
				if (!(fp = fopen(*argv, "r"))) {
					warn("%s", *argv);
					rval = 1;
					continue;
				}
				fcn(fp, *argv);
				(void)fclose(fp);
			}
		}
	else
		fcn(stdin, "stdin");
	exit(rval);
}

size_t autostart, autostop, maxval;

char *positions;

void
get_list(list)
	char *list;
{
	size_t setautostart, start, stop;
	char *pos;
	char *p;

	/*
	 * set a byte in the positions array to indicate if a field or
	 * column is to be selected; use +1, it's 1-based, not 0-based.
	 * This parser is less restrictive than the Draft 9 POSIX spec.
	 * POSIX doesn't allow lists that aren't in increasing order or
	 * overlapping lists.  We also handle "-3-5" although there's no
	 * real reason too.
	 */
	for (; (p = strsep(&list, ", \t")) != NULL;) {
		setautostart = start = stop = 0;
		if (*p == '-') {
			++p;
			setautostart = 1;
		}
		if (isdigit((unsigned char)*p)) {
			start = stop = strtol(p, &p, 10);
			if (setautostart && start > autostart)
				autostart = start;
		}
		if (*p == '-') {
			if (isdigit((unsigned char)p[1]))
				stop = strtol(p + 1, &p, 10);
			if (*p == '-') {
				++p;
				if (!autostop || autostop > stop)
					autostop = stop;
			}
		}
		if (*p)
			errx(1, "[-cf] list: illegal list value");
		if (!stop || !start)
			errx(1, "[-cf] list: values may not include zero");
		if (maxval < stop) {
			maxval = stop;
			needpos(maxval + 1);
		}
		for (pos = positions + start; start++ <= stop; *pos++ = 1);
	}

	/* overlapping ranges */
	if (autostop && maxval > autostop) {
		maxval = autostop;
		needpos(maxval + 1);
	}

	/* set autostart */
	if (autostart)
		memset(positions + 1, '1', autostart);
}

void
needpos(size_t n)
{
	static size_t npos;
	size_t oldnpos;

	/* Grow the positions array to at least the specified size. */
	if (n > npos) {
		oldnpos = npos;
		if (npos == 0)
			npos = n;
		while (n > npos)
			npos *= 2;
		if ((positions = realloc(positions, npos)) == NULL)
			err(1, "realloc");
		memset((char *)positions + oldnpos, 0, npos - oldnpos);
	}
}

/*
 * Cut based on byte positions, taking care not to split multibyte characters.
 * Although this function also handles the case where -n is not specified,
 * c_cut() ought to be much faster.
 */
void
b_n_cut(fp, fname)
	FILE *fp;
	const char *fname;
{
	size_t col, i, lbuflen;
	char *lbuf;
	int canwrite, clen, warned;

	warned = 0;
	while ((lbuf = fgetln(fp, &lbuflen)) != NULL) {
		for (col = 0; lbuflen > 0; col += clen) {
			if ((clen = mblen(lbuf, lbuflen)) < 0) {
				if (!warned) {
					warn("%s", fname);
					warned = 1;
				}
				clen = 1;
			}
			if (clen == 0 || *lbuf == '\n')
				break;
			if (col < maxval && !positions[1 + col]) {
				/*
				 * Print the character if (1) after an initial
				 * segment of un-selected bytes, the rest of
				 * it is selected, and (2) the last byte is
				 * selected.
				 */
				i = col;
				while (i < col + clen && i < maxval &&
				    !positions[1 + i])
					i++;
				canwrite = i < col + clen;
				for (; i < col + clen && i < maxval; i++)
					canwrite &= positions[1 + i];
				if (canwrite)
					fwrite(lbuf, 1, clen, stdout);
			} else {
				/*
				 * Print the character if all of it has
				 * been selected.
				 */
				canwrite = 1;
				for (i = col; i < col + clen; i++)
					if ((i >= maxval && !autostop) ||
					    (i < maxval && !positions[1 + i])) {
						canwrite = 0;
						break;
					}
				if (canwrite)
					fwrite(lbuf, 1, clen, stdout);
			}
			lbuf += clen;
			lbuflen -= clen;
		}
		if (lbuflen > 0)
			putchar('\n');
	}
}

void
c_cut(fp, fname)
	FILE *fp;
	const char *fname __unused;
{
	int ch, col;
	char *pos;

	ch = 0;
	for (;;) {
		pos = positions + 1;
		for (col = maxval; col; --col) {
			if ((ch = getc(fp)) == EOF)
				return;
			if (ch == '\n')
				break;
			if (*pos++)
				(void)putchar(ch);
		}
		if (ch != '\n') {
			if (autostop)
				while ((ch = getc(fp)) != EOF && ch != '\n')
					(void)putchar(ch);
			else
				while ((ch = getc(fp)) != EOF && ch != '\n');
		}
		(void)putchar('\n');
	}
}

void
f_cut(fp, fname)
	FILE *fp;
	const char *fname __unused;
{
	int ch, field, isdelim;
	char *pos, *p, sep;
	int output;
	char *lbuf, *mlbuf;
	size_t lbuflen;

	mlbuf = NULL;
	for (sep = dchar; (lbuf = fgetln(fp, &lbuflen)) != NULL;) {
		/* Assert EOL has a newline. */
		if (*(lbuf + lbuflen - 1) != '\n') {
			/* Can't have > 1 line with no trailing newline. */
			mlbuf = malloc(lbuflen + 1);
			if (mlbuf == NULL)
				err(1, "malloc");
			memcpy(mlbuf, lbuf, lbuflen);
			*(mlbuf + lbuflen) = '\n';
			lbuf = mlbuf;
		}
		output = 0;
		for (isdelim = 0, p = lbuf;; ++p) {
			ch = *p;
			/* this should work if newline is delimiter */
			if (ch == sep)
				isdelim = 1;
			if (ch == '\n') {
				if (!isdelim && !sflag)
					(void)fwrite(lbuf, lbuflen, 1, stdout);
				break;
			}
		}
		if (!isdelim)
			continue;

		pos = positions + 1;
		for (field = maxval, p = lbuf; field; --field, ++pos) {
			if (*pos) {
				if (output++)
					(void)putchar(sep);
				while ((ch = *p++) != '\n' && ch != sep)
					(void)putchar(ch);
			} else {
				while ((ch = *p++) != '\n' && ch != sep)
					continue;
			}
			if (ch == '\n')
				break;
		}
		if (ch != '\n') {
			if (autostop) {
				if (output)
					(void)putchar(sep);
				for (; (ch = *p) != '\n'; ++p)
					(void)putchar(ch);
			} else
				for (; (ch = *p) != '\n'; ++p);
		}
		(void)putchar('\n');
	}
	if (mlbuf != NULL)
		free(mlbuf);
}

static void
usage()
{
	(void)fprintf(stderr, "%s\n%s\n%s\n",
		"usage: cut -b list [-n] [file ...]",
		"       cut -c list [file ...]",
		"       cut -f list [-s] [-d delim] [file ...]");
	exit(1);
}
