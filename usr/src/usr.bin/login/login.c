/*
 * Copyright (c) 1980, 1987, 1988 The Regents of the University of California.
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
"@(#) Copyright (c) 1980, 1987, 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)login.c	5.29 (Berkeley) 1/20/89";
#endif /* not lint */

/*
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */

#include <sys/param.h>
#include <sys/quota.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <utmp.h>
#include <signal.h>
#include <lastlog.h>
#include <errno.h>
#include <ttyent.h>
#include <syslog.h>
#include <grp.h>
#include <pwd.h>
#include <setjmp.h>
#include <stdio.h>
#include <strings.h>

#ifdef	KERBEROS
#include <kerberos/krb.h>
#include <sys/termios.h>
char	inst[INST_SZ];
char	realm[REALM_SZ];
int	kerror = KSUCCESS, notickets = 1;
#define	LIFE		96	/* ticket lifetime in 5-min units */
#endif

#define	TTYGRPNAME	"tty"		/* name of group to own ttys */

#define	MOTDFILE	"/etc/motd"
#define	MAILDIR		"/usr/spool/mail"
#define	NOLOGIN		"/etc/nologin"
#define	HUSHLOGIN	".hushlogin"
#define	LASTLOG		"/usr/adm/lastlog"
#define	BSHELL		"/bin/sh"

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
int	timeout = 300;

struct	passwd *pwd;
int	failures;
char	term[64], *hostname, *username, *tty;

struct	sgttyb sgttyb;
struct	tchars tc = {
	CINTR, CQUIT, CSTART, CSTOP, CEOT, CBRK
};
struct	ltchars ltc = {
	CSUSP, CDSUSP, CRPRNT, CFLUSH, CWERASE, CLNEXT
};

