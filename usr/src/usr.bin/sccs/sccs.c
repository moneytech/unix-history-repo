# include <stdio.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/dir.h>
# include <errno.h>
# include <signal.h>
# include <sysexits.h>
# include <whoami.h>

/*
**  SCCS.C -- human-oriented front end to the SCCS system.
**
**	Without trying to add any functionality to speak of, this
**	program tries to make SCCS a little more accessible to human
**	types.  The main thing it does is automatically put the
**	string "SCCS/s." on the front of names.  Also, it has a
**	couple of things that are designed to shorten frequent
**	combinations, e.g., "delget" which expands to a "delta"
**	and a "get".
**
**	This program can also function as a setuid front end.
**	To do this, you should copy the source, renaming it to
**	whatever you want, e.g., "syssccs".  Change any defaults
**	in the program (e.g., syssccs might default -d to
**	"/usr/src/sys").  Then recompile and put the result
**	as setuid to whomever you want.  In this mode, sccs
**	knows to not run setuid for certain programs in order
**	to preserve security, and so forth.
**
**	Usage:
**		sccs [flags] command [args]
**
**	Flags:
**		-d<dir>		<dir> represents a directory to search
**				out of.  It should be a full pathname
**				for general usage.  E.g., if <dir> is
**				"/usr/src/sys", then a reference to the
**				file "dev/bio.c" becomes a reference to
**				"/usr/src/sys/dev/bio.c".
**		-p<path>	prepends <path> to the final component
**				of the pathname.  By default, this is
**				"SCCS".  For example, in the -d example
**				above, the path then gets modified to
**				"/usr/src/sys/dev/SCCS/s.bio.c".  In
**				more common usage (without the -d flag),
**				"prog.c" would get modified to
**				"SCCS/s.prog.c".  In both cases, the
**				"s." gets automatically prepended.
**		-r		run as the real user.
**
**	Commands:
**		admin,
**		get,
**		delta,
**		rmdel,
**		chghist,
**		etc.		Straight out of SCCS; only difference
**				is that pathnames get modified as
**				described above.
**		edit		Macro for "get -e".
**		unedit		Removes a file being edited, knowing
**				about p-files, etc.
**		delget		Macro for "delta" followed by "get".
**		deledit		Macro for "delta" followed by "get -e".
**		info		Tell what files being edited.
**		clean		Remove all files that can be
**				regenerated from SCCS files.
**		check		Like info, but return exit status, for
**				use in makefiles.
**		fix		Remove a top delta & reedit, but save
**				the previous changes in that delta.
**
**	Compilation Flags:
**		UIDUSER -- determine who the user is by looking at the
**			uid rather than the login name -- for machines
**			where SCCS gets the user in this way.
**		SCCSDIR -- if defined, forces the -d flag to take on
**			this value.  This is so that the setuid
**			aspects of this program cannot be abused.
**			This flag also disables the -p flag.
**		SCCSPATH -- the default for the -p flag.
**		MYNAME -- the title this program should print when it
**			gives error messages.
**
**	Compilation Instructions:
**		cc -O -n -s sccs.c
**		The flags listed above can be -D defined to simplify
**			recompilation for variant versions.
**
**	Author:
**		Eric Allman, UCB/INGRES
**		Copyright 1980 Regents of the University of California
*/

static char SccsId[] = "@(#)sccs.c	1.48 %G%";

/*******************  Configuration Information  ********************/

/* special defines for local berkeley systems */
# include <whoami.h>

# ifdef CSVAX
# define UIDUSER
# define PROGPATH(name)	"/usr/local/name"
# endif CSVAX

# ifdef INGVAX
# define PROGPATH(name)	"/usr/local/name"
# endif INGVAX

/* end of berkeley systems defines */

# ifndef SCCSPATH
# define SCCSPATH	"SCCS"	/* pathname in which to find s-files */
# endif NOT SCCSPATH

# ifndef MYNAME
# define MYNAME		"sccs"	/* name used for printing errors */
# endif NOT MYNAME

# ifndef PROGPATH
# define PROGPATH(name)	"/usr/sccs/name"	/* place to find binaries */
# endif PROGPATH

