#!/stand/sh
#
# bininst - perform the last stage of installation by somehow getting
# a bindist onto the user's disk and unpacking it.  The name bininst
# is actually something of a misnomer, since this utility will install
# more than just the bindist set.
#
# Written:  November 11th, 1994
# Copyright (C) 1994 by Jordan K. Hubbard
#
# Permission to copy or use this software for any purpose is granted
# provided that this message stay intact, and at this location (e.g. no
# putting your name on top after doing something trivial like reindenting
# it, just to make it look like you wrote it!).
#
# $Id: bininst,v 1.56 1995/01/12 16:18:16 jkh Exp $

if [ "${_BININST_LOADED_}" = "yes" ]; then
	error "Error, $0 loaded more than once!"
	return 1
else
	_BININST_LOADED_=yes
fi

# Grab the miscellaneous functions.
. /stand/miscfuncs.sh

# Grab the installation routines
. /stand/instdist.sh

# Grab the network setup routines
. /stand/netinst.sh

# Deal with trigger-happy users.
trap interrupt 1 2 15

# set initial defaults
set_defaults()
{
	network_set_defaults
	media_set_defaults
	INSTALLING="yes"
	mkdir -p ${TMP}
	cp /stand/etc/* /etc
}

# Print welcome banner.
welcome()
{
	dialog --title "Welcome to FreeBSD!" --msgbox \
"Installation may now proceed from tape, CDROM, DOS (floppy or existing
hard disk partition), or a network connection (SLIP, ethernet or parallel
port for FTP or NFS).  Please remove the cpio floppy from the
drive and press return to continue." -1 -1
}

do_last_config()
{
	sh /stand/setup.sh
	dialog --title "Auf Wiedersehen!" --msgbox \
"Don't forget that the login name \"root\" has no password.
If you didn't create any users with adduser, you can at least log in
as this user.  Also be aware that root is the _superuser_, which means
that you can easily wipe out your system if you're not careful!

Further information may be obtained by sending mail to
questions@freebsd.org (though please read the docs first,
we get LOTS of questions! :-) or browsing http://www.freebsd.org/

We sincerely hope you enjoy FreeBSD 2.0!

		The FreeBSD Project Team" -1 -1
}

welcome
set_defaults

while [ "${INSTALLING}" = "yes" ]; do
	if media_select_distribution; then
		if media_chose; then
			for xx in ${MEDIA_DISTRIBUTIONS}; do
				MEDIA_DISTRIBUTION=`eval echo \`echo $xx\``
				media_install_set
			done
		fi
	else
		do_last_config
		INSTALLING="no"
	fi
done
echo; echo "Spawning shell.  Exit shell to continue with new bindist."
echo "Progress <installation completed>" > /dev/ttyv1
/stand/sh
exit 20