main(argc, argv)
	int argc;
	char **argv;
{
	extern int errno, optind;
	extern char *optarg, **environ;
	struct group *gr;
	register int ch;
	register char *p;
	int ask, fflag, hflag, pflag, cnt;
	int quietlog, passwd_req, ioctlval, timedout();
	char *domain, *salt, *envinit[1], *ttyn, *pp;
	char tbuf[MAXPATHLEN + 2];
	char *ttyname(), *stypeof(), *crypt(), *getpass();
	time_t time();
	off_t lseek();

	(void)signal(SIGALRM, timedout);
	(void)alarm((u_int)timeout);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)setpriority(PRIO_PROCESS, 0, 0);
	(void)quota(Q_SETUID, 0, 0, 0);

	/*
	 * -p is used by getty to tell login not to destroy the environment
 	 * -f is used to skip a second login authentication 
	 * -h is used by other servers to pass the name of the remote
	 *    host to login so that it may be placed in utmp and wtmp
	 */
	(void)gethostname(tbuf, sizeof(tbuf));
	domain = index(tbuf, '.');

	fflag = hflag = pflag = 0;
	passwd_req = 1;
	while ((ch = getopt(argc, argv, "fh:p")) != EOF)
		switch (ch) {
		case 'f':
			fflag = 1;
			break;
		case 'h':
			if (getuid()) {
				fprintf(stderr,
				    "login: -h for super-user only.\n");
				exit(1);
			}
			hflag = 1;
			if (domain && (p = index(optarg, '.')) &&
			    strcasecmp(p, domain) == 0)
				*p = 0;
			hostname = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		case '?':
		default:
			fprintf(stderr, "usage: login [-fp] [username]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;
	if (*argv) {
		username = *argv;
		ask = 0;
	} else
		ask = 1;

	ioctlval = 0;
	(void)ioctl(0, TIOCLSET, &ioctlval);
	(void)ioctl(0, TIOCNXCL, 0);
	(void)fcntl(0, F_SETFL, ioctlval);
	(void)ioctl(0, TIOCGETP, &sgttyb);
	sgttyb.sg_erase = CERASE;
	sgttyb.sg_kill = CKILL;
	(void)ioctl(0, TIOCSLTC, &ltc);
	(void)ioctl(0, TIOCSETC, &tc);
	(void)ioctl(0, TIOCSETP, &sgttyb);

	for (cnt = getdtablesize(); cnt > 2; cnt--)
		close(cnt);

	ttyn = ttyname(0);
	if (ttyn == NULL || *ttyn == '\0')
		ttyn = "/dev/tty??";
	if (tty = rindex(ttyn, '/'))
		++tty;
	else
		tty = ttyn;

	openlog("login", LOG_ODELAY, LOG_AUTH);

	for (cnt = 0;; ask = 1) {
		ioctlval = 0;
		(void)ioctl(0, TIOCSETD, &ioctlval);

		if (ask) {
			fflag = 0;
			getloginname();
		}
		/*
		 * Note if trying multiple user names;
		 * log failures for previous user name,
		 * but don't bother logging one failure
		 * for nonexistent name (mistyped username).
		 */
		if (failures && strcmp(tbuf, username)) {
			if (failures > (pwd ? 0 : 1))
				badlogin(tbuf);
			failures = 0;
		}
		(void)strcpy(tbuf, username);
		if (pwd = getpwnam(username))
			salt = pwd->pw_passwd;
		else
			salt = "xx";

		/* if user not super-user, check for disabled logins */
		if (pwd == NULL || pwd->pw_uid)
			checknologin();

		/*
		 * Disallow automatic login to root; if not invoked by
		 * root, disallow if the uid's differ.
		 */
		if (fflag && pwd) {
			int uid = getuid();

			passwd_req = pwd->pw_uid == 0 ||
			    (uid && uid != pwd->pw_uid);
		}

		/*
		 * If no pre-authentication and a password exists
		 * for this user, prompt for one and verify it.
		 */
		if (!passwd_req || (pwd && !*pwd->pw_passwd))
			break;

		setpriority(PRIO_PROCESS, 0, -4);
		pp = getpass("Password:");
		p = crypt(pp, salt);
		setpriority(PRIO_PROCESS, 0, 0);

#ifdef	KERBEROS

		/*
		 * If we aren't Kerberos-authenticated, try the normal
		 * pw file for a password.  If that's ok, log the user
		 * in without issueing any tickets.
		 */

		if (!get_krbrlm(realm,1)) {
			/* get TGT for local realm */
			kerror = get_in_tkt(pwd->pw_name, inst, realm,
				"krbtgt", realm, LIFE, pp);
			if (kerror == KSUCCESS) {
				bzero(pp, strlen(pp));
				notickets = 0;	/* user got ticket */
				break;
			}
		}
#endif
		(void) bzero(pp, strlen(pp));
		if (pwd && !strcmp(p, pwd->pw_passwd))
			break;

		printf("Login incorrect\n");
		failures++;
		/* we allow 10 tries, but after 3 we start backing off */
		if (++cnt > 3) {
			if (cnt >= 10) {
				badlogin(username);
				(void)ioctl(0, TIOCHPCL, (struct sgttyb *)NULL);
				sleepexit(1);
			}
			sleep((u_int)((cnt - 3) * 5));
		}
	}

	/* committed to login -- turn off timeout */
	(void)alarm((u_int)0);

	/*
	 * If valid so far and root is logging in, see if root logins on
	 * this terminal are permitted.
	 */
	if (pwd->pw_uid == 0 && !rootterm(tty)) {
		if (hostname)
			syslog(LOG_NOTICE, "ROOT LOGIN REFUSED FROM %s",
			    hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN REFUSED ON %s", tty);
		printf("Login incorrect\n");
		sleepexit(1);
	}

	if (quota(Q_SETUID, pwd->pw_uid, 0, 0) < 0 && errno != EINVAL) {
		switch(errno) {
		case EUSERS:
			fprintf(stderr,
		"Too many users logged on already.\nTry again later.\n");
			break;
		case EPROCLIM:
			fprintf(stderr,
			    "You have too many processes running.\n");
			break;
		default:
			perror("quota (Q_SETUID)");
		}
		sleepexit(0);
	}

	if (chdir(pwd->pw_dir) < 0) {
		printf("No directory %s!\n", pwd->pw_dir);
		if (chdir("/"))
			exit(0);
		pwd->pw_dir = "/";
		printf("Logging in with home = \"/\".\n");
	}

#ifdef	KERBEROS
	if (notickets)
		printf("Warning: no Kerberos tickets issued\n");
#endif

	/* nothing else left to fail -- really log in */
	{
		struct utmp utmp;

		bzero((char *)&utmp, sizeof(utmp));
		(void)time(&utmp.ut_time);
		strncpy(utmp.ut_name, username, sizeof(utmp.ut_name));
		if (hostname)
			strncpy(utmp.ut_host, hostname, sizeof(utmp.ut_host));
		strncpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
		login(&utmp);
	}

	quietlog = access(HUSHLOGIN, F_OK) == 0;
	dolastlog(quietlog);

	if (!hflag) {					/* XXX */
		static struct winsize win = { 0, 0, 0, 0 };

		(void)ioctl(0, TIOCSWINSZ, &win);
	}

	(void)chown(ttyn, pwd->pw_uid,
	    (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid);
	(void)chmod(ttyn, 0620);
	(void)setgid(pwd->pw_gid);

	initgroups(username, pwd->pw_gid);

	quota(Q_DOWARN, pwd->pw_uid, (dev_t)-1, 0);
	(void)setuid(pwd->pw_uid);

	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = BSHELL;
	/* turn on new line discipline for the csh */
	else if (!strcmp(pwd->pw_shell, "/bin/csh")) {
		ioctlval = NTTYDISC;
		(void)ioctl(0, TIOCSETD, &ioctlval);
	}

	/* destroy environment unless user has requested preservation */
	if (!pflag)
		environ = envinit;
	(void)setenv("HOME", pwd->pw_dir, 1);
	(void)setenv("SHELL", pwd->pw_shell, 1);
	if (term[0] == '\0')
		strncpy(term, stypeof(tty), sizeof(term));
	(void)setenv("TERM", term, 0);
	(void)setenv("USER", pwd->pw_name, 1);
	(void)setenv("PATH", "/usr/ucb:/bin:/usr/bin:", 0);

	if (tty[sizeof("tty")-1] == 'd')
		syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);
	if (pwd->pw_uid == 0)
		if (hostname)
			syslog(LOG_NOTICE, "ROOT LOGIN ON %s FROM %s",
			    tty, hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN ON %s", tty);

	if (!quietlog) {
		struct stat st;

		motd();
		(void)sprintf(tbuf, "%s/%s", MAILDIR, pwd->pw_name);
		if (stat(tbuf, &st) == 0 && st.st_size != 0)
			printf("You have %smail.\n",
			    (st.st_mtime > st.st_atime) ? "new " : "");
	}

	(void)signal(SIGALRM, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGTSTP, SIG_IGN);

	tbuf[0] = '-';
	strcpy(tbuf + 1, (p = rindex(pwd->pw_shell, '/')) ?
	    p + 1 : pwd->pw_shell);
	execlp(pwd->pw_shell, tbuf, 0);
	fprintf(stderr, "login: no shell: ");
	perror(pwd->pw_shell);
	exit(0);
}

getloginname()
{
	register int ch;
	register char *p;
	static char nbuf[UT_NAMESIZE + 1];

	for (;;) {
		printf("login: ");
		for (p = nbuf; (ch = getchar()) != '\n'; ) {
			if (ch == EOF) {
				badlogin(username);
				exit(0);
			}
			if (p < nbuf + UT_NAMESIZE)
				*p++ = ch;
		}
		if (p > nbuf)
			if (nbuf[0] == '-')
				fprintf(stderr,
				    "login names may not start with '-'.\n");
			else {
				*p = '\0';
				username = nbuf;
				break;
			}
	}
}

timedout()
{
	fprintf(stderr, "Login timed out after %d seconds\n", timeout);
	exit(0);
}

rootterm(ttyn)
	char *ttyn;
{
	struct ttyent *t;

	return((t = getttynam(ttyn)) && t->ty_status&TTY_SECURE);
}

jmp_buf motdinterrupt;

motd()
{
	register int fd, nchars;
	int (*oldint)(), sigint();
	char tbuf[8192];

	if ((fd = open(MOTDFILE, O_RDONLY, 0)) < 0)
		return;
	oldint = signal(SIGINT, sigint);
	if (setjmp(motdinterrupt) == 0)
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
	(void)signal(SIGINT, oldint);
	(void)close(fd);
}

sigint()
{
	longjmp(motdinterrupt, 1);
}

checknologin()
{
	register int fd, nchars;
	char tbuf[8192];

	if ((fd = open(NOLOGIN, O_RDONLY, 0)) >= 0) {
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
		sleepexit(0);
	}
}

dolastlog(quiet)
	int quiet;
{
	struct lastlog ll;
	int fd;

	if ((fd = open(LASTLOG, O_RDWR, 0)) >= 0) {
		(void)lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), L_SET);
		if (!quiet) {
			if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
			    ll.ll_time != 0) {
				printf("Last login: %.*s ",
				    24-5, (char *)ctime(&ll.ll_time));
				if (*ll.ll_host != '\0')
					printf("from %.*s\n",
					    sizeof(ll.ll_host), ll.ll_host);
				else
					printf("on %.*s\n",
					    sizeof(ll.ll_line), ll.ll_line);
			}
			(void)lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), L_SET);
		}
		bzero((char *)&ll, sizeof(ll));
		(void)time(&ll.ll_time);
		strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
		if (hostname)
			strncpy(ll.ll_host, hostname, sizeof(ll.ll_host));
		(void)write(fd, (char *)&ll, sizeof(ll));
		(void)close(fd);
	}
}

badlogin(name)
	char *name;
{
	if (failures == 0)
		return;
	if (hostname)
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s FROM %s, %s",
		    failures, failures > 1 ? "S" : "", hostname, name);
	else
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s ON %s, %s",
		    failures, failures > 1 ? "S" : "", tty, name);
}

#undef	UNKNOWN
#define	UNKNOWN	"su"

char *
stypeof(ttyid)
	char *ttyid;
{
	struct ttyent *t;

	return(ttyid && (t = getttynam(ttyid)) ? t->ty_type : UNKNOWN);
}

getstr(buf, cnt, err)
	char *buf, *err;
	int cnt;
{
	char ch;

	do {
		if (read(0, &ch, sizeof(ch)) != sizeof(ch))
			exit(1);
		if (--cnt < 0) {
			fprintf(stderr, "%s too long\r\n", err);
			sleepexit(1);
		}
		*buf++ = ch;
	} while (ch);
}

sleepexit(eval)
	int eval;
{
	sleep((u_int)5);
	exit(eval);
}