/****************  End of Configuration Information  ****************/

typedef char	bool;
# define TRUE	1
# define FALSE	0

# define bitset(bit, word)	((bool) ((bit) & (word)))

# ifdef UIDUSER
# include <pwd.h>
# endif UIDUSER

struct sccsprog
{
	char	*sccsname;	/* name of SCCS routine */
	short	sccsoper;	/* opcode, see below */
	short	sccsflags;	/* flags, see below */
	char	*sccspath;	/* pathname of binary implementing */
};

/* values for sccsoper */
# define PROG		0	/* call a program */
# define CMACRO		1	/* command substitution macro */
# define FIX		2	/* fix a delta */
# define CLEAN		3	/* clean out recreatable files */
# define UNEDIT		4	/* unedit a file */
# define SHELL		5	/* call a shell file (like PROG) */
# define DIFFS		6	/* diff between sccs & file out */

/* bits for sccsflags */
# define NO_SDOT	0001	/* no s. on front of args */
# define REALUSER	0002	/* protected (e.g., admin) */

/* modes for the "clean", "info", "check" ops */
# define CLEANC		0	/* clean command */
# define INFOC		1	/* info command */
# define CHECKC		2	/* check command */
# define TELLC		3	/* give list of files being edited */

/*
**  Description of commands known to this program.
**	First argument puts the command into a class.  Second arg is
**	info regarding treatment of this command.  Third arg is a
**	list of flags this command accepts from macros, etc.  Fourth
**	arg is the pathname of the implementing program, or the
**	macro definition, or the arg to a sub-algorithm.
*/

struct sccsprog SccsProg[] =
{
	"admin",	PROG,	REALUSER,	PROGPATH(admin),
	"chghist",	PROG,	0,		PROGPATH(rmdel),
	"comb",		PROG,	0,		PROGPATH(comb),
	"delta",	PROG,	0,		PROGPATH(delta),
	"get",		PROG,	0,		PROGPATH(get),
	"help",		PROG,	NO_SDOT,	PROGPATH(help),
	"prt",		PROG,	0,		PROGPATH(prt),
	"rmdel",	PROG,	REALUSER,	PROGPATH(rmdel),
	"what",		PROG,	NO_SDOT,	PROGPATH(what),
	"sccsdiff",	SHELL,	REALUSER,	PROGPATH(sccsdiff),
	"edit",		CMACRO,	NO_SDOT,	"get -e",
	"delget",	CMACRO,	NO_SDOT,	"delta:mysrp/get:ixbeskcl -t",
	"deledit",	CMACRO,	NO_SDOT,	"delta:mysrp/get:ixbskcl -e -t",
	"fix",		FIX,	NO_SDOT,	NULL,
	"clean",	CLEAN,	REALUSER,	(char *) CLEANC,
	"info",		CLEAN,	REALUSER,	(char *) INFOC,
	"check",	CLEAN,	REALUSER,	(char *) CHECKC,
	"tell",		CLEAN,	REALUSER,	(char *) TELLC,
	"unedit",	UNEDIT,	NO_SDOT,	NULL,
	"diffs",	DIFFS,	NO_SDOT|REALUSER, NULL,
	NULL,		-1,	0,		NULL
};

/* one line from a p-file */
struct pfile
{
	char	*p_osid;	/* old SID */
	char	*p_nsid;	/* new SID */
	char	*p_user;	/* user who did edit */
	char	*p_date;	/* date of get */
	char	*p_time;	/* time of get */
};

char	*SccsPath = SCCSPATH;	/* pathname of SCCS files */
# ifdef SCCSDIR
char	*SccsDir = SCCSDIR;	/* directory to begin search from */
# else
char	*SccsDir = "";
# endif
char	MyName[] = MYNAME;	/* name used in messages */
int	OutFile = -1;		/* override output file for commands */
bool	RealUser;		/* if set, running as real user */
# ifdef DEBUG
bool	Debug;			/* turn on tracing */
# endif

