/*-
 * Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
static char copyright[] =
"@(#) Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)login.c	8.4 (Berkeley) 4/2/94";
#endif /* not lint */

/*
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ttyent.h>
#include <unistd.h>
#include <utmp.h>

#ifdef	SKEY
#include <skey.h>
#endif

#include "pathnames.h"

void	 badlogin __P((char *));
void	 checknologin __P((void));
void	 dolastlog __P((int));
void	 getloginname __P((void));
void	 motd __P((void));
int	 rootterm __P((char *));
void	 sigint __P((int));
void	 sleepexit __P((int));
char	*stypeof __P((char *));
void	 timedout __P((int));
void     login_fbtab __P((char *, uid_t, gid_t));
#ifdef KERBEROS
int	 klogin __P((struct passwd *, char *, char *, char *));
#endif

extern void login __P((struct utmp *));

#define	TTYGRPNAME	"tty"		/* name of group to own ttys */
#define	DEFAULT_RETRIES	3
#define	DEFAULT_BACKOFF	10

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
u_int	timeout = 300;

#ifdef KERBEROS
int	notickets = 1;
int	noticketsdontcomplain = 1;
char	*instance;
char	*krbtkfile_env;
int	authok;
#endif

struct	passwd *pwd;
int	failures;
char	*term, *envinit[1], *hostname, *username, *tty;
char    full_hostname[MAXHOSTNAMELEN];

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char **environ;
	struct group *gr;
	struct stat st;
	struct timeval tp;
	struct utmp utmp;
	int rootok;
	int ask, ch, cnt, fflag, hflag, pflag, quietlog, rootlogin, rval;
	int changepass;
	uid_t uid;
	char *domain, *p, *ep, *salt, *ttyn;
	char tbuf[MAXPATHLEN + 2], tname[sizeof(_PATH_TTY) + 10];
	char localhost[MAXHOSTNAMELEN];
#ifdef	SKEY
	int permit_passwd = 0;
