#ifndef lint
static char sccsid[] = "@(#)shell.c	4.2	(Berkeley)	%G%";
#endif not lint

/*
**  CALL THE SHELL
*/

shell()
{
	int		i;
	register int	pid;
	register int	sav2, sav3;

	if (!(pid = fork()))
	{
		setuid(getuid());
		nice(0);
		execl("/bin/csh", "-", 0);
		syserr("cannot execute /bin/csh");
	}
	sav2 = signal(2, 1);
	sav3 = signal(3, 1);
	while (wait(&i) != pid) ;
	signal(2, sav2);
	signal(3, sav3);
}