main(argc, argv)
	int argc;
	char **argv;
{
	register char *p;
	extern struct sccsprog *lookup();
	register int i;

	/*
	**  Detect and decode flags intended for this program.
	*/

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s [flags] command [flags]\n", MyName);
		exit(EX_USAGE);
	}
	argv[argc] = NULL;

	if (lookup(argv[0]) == NULL)
	{
		while ((p = *++argv) != NULL)
		{
			if (*p != '-')
				break;
			switch (*++p)
			{
			  case 'r':		/* run as real user */
				setuid(getuid());
				RealUser++;
				break;

# ifndef SCCSDIR
			  case 'p':		/* path of sccs files */
				SccsPath = ++p;
				break;

			  case 'd':		/* directory to search from */
				SccsDir = ++p;
				break;
# endif

# ifdef DEBUG
			  case 'T':		/* trace */
				Debug++;
				break;
# endif

			  default:
				usrerr("unknown option -%s", p);
				break;
			}
		}
		if (SccsPath[0] == '\0')
			SccsPath = ".";
	}

	i = command(argv, FALSE, "");
	exit(i);
}

/*
**  COMMAND -- look up and perform a command
**
**	This routine is the guts of this program.  Given an
**	argument vector, it looks up the "command" (argv[0])
**	in the configuration table and does the necessary stuff.
**
**	Parameters:
**		argv -- an argument vector to process.
**		forkflag -- if set, fork before executing the command.
**		editflag -- if set, only include flags listed in the
**			sccsklets field of the command descriptor.
**		arg0 -- a space-seperated list of arguments to insert
**			before argv.
**
**	Returns:
**		zero -- command executed ok.
**		else -- error status.
**
**	Side Effects:
**		none.
*/

command(argv, forkflag, arg0)
	char **argv;
	bool forkflag;
	char *arg0;
{
	register struct sccsprog *cmd;
	register char *p;
	char buf[40];
	extern struct sccsprog *lookup();
	char *nav[1000];
	char **np;
	register char **ap;
	register int i;
	register char *q;
	extern bool unedit();
	int rval = 0;
	extern char *index();
	extern char *makefile();
	char *editchs;
	extern char *tail();

# ifdef DEBUG
	if (Debug)
	{
		printf("command:\n\t\"%s\"\n", arg0);
		for (np = argv; *np != NULL; np++)
			printf("\t\"%s\"\n", *np);
	}
# endif

	/*
	**  Copy arguments.
	**	Copy from arg0 & if necessary at most one arg
	**	from argv[0].
	*/

	np = ap = &nav[1];
	editchs = NULL;
	for (p = arg0, q = buf; *p != '\0' && *p != '/'; )
	{
		*np++ = q;
		while (*p == ' ')
			p++;
		while (*p != ' ' && *p != '\0' && *p != '/' && *p != ':')
			*q++ = *p++;
		*q++ = '\0';
		if (*p == ':')
		{
			editchs = q;
			while (*++p != '\0' && *p != '/' && *p != ' ')
				*q++ = *p;
			*q++ = '\0';
		}
	}
	*np = NULL;
	if (*ap == NULL)
		*np++ = *argv++;

	/*
	**  Look up command.
	**	At this point, *ap is the command name.
	*/

	cmd = lookup(*ap);
	if (cmd == NULL)
	{
		usrerr("Unknown command \"%s\"", *ap);
		return (EX_USAGE);
	}

	/*
	**  Copy remaining arguments doing editing as appropriate.
	*/

	for (; *argv != NULL; argv++)
	{
		p = *argv;
		if (*p == '-')
		{
			if (p[1] == '\0' || editchs == NULL || index(editchs, p[1]) != NULL)
				*np++ = p;
		}
		else
		{
			if (!bitset(NO_SDOT, cmd->sccsflags))
				p = makefile(p);
			if (p != NULL)
				*np++ = p;
		}
	}
	*np = NULL;

	/*
	**  Interpret operation associated with this command.
	*/

	switch (cmd->sccsoper)
	{
	  case SHELL:		/* call a shell file */
		*ap = cmd->sccspath;
		*--ap = "sh";
		rval = callprog("/bin/sh", cmd->sccsflags, ap, forkflag);
		break;

	  case PROG:		/* call an sccs prog */
		rval = callprog(cmd->sccspath, cmd->sccsflags, ap, forkflag);
		break;

	  case CMACRO:		/* command macro */
		/* step through & execute each part of the macro */
		for (p = cmd->sccspath; *p != '\0'; p++)
		{
			q = p;
			while (*p != '\0' && *p != '/')
				p++;
			rval = command(&ap[1], *p != '\0', q);
			if (rval != 0)
				break;
		}
		break;

	  case FIX:		/* fix a delta */
		if (strncmp(ap[1], "-r", 2) != 0)
		{
			usrerr("-r flag needed for fix command");
			rval = EX_USAGE;
			break;
		}

		/* get the version with all changes */
		rval = command(&ap[1], TRUE, "get -k");

		/* now remove that version from the s-file */
		if (rval == 0)
			rval = command(&ap[1], TRUE, "rmdel:r");

		/* and edit the old version (but don't clobber new vers) */
		if (rval == 0)
			rval = command(&ap[2], FALSE, "get -e -g");
		break;

	  case CLEAN:
		rval = clean((int) cmd->sccspath, ap);
		break;

	  case UNEDIT:
		for (argv = np = &ap[1]; *argv != NULL; argv++)
		{
			if (unedit(*argv))
				*np++ = *argv;
		}
		*np = NULL;

		/* get all the files that we unedited successfully */
		if (np > &ap[1])
			rval = command(&ap[1], FALSE, "get");
		break;

	  case DIFFS:		/* diff between s-file & edit file */
		/* find the end of the flag arguments */
		for (np = &ap[1]; *np != NULL && **np == '-'; np++)
			continue;
		argv = np;

		/* for each file, do the diff */
		p = argv[1];
		while (*np != NULL)
		{
			/* messy, but we need a null terminated argv */
			*argv = *np++;
			argv[1] = NULL;
			i = dodiff(ap, tail(*argv));
			if (rval == 0)
				rval = i;
			argv[1] = p;
		}
		break;

	  default:
		syserr("oper %d", cmd->sccsoper);
		exit(EX_SOFTWARE);
	}
# ifdef DEBUG
	if (Debug)
		printf("command: rval=%d\n", rval);
# endif
	return (rval);
}