#endif

	(void)signal(SIGALRM, timedout);
	(void)alarm(timeout);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)setpriority(PRIO_PROCESS, 0, 0);

	openlog("login", LOG_ODELAY, LOG_AUTH);

	/*
	 * -p is used by getty to tell login not to destroy the environment
	 * -f is used to skip a second login authentication
	 * -h is used by other servers to pass the name of the remote
	 *    host to login so that it may be placed in utmp and wtmp
	 */
	*full_hostname = '\0';
	domain = NULL;
	if (gethostname(localhost, sizeof(localhost)) < 0)
		syslog(LOG_ERR, "couldn't get local hostname: %m");
	else
		domain = strchr(localhost, '.');

	fflag = hflag = pflag = 0;
	uid = getuid();
	while ((ch = getopt(argc, argv, "fh:p")) != EOF)
		switch (ch) {
		case 'f':
			fflag = 1;
			break;
		case 'h':
			if (uid)
				errx(1, "-h option: %s", strerror(EPERM));
			hflag = 1;
			strncpy(full_hostname, optarg, sizeof(full_hostname)-1);
			if (domain && (p = strchr(optarg, '.')) &&
			    strcasecmp(p, domain) == 0)
				*p = 0;
			if (strlen(optarg) > UT_HOSTSIZE) {
				struct hostent *hp = gethostbyname(optarg);

				if (hp != NULL) {
					struct in_addr in;

					memmove(&in, hp->h_addr, sizeof(in));
					optarg = strdup(inet_ntoa(in));
				} else
					optarg = "invalid hostname";
			}
			hostname = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		case '?':
		default:
			if (!uid)
				syslog(LOG_ERR, "invalid flag %c", ch);
			(void)fprintf(stderr,
			    "usage: login [-fp] [-h hostname] [username]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (*argv) {
		username = *argv;
		ask = 0;
	} else
		ask = 1;

	for (cnt = getdtablesize(); cnt > 2; cnt--)
		(void)close(cnt);

	ttyn = ttyname(STDIN_FILENO);
	if (ttyn == NULL || *ttyn == '\0') {
		(void)snprintf(tname, sizeof(tname), "%s??", _PATH_TTY);
		ttyn = tname;
	}
	if (tty = strrchr(ttyn, '/'))
		++tty;
	else
		tty = ttyn;

	for (cnt = 0;; ask = 1) {
		if (ask) {
			fflag = 0;
			getloginname();
		}
		rootlogin = 0;
		rootok = rootterm(tty);
#ifdef	KERBEROS
		if ((instance = strchr(username, '.')) != NULL) {
			if (strncmp(instance, ".root", 5) == 0)
				rootlogin = 1;
			*instance++ = '\0';
		} else
			instance = "";
#endif
		if (strlen(username) > UT_NAMESIZE)
			username[UT_NAMESIZE] = '\0';

		/*
		 * Note if trying multiple user names; log failures for
		 * previous user name, but don't bother logging one failure
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

		/*
		 * if we have a valid account name, and it doesn't have a
		 * password, or the -f option was specified and the caller
		 * is root or the caller isn't changing their uid, don't
		 * authenticate.
		 */
		if (pwd) {
			if (pwd->pw_uid == 0)
				rootlogin = 1;

			if (fflag && (uid == 0 || uid == pwd->pw_uid)) {
				/* already authenticated */
				break;
			} else if (pwd->pw_passwd[0] == '\0') {
				if (rootlogin && !rootok) {
					/* pretend password okay */
					rval = 0;
					goto ttycheck;
				}
			}
		}

		fflag = 0;

		(void)setpriority(PRIO_PROCESS, 0, -4);

#ifdef	SKEY
		permit_passwd = skeyaccess(username, tty,
					   hostname ? full_hostname : NULL,
					   NULL);
		p = skey_getpass("Password:", pwd, permit_passwd);
		ep = skey_crypt(p, salt, pwd, permit_passwd);
#else
		p = getpass("Password:");
		ep = crypt(p, salt);
#endif

		if (pwd) {
#ifdef KERBEROS
#ifdef SKEY
			/*
			 * Do not allow user to type in kerberos password
			 * over the net (actually, this is ok for encrypted
			 * links, but we have no way of determining if the
			 * link is encrypted.
			 */
			if (!permit_passwd) {
				rval = 1;		/* failed */
			} else
#endif
			rval = klogin(pwd, instance, localhost, p);
			if (rval != 0 && rootlogin && pwd->pw_uid != 0)
				rootlogin = 0;
			if (rval == 0)
				authok = 1;
			else if (rval == 1)
				rval = strcmp(ep, pwd->pw_passwd);
#else
			rval = strcmp(ep, pwd->pw_passwd);
#endif
		}
		memset(p, 0, strlen(p));

		(void)setpriority(PRIO_PROCESS, 0, 0);

	ttycheck:
		/*
		 * If trying to log in as root without Kerberos,
		 * but with insecure terminal, refuse the login attempt.
		 */
		if (pwd && !rval) {
#ifdef KERBEROS
			if (authok == 0 && rootlogin && !rootok) {
#else
			if (rootlogin && !rootok) {
#endif
				/*
				 * Fall through to standard failure message
				 * and standard backoff behaviour
				 */
				if (hostname)
					syslog(LOG_NOTICE,
					"LOGIN %s REFUSED FROM %s ON TTY %s",
					pwd->pw_name, full_hostname, tty);
				else
					syslog(LOG_NOTICE,
					"LOGIN %s REFUSED ON TTY %s",
					pwd->pw_name, tty);
			}
			else	/* valid password & authenticated */
				break;
		}

		(void)printf("Login incorrect\n");
		failures++;

		/*
		 * we allow 10 tries, but after 3 we start backing off
		 */
		if (++cnt > DEFAULT_BACKOFF) {
			if (cnt >= DEFAULT_RETRIES) {
				badlogin(username);
				sleepexit(1);
			}
			sleep((u_int)((cnt - 3) * 5));
		}
	}

	/* committed to login -- turn off timeout */
	(void)alarm((u_int)0);

	endpwent();

	/* if user not super-user, check for disabled logins */
	if (!rootlogin)
		checknologin();

	if (chdir(pwd->pw_dir) < 0) {
		(void)printf("No home directory %s!\n", pwd->pw_dir);
		if (chdir("/"))
			exit(0);
		pwd->pw_dir = "/";
		(void)printf("Logging in with home = \"/\".\n");
	}

	quietlog = access(_PATH_HUSHLOGIN, F_OK) == 0;

	if (pwd->pw_change || pwd->pw_expire)
		(void)gettimeofday(&tp, (struct timezone *)NULL);

	changepass=0;
	if (pwd->pw_change)
		if (tp.tv_sec >= pwd->pw_change) {
			(void)printf("Sorry -- your password has expired.\n");
			changepass=1;
		} else if (pwd->pw_change - tp.tv_sec <
		    2 * 7 * 86400 && !quietlog)
			(void)printf("Warning: your password expires on %s",
			    ctime(&pwd->pw_change));
	if (pwd->pw_expire)
		if (tp.tv_sec >= pwd->pw_expire) {
			(void)printf("Sorry -- your account has expired.\n");
			sleepexit(1);
		} else if (pwd->pw_expire - tp.tv_sec <
		    2 * 7 * 86400 && !quietlog)
			(void)printf("Warning: your account expires on %s",
			    ctime(&pwd->pw_expire));

	/* Nothing else left to fail -- really log in. */
	memset((void *)&utmp, 0, sizeof(utmp));
	(void)time(&utmp.ut_time);
	(void)strncpy(utmp.ut_name, username, sizeof(utmp.ut_name));
	if (hostname)
		(void)strncpy(utmp.ut_host, hostname, sizeof(utmp.ut_host));
	(void)strncpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
	login(&utmp);

	dolastlog(quietlog);

	/*
	 * Set device protections, depending on what terminal the
	 * user is logged in. This feature is used on Suns to give
	 * console users better privacy.
	 */
	login_fbtab(tty, pwd->pw_uid, pwd->pw_gid);

	(void)chown(ttyn, pwd->pw_uid,
	    (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid);
	(void)setgid(pwd->pw_gid);

	initgroups(username, pwd->pw_gid);

	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;

	if ((term = getenv("TERM")) != NULL)
		term = strdup(term);
	/* Destroy environment unless user has requested its preservation. */
	if (!pflag)
		environ = envinit;
	(void)setenv("HOME", pwd->pw_dir, 1);
	(void)setenv("SHELL", pwd->pw_shell, 1);
	if (term == NULL || *term == '\0')
		term = stypeof(tty);
	(void)setenv("TERM", term, 0);
	(void)setenv("LOGNAME", pwd->pw_name, 1);
	(void)setenv("USER", pwd->pw_name, 1);
	(void)setenv("PATH", _PATH_DEFPATH, 0);
#ifdef KERBEROS
	if (krbtkfile_env)
		(void)setenv("KRBTKFILE", krbtkfile_env, 1);
#endif

	if (tty[sizeof("tty")-1] == 'd')
		syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);

	/* If fflag is on, assume caller/authenticator has logged root login. */
	if (rootlogin && fflag == 0)
		if (hostname)
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s FROM %s",
			    username, tty, full_hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s", username, tty);

#ifdef KERBEROS
	if (!quietlog && notickets == 1 && !noticketsdontcomplain)
		(void)printf("Warning: no Kerberos tickets issued.\n");
#endif

#ifdef LOGALL
	/*
	 * Syslog each successful login, so we don't have to watch hundreds
	 * of wtmp or lastlogin files.
	 */
	if (hostname) {
		syslog(LOG_INFO, "login from %s as %s", full_hostname, pwd->pw_name);
	} else {
		syslog(LOG_INFO, "login on %s as %s", tty, pwd->pw_name);
	}
#endif

	if (!quietlog) {
		(void)printf("%s\n\t%s  %s\n\n",
	    "Copyright (c) 1980, 1983, 1986, 1988, 1990, 1991, 1993, 1994",
		    "The Regents of the University of California. ",
		    "All rights reserved.");
		motd();
		(void)snprintf(tbuf,
		    sizeof(tbuf), "%s/%s", _PATH_MAILDIR, pwd->pw_name);
		if (stat(tbuf, &st) == 0 && st.st_size != 0)
			(void)printf("You have %smail.\n",
			    (st.st_mtime > st.st_atime) ? "new " : "");
	}

#ifdef LOGIN_ACCESS
	if (login_access(pwd->pw_name, hostname ? full_hostname : tty) == 0) {
		printf("Permission denied\n");
		if (hostname)
			syslog(LOG_NOTICE, "%s LOGIN REFUSED FROM %s",
				pwd->pw_name, full_hostname);
		else
			syslog(LOG_NOTICE, "%s LOGIN REFUSED ON %s",
				pwd->pw_name, tty);
		sleepexit(1);
	}
#endif

	(void)signal(SIGALRM, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGTSTP, SIG_IGN);

	tbuf[0] = '-';
	(void)strcpy(tbuf + 1, (p = strrchr(pwd->pw_shell, '/')) ?
	    p + 1 : pwd->pw_shell);

     	if (setlogin(pwd->pw_name) < 0)
                syslog(LOG_ERR, "setlogin() failure: %m");

	/* Discard permissions last so can't get killed and drop core. */
	if (rootlogin)
		(void) setuid(0);
	else
		(void) setuid(pwd->pw_uid);

	if (changepass) {
		int res;
		if ((res=system(_PATH_CHPASS)))
			sleepexit(1);
	}

	execlp(pwd->pw_shell, tbuf, 0);
	err(1, "%s", pwd->pw_shell);
}

#ifdef	KERBEROS
#define	NBUFSIZ		(UT_NAMESIZE + 1 + 5)	/* .root suffix */
#else
#define	NBUFSIZ		(UT_NAMESIZE + 1)
#endif

void
getloginname()
{
	int ch;
	char *p;
	static char nbuf[NBUFSIZ];

	for (;;) {
		(void)printf("login: ");
		for (p = nbuf; (ch = getchar()) != '\n'; ) {
			if (ch == EOF) {
				badlogin(username);
				exit(0);
			}
			if (p < nbuf + (NBUFSIZ - 1))
				*p++ = ch;
		}
		if (p > nbuf)
			if (nbuf[0] == '-')
				(void)fprintf(stderr,
				    "login names may not start with '-'.\n");
			else {
				*p = '\0';
				username = nbuf;
				break;
			}
	}
}

int
rootterm(ttyn)
	char *ttyn;
{
	struct ttyent *t;

	return ((t = getttynam(ttyn)) && t->ty_status & TTY_SECURE);
}

jmp_buf motdinterrupt;

void
motd()
{
	int fd, nchars;
	sig_t oldint;
	char tbuf[8192];

	if ((fd = open(_PATH_MOTDFILE, O_RDONLY, 0)) < 0)
		return;
	oldint = signal(SIGINT, sigint);
	if (setjmp(motdinterrupt) == 0)
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
	(void)signal(SIGINT, oldint);
	(void)close(fd);
}

/* ARGSUSED */
void
sigint(signo)
	int signo;
{

	longjmp(motdinterrupt, 1);
}

/* ARGSUSED */
void
timedout(signo)
	int signo;
{

	(void)fprintf(stderr, "Login timed out after %d seconds\n", timeout);
	exit(0);
}

void
checknologin()
{
	int fd, nchars;
	char tbuf[8192];

	if ((fd = open(_PATH_NOLOGIN, O_RDONLY, 0)) >= 0) {
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
		sleepexit(0);
	}
}

void
dolastlog(quiet)
	int quiet;
{
	struct lastlog ll;
	int fd;

	if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
		(void)lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), L_SET);
		if (!quiet) {
			if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
			    ll.ll_time != 0) {
				(void)printf("Last login: %.*s ",
				    24-5, (char *)ctime(&ll.ll_time));
				if (*ll.ll_host != '\0')
					(void)printf("from %.*s\n",
					    (int)sizeof(ll.ll_host),
					    ll.ll_host);
				else
					(void)printf("on %.*s\n",
					    (int)sizeof(ll.ll_line),
					    ll.ll_line);
			}
			(void)lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), L_SET);
		}
		memset((void *)&ll, 0, sizeof(ll));
		(void)time(&ll.ll_time);
		(void)strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
		if (hostname)
			(void)strncpy(ll.ll_host, hostname, sizeof(ll.ll_host));
		(void)write(fd, (char *)&ll, sizeof(ll));
		(void)close(fd);
	}
}

void
badlogin(name)
	char *name;
{

	if (failures == 0)
		return;
	if (hostname) {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s FROM %s",
		    failures, failures > 1 ? "S" : "", full_hostname);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s FROM %s, %s",
		    failures, failures > 1 ? "S" : "", full_hostname, name);
	} else {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s ON %s",
		    failures, failures > 1 ? "S" : "", tty);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s ON %s, %s",
		    failures, failures > 1 ? "S" : "", tty, name);
	}
}

#undef	UNKNOWN
#define	UNKNOWN	"su"

char *
stypeof(ttyid)
	char *ttyid;
{
	struct ttyent *t;

	return (ttyid && (t = getttynam(ttyid)) ? t->ty_type : UNKNOWN);
}

void
sleepexit(eval)
	int eval;
{

	(void)sleep(5);
	exit(eval);
}