/*
**  LOOKUP -- look up an SCCS command name.
**
**	Parameters:
**		name -- the name of the command to look up.
**
**	Returns:
**		ptr to command descriptor for this command.
**		NULL if no such entry.
**
**	Side Effects:
**		none.
*/

struct sccsprog *
lookup(name)
	char *name;
{
	register struct sccsprog *cmd;

	for (cmd = SccsProg; cmd->sccsname != NULL; cmd++)
	{
		if (strcmp(cmd->sccsname, name) == 0)
			return (cmd);
	}
	return (NULL);
}

/*
**  CALLPROG -- call a program
**
**	Used to call the SCCS programs.
**
**	Parameters:
**		progpath -- pathname of the program to call.
**		flags -- status flags from the command descriptors.
**		argv -- an argument vector to pass to the program.
**		forkflag -- if true, fork before calling, else just
**			exec.
**
**	Returns:
**		The exit status of the program.
**		Nothing if forkflag == FALSE.
**
**	Side Effects:
**		Can exit if forkflag == FALSE.
*/

callprog(progpath, flags, argv, forkflag)
	char *progpath;
	short flags;
	char **argv;
	bool forkflag;
{
	register int i;
	auto int st;

# ifdef DEBUG
	if (Debug)
	{
		printf("callprog:\n");
		for (i = 0; argv[i] != NULL; i++)
			printf("\t\"%s\"\n", argv[i]);
	}
# endif

	if (*argv == NULL)
		return (-1);

	/*
	**  Fork if appropriate.
	*/

	if (forkflag)
	{
# ifdef DEBUG
		if (Debug)
			printf("Forking\n");
# endif
		i = fork();
		if (i < 0)
		{
			syserr("cannot fork");
			exit(EX_OSERR);
		}
		else if (i > 0)
		{
			wait(&st);
			if ((st & 0377) == 0)
				st = (st >> 8) & 0377;
			if (OutFile >= 0)
			{
				close(OutFile);
				OutFile = -1;
			}
			return (st);
		}
	}
	else if (OutFile >= 0)
	{
		syserr("callprog: setting stdout w/o forking");
		exit(EX_SOFTWARE);
	}

	/* set protection as appropriate */
	if (bitset(REALUSER, flags))
		setuid(getuid());

	/* change standard input & output if needed */
	if (OutFile >= 0)
	{
		close(1);
		dup(OutFile);
		close(OutFile);
	}
	
	/* call real SCCS program */
	execv(progpath, argv);
	syserr("cannot execute %s", progpath);
	exit(EX_UNAVAILABLE);
	/*NOTREACHED*/
}

/*
**  MAKEFILE -- make filename of SCCS file
**
**	If the name passed is already the name of an SCCS file,
**	just return it.  Otherwise, munge the name into the name
**	of the actual SCCS file.
**
**	There are cases when it is not clear what you want to
**	do.  For example, if SccsPath is an absolute pathname
**	and the name given is also an absolute pathname, we go
**	for SccsPath (& only use the last component of the name
**	passed) -- this is important for security reasons (if
**	sccs is being used as a setuid front end), but not
**	particularly intuitive.
**
**	Parameters:
**		name -- the file name to be munged.
**
**	Returns:
**		The pathname of the sccs file.
**		NULL on error.
**
**	Side Effects:
**		none.
*/

char *
makefile(name)
	char *name;
{
	register char *p;
	char buf[512];
	extern char *malloc();
	extern char *rindex();
	extern bool isdir();
	register char *q;

	p = rindex(name, '/');
	if (p == NULL)
		p = name;
	else
		p++;

	/*
	**  See if the name can be used as-is.
	*/

	if (SccsPath[0] != '/' || name[0] == '/' || strncmp(name, "./", 2) == 0)
	{
		if (strncmp(p, "s.", 2) == 0)
			return (name);
		if (isdir(name))
			return (name);
	}

	/*
	**  Create the actual pathname.
	*/

	/* first the directory part */
	if (name[0] != '/')
	{
		strcpy(buf, SccsDir);
		strcat(buf, "/");
	}
	else
		strcpy(buf, "");
	
	/* then the head of the pathname */
	strncat(buf, name, p - name);
	q = &buf[strlen(buf)];
	strcpy(q, p);
	if (strncmp(p, "s.", 2) != 0 && !isdir(buf))
	{
		/* sorry, no; copy the SCCS pathname & the "s." */
		strcpy(q, SccsPath);
		strcat(buf, "/s.");

		/* and now the end of the name */
		strcat(buf, p);
	}

	/* if i haven't changed it, why did I do all this? */
	if (strcmp(buf, name) == 0)
		p = name;

	return (stat(name, &stbuf) >= 0 && (stbuf.st_mode & S_IFMT) == S_IFDIR);
}
/*
**  ISDIR -- return true if the argument is a directory.
**
**	Parameters:
**		name -- the pathname of the file to check.
**
**	Returns:
**		TRUE if 'name' is a directory, FALSE otherwise.
**
**	Side Effects:
**		none.
*/

bool
isdir(name)
	char *name;
{
	struct stat stbuf;

	return (stat(name, &stbuf) >= 0 && (stbuf.st_mode & S_IFMT) == S_IFDIR);
}

/*
**  SAFEPATH -- determine whether a pathname is "safe"
**
**	"Safe" pathnames only allow you to get deeper into the
**	directory structure, i.e., full pathnames and ".." are
**	not allowed.
**
**	Parameters:
**		p -- the name to check.
**
**	Returns:
**		TRUE -- if the path is safe.
**		FALSE -- if the path is not safe.
**
**	Side Effects:
**		Prints a message if the path is not safe.
*/

bool
safepath(p)
	register char *p;
{
	extern char *index();

	if (*p != '/')
	{
		while (strncmp(p, "../", 3) != 0 && strcmp(p, "..") != 0)
		{
			p = index(p, '/');
			if (p == NULL)
				return (TRUE);
			p++;
		}
	}

	printf("You may not use full pathnames or \"..\"\n");
	return (FALSE);
}

/*
**  CLEAN -- clean out recreatable files
**
**	Any file for which an "s." file exists but no "p." file
**	exists in the current directory is purged.
**
**	Parameters:
**		mode -- tells whether this came from a "clean", "info", or
**			"check" command.
**		argv -- the rest of the argument vector.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Removes files in the current directory.
**		Prints information regarding files being edited.
**		Exits if a "check" command.
*/

clean(mode, argv)
	int mode;
	char **argv;
{
	struct direct dir;
	char buf[100];
	register FILE *dirfd;
	register char *basefile;
	bool gotedit;
	bool gotpfent;
	FILE *pfp;
	bool nobranch = FALSE;
	extern struct pfile *getpfent();
	register struct pfile *pf;
	register char **ap;

	/*
	**  Process the argv
	*/

	for (ap = argv; *ap != NULL; ap++)
	{
		if (strcmp(*ap, "-b") == 0)
			nobranch = TRUE;
	}

	/*
	**  Find and open the SCCS directory.
	*/

	strcpy(buf, SccsDir);
	if (buf[0] != '\0')
		strcat(buf, "/");
	strcat(buf, SccsPath);

	dirfd = fopen(buf, "r");
	if (dirfd == NULL)
	{
		usrerr("cannot open %s", buf);
		return (EX_NOINPUT);
	}

	/*
	**  Scan the SCCS directory looking for s. files.
	**	gotedit tells whether we have tried to clean any
	**		files that are being edited.
	*/

	gotedit = FALSE;
	while (fread((char *)&dir, sizeof dir, 1, dirfd) != NULL)
	{
		if (dir.d_ino == 0 || strncmp(dir.d_name, "s.", 2) != 0)
			continue;
		
		/* got an s. file -- see if the p. file exists */
		strcpy(buf, SccsDir);
		if (buf[0] != '\0')
			strcat(buf, "/");
		strcat(buf, SccsPath);
		strcat(buf, "/p.");
		basefile = &buf[strlen(buf)];
		strncpy(basefile, &dir.d_name[2], sizeof dir.d_name - 2);
		basefile[sizeof dir.d_name - 2] = '\0';

		/*
		**  open and scan the p-file.
		**	'gotpfent' tells if we have found a valid p-file
		**		entry.
		*/

		pfp = fopen(buf, "r");
		gotpfent = FALSE;
		if (pfp != NULL)
		{
			/* the file exists -- report it's contents */
			while ((pf = getpfent(pfp)) != NULL)
			{
				if (nobranch && isbranch(pf->p_nsid))
					continue;
				gotedit = TRUE;
				gotpfent = TRUE;
				if (mode == TELLC)
				{
					printf("%s\n", basefile);
					break;
				}
				printf("%12s: being edited: %s %s %s %s %s\n",
				       basefile, pf->p_osid, pf->p_nsid,
				       pf->p_user, pf->p_date, pf->p_time);
			}
			fclose(pfp);
		}
		if (!gotpfent)
			continue;
		
		/* the s. file exists and no p. file exists -- unlink the g-file */
		if (mode == CLEANC)
		{
			strncpy(buf, &dir.d_name[2], sizeof dir.d_name - 2);
			buf[sizeof dir.d_name - 2] = '\0';
			unlink(buf);
		}
	}

	/* cleanup & report results */
	fclose(dirfd);
	if (!gotedit && mode == INFOC)
		printf("Nothing being edited\n");
	if (mode == CHECKC)
		exit(gotedit);
	return (EX_OK);
}

/*
**  ISBRANCH -- is the SID a branch?
**
**	Parameters:
**		sid -- the sid to check.
**
**	Returns:
**		TRUE if the sid represents a branch.
**		FALSE otherwise.
**
**	Side Effects:
**		none.
*/

isbranch(sid)
	char *sid;
{
	register char *p;
	int dots;

	dots = 0;
	for (p = sid; *p != '\0'; p++)
	{
		if (*p == '.')
			dots++;
		if (dots > 1)
			return (TRUE);
	}
	return (FALSE);
}

/*
**  UNEDIT -- unedit a file
**
**	Checks to see that the current user is actually editting
**	the file and arranges that s/he is not editting it.
**
**	Parameters:
**		fn -- the name of the file to be unedited.
**
**	Returns:
**		TRUE -- if the file was successfully unedited.
**		FALSE -- if the file was not unedited for some
**			reason.
**
**	Side Effects:
**		fn is removed
**		entries are removed from pfile.
*/

bool
unedit(fn)
	char *fn;
{
	register FILE *pfp;
	char *pfn;
	static char tfn[] = "/tmp/sccsXXXXX";
	FILE *tfp;
	register char *q;
	bool delete = FALSE;
	bool others = FALSE;
	char *myname;
	extern char *getlogin();
	struct pfile *pent;
	extern struct pfile *getpfent();
	char buf[120];
	extern char *makefile();
# ifdef UIDUSER
	struct passwd *pw;
	extern struct passwd *getpwuid();
# endif UIDUSER

	/* make "s." filename & find the trailing component */
	pfn = makefile(fn);
	if (pfn == NULL)
		return (FALSE);
	q = rindex(pfn, '/');
	if (q == NULL)
		q = &pfn[-1];
	if (q[1] != 's' || q[2] != '.')
	{
		usrerr("bad file name \"%s\"", fn);
		return (FALSE);
	}

	/* turn "s." into "p." & try to open it */
	*++q = 'p';

	pfp = fopen(pfn, "r");
	if (pfp == NULL)
	{
		printf("%12s: not being edited\n", fn);
		return (FALSE);
	}

	/* create temp file for editing p-file */
	mktemp(tfn);
	tfp = fopen(tfn, "w");
	if (tfp == NULL)
	{
		usrerr("cannot create \"%s\"", tfn);
		exit(EX_OSERR);
	}

	/* figure out who I am */
# ifdef UIDUSER
	pw = getpwuid(getuid());
	if (pw == NULL)
	{
		syserr("who are you? (uid=%d)", getuid());
		exit(EX_OSERR);
	}
	myname = pw->pw_name;
# else
	myname = getlogin();
# endif UIDUSER

	/*
	**  Copy p-file to temp file, doing deletions as needed.
	*/

	while ((pent = getpfent(pfp)) != NULL)
	{
		if (strcmp(pent->p_user, myname) == 0)
		{
			/* a match */
			delete++;
		}
		else
		{
			/* output it again */
			fprintf(tfp, "%s %s %s %s %s\n", pent->p_osid,
			    pent->p_nsid, pent->p_user, pent->p_date,
			    pent->p_time);
			others++;
		}
	}

	/* do final cleanup */
	if (others)
	{
		/* copy it back (perhaps it should be linked?) */
		if (freopen(tfn, "r", tfp) == NULL)
		{
			syserr("cannot reopen \"%s\"", tfn);
			exit(EX_OSERR);
		}
		if (freopen(pfn, "w", pfp) == NULL)
		{
			usrerr("cannot create \"%s\"", pfn);
			return (FALSE);
		}
		while (fgets(buf, sizeof buf, tfp) != NULL)
			fputs(buf, pfp);
	}
	else
	{
		/* it's empty -- remove it */
		unlink(pfn);
	}
	fclose(tfp);
	fclose(pfp);
	unlink(tfn);

	/* actually remove the g-file */
	if (delete)
	{
		unlink(tail(fn));
		printf("%12s: removed\n", tail(fn));
		return (TRUE);
	}
	else
	{
		printf("%12s: not being edited by you\n", fn);
		return (FALSE);
	}
}

/*
**  DODIFF -- diff an s-file against a g-file
**
**	Parameters:
**		getv -- argv for the 'get' command.
**		gfile -- name of the g-file to diff against.
**
**	Returns:
**		Result of get.
**
**	Side Effects:
**		none.
*/

dodiff(getv, gfile)
	char **getv;
	char *gfile;
{
	int pipev[2];
	int rval;
	register int i;
	register int pid;
	auto int st;
	extern int errno;
	int (*osig)();

	printf("\n------- %s -------\n", gfile);

	/* create context for diff to run in */
	if (pipe(pipev) < 0)
	{
		syserr("dodiff: pipe failed");
		exit(EX_OSERR);
	}
	if ((pid = fork()) < 0)
	{
		syserr("dodiff: fork failed");
		exit(EX_OSERR);
	}
	else if (pid > 0)
	{
		/* in parent; run get */
		OutFile = pipev[1];
		close(pipev[0]);
		rval = command(&getv[1], TRUE, "get -s -k -p");
		osig = signal(SIGINT, SIG_IGN);
		while (((i = wait(&st)) >= 0 && i != pid) || errno == EINTR)
			errno = 0;
		signal(SIGINT, osig);
		/* ignore result of diff */
	}
	else
	{
		/* in child, run diff */
		if (close(pipev[1]) < 0 || close(0) < 0 ||
		    dup(pipev[0]) != 0 || close(pipev[0]) < 0)
		{
			syserr("dodiff: magic failed");
			exit(EX_OSERR);
		}
		execl(PROGPATH(bdiff), "bdiff", "-", gfile, NULL);
# ifndef V6
		execlp("bdiff", "bdiff", "-", gfile, NULL);
		execlp("diff", "diff", "-", gfile, NULL);
# endif NOT V6
		syserr("bdiff: cannot execute");
		exit(EX_OSERR);
	}
	return (rval);
}

/*
**  TAIL -- return tail of filename.
**
**	Parameters:
**		fn -- the filename.
**
**	Returns:
**		a pointer to the tail of the filename; e.g., given
**		"cmd/ls.c", "ls.c" is returned.
**
**	Side Effects:
**		none.
*/

char *
tail(fn)
	register char *fn;
{
	register char *p;

	for (p = fn; *p != 0; p++)
		if (*p == '/' && p[1] != '\0' && p[1] != '/')
			fn = &p[1];
	return (fn);
}

/*
**  GETPFENT -- get an entry from the p-file
**
**	Parameters:
**		pfp -- p-file file pointer
**
**	Returns:
**		pointer to p-file struct for next entry
**		NULL on EOF or error
**
**	Side Effects:
**		Each call wipes out results of previous call.
*/

struct pfile *
getpfent(pfp)
	FILE *pfp;
{
	static struct pfile ent;
	static char buf[120];
	register char *p;
	extern char *nextfield();

	if (fgets(buf, sizeof buf, pfp) == NULL)
		return (NULL);

	ent.p_osid = p = buf;
	ent.p_nsid = p = nextfield(p);
	ent.p_user = p = nextfield(p);
	ent.p_date = p = nextfield(p);
	ent.p_time = p = nextfield(p);
	if (p == NULL || nextfield(p) != NULL)
		return (NULL);

	return (&ent);
}


char *
nextfield(p)
	register char *p;
{
	if (p == NULL || *p == '\0')
		return (NULL);
	while (*p != ' ' && *p != '\n' && *p != '\0')
		p++;
	if (*p == '\n' || *p == '\0')
	{
		*p = '\0';
		return (NULL);
	}
	*p++ = '\0';
	return (p);
}

/*
**  USRERR -- issue user-level error
**
**	Parameters:
**		f -- format string.
**		p1-p3 -- parameters to a printf.
**
**	Returns:
**		-1
**
**	Side Effects:
**		none.
*/

/*VARARGS1*/
usrerr(f, p1, p2, p3)
	char *f;
{
	fprintf(stderr, "\n%s: ", MyName);
	fprintf(stderr, f, p1, p2, p3);
	fprintf(stderr, "\n");

	return (-1);
}

/*
**  SYSERR -- print system-generated error.
**
**	Parameters:
**		f -- format string to a printf.
**		p1, p2, p3 -- parameters to f.
**
**	Returns:
**		never.
**
**	Side Effects:
**		none.
*/

/*VARARGS1*/
syserr(f, p1, p2, p3)
	char *f;
{
	extern int errno;

	fprintf(stderr, "\n%s SYSERR: ", MyName);
	fprintf(stderr, f, p1, p2, p3);
	fprintf(stderr, "\n");
	if (errno == 0)
		exit(EX_SOFTWARE);
	else
	{
		perror(NULL);
		exit(EX_OSERR);
	}
}
