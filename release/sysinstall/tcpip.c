/*
 * $Id: tcpip.c,v 1.29.2.2 1995/05/31 10:17:45 jkh Exp $
 *
 * Copyright (c) 1995
 *      Gary J Palmer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gary J Palmer
 *	for the FreeBSD Project.
 * 4. The name of Gary J Palmer or the FreeBSD Project may
 *    not be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GARY J PALMER ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL GARY J PALMER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * All kinds of hacking also performed by jkh on this code.  Don't
 * blame Gary for every bogosity you see here.. :-)
 *
 * -jkh
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>
#include <dialog.h>
#include "ui_objects.h"
#include "dir.h"
#include "dialog.priv.h"
#include "colors.h"
#include "rc.h"
#include "sysinstall.h"

/* These are nasty, but they make the layout structure a lot easier ... */

static char		hostname[HOSTNAME_FIELD_LEN], domainname[HOSTNAME_FIELD_LEN],
			gateway[IPADDR_FIELD_LEN], nameserver[IPADDR_FIELD_LEN];
static int		okbutton, cancelbutton;
static char		ipaddr[IPADDR_FIELD_LEN], netmask[IPADDR_FIELD_LEN], extras[EXTRAS_FIELD_LEN];

/* What the screen size is meant to be */
#define TCP_DIALOG_Y		0
#define TCP_DIALOG_X		8
#define TCP_DIALOG_WIDTH	COLS - 16
#define TCP_DIALOG_HEIGHT	LINES - 2

/* The screen layout structure */
typedef struct _layout {
    int		y;		/* x & Y co-ordinates */
    int		x;
    int		len;		/* The size of the dialog on the screen */
    int		maxlen;		/* How much the user can type in ... */
    char	*prompt;	/* The string for the prompt */
    char	*help;		/* The display for the help line */
    void	*var;		/* The var to set when this changes */
    int		type;		/* The type of the dialog to create */
    void	*obj;		/* The obj pointer returned by libdialog */
} Layout;

static Layout layout[] = {
{ 1, 2, 25, HOSTNAME_FIELD_LEN - 1,
      "Host name:", "The name of your machine on a network, e.g. foo.bar.com",
      hostname, STRINGOBJ, NULL },
#define LAYOUT_HOSTNAME		0
{ 1, 35, 20, HOSTNAME_FIELD_LEN - 1,
      "Domain name:",
      "The name of the domain that your machine is in, e.g. bar.com",
      domainname, STRINGOBJ, NULL },
#define LAYOUT_DOMAINNAME	1
{ 5, 2, 18, IPADDR_FIELD_LEN - 1,
      "Gateway:",
      "IP address of host forwarding packets to non-local destinations",
      gateway, STRINGOBJ, NULL },
#define LAYOUT_GATEWAY		2
{ 5, 35, 18, IPADDR_FIELD_LEN - 1,
      "Name server:", "IP address of your local DNS server",
      nameserver, STRINGOBJ, NULL },
#define LAYOUT_NAMESERVER	3
{ 10, 10, 18, IPADDR_FIELD_LEN - 1,
      "IP Address:",
      "The IP address to be used for this interface",
      ipaddr, STRINGOBJ, NULL },
#define LAYOUT_IPADDR		5
{ 10, 35, 18, IPADDR_FIELD_LEN - 1,
      "Netmask:",
      "The netmask for this interfaace, e.g. 0xffffff00 for a class C network",
      netmask, STRINGOBJ, NULL },
#define LAYOUT_NETMASK		6
{ 14, 10, 37, HOSTNAME_FIELD_LEN - 1,
      "Extra options to ifconfig:",
      "Any interface-specific options to ifconfig you would like to use",
      extras, STRINGOBJ, NULL },
#define LAYOUT_EXTRAS		7
{ 19, 15, 0, 0,
      "OK", "Select this if you are happy with these settings",
      &okbutton, BUTTONOBJ, NULL },
#define LAYOUT_OKBUTTON		8
{ 19, 35, 0, 0,
      "CANCEL", "Select this if you wish to cancel this screen",
      &cancelbutton, BUTTONOBJ, NULL },
#define LAYOUT_CANCELBUTTON	9
{ NULL },
};

#define _validByte(b) ((b) >= 0 && (b) < 255)

/* whine */
static void
feepout(char *msg)
{
    beep();
    dialog_notify(msg);
}

/* Very basic IP address integrity check - could be drastically improved */
static int
verifyIP(char *ip)
{
    int a, b, c, d;

    if (ip && sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 &&
	_validByte(a) && _validByte(b) && _validByte(c) &&
	_validByte(d))
	return 1;
    else
	return 0;
}

/* Check for the settings on the screen - the per interface stuff is
   moved to the main handling code now to do it on the fly - sigh */

static int
verifySettings(void)
{
    if (!hostname[0])
	feepout("Must specify a host name of some sort!");
    else if (gateway[0] && !verifyIP(gateway))
	feepout("Invalid gateway IP address specified");
    else if (nameserver[0] && !verifyIP(nameserver))
	feepout("Invalid name server IP address specified");
    else if (netmask[0] && (netmask[0] < '0' && netmask[0] > '3'))
	feepout("Invalid netmask value");
    else if (ipaddr[0] && !verifyIP(ipaddr))
	feepout("Invalid IP address");
    else
	return 1;
    return 0;
}

/* This is it - how to get TCP setup values */
int
tcpOpenDialog(Device *devp)
{
    WINDOW              *ds_win;
    ComposeObj          *obj = NULL;
    ComposeObj		*first, *last;
    int                 n=0, quit=FALSE, cancel=FALSE, ret;
    int			max;
    char                *tmp;
    char		help[FILENAME_MAX];
    char		title[80];

    /* We need a curses window */
    ds_win = newwin(LINES, COLS, 0, 0);
    if (ds_win == 0)
	msgFatal("Cannot open TCP/IP dialog window!!");

    /* Say where our help comes from */
    systemHelpFile(TCP_HELPFILE, help);
    use_helpfile(help);

    /* Setup a nice screen for us to splat stuff onto */
    draw_box(ds_win, TCP_DIALOG_Y, TCP_DIALOG_X, TCP_DIALOG_HEIGHT, TCP_DIALOG_WIDTH, dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, TCP_DIALOG_Y, TCP_DIALOG_X + 20, " Network Configuration ");
    draw_box(ds_win, TCP_DIALOG_Y + 9, TCP_DIALOG_X + 8, TCP_DIALOG_HEIGHT - 13, TCP_DIALOG_WIDTH - 17,
	     dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    sprintf(title, " Configuration for Interface %s ", devp->name);
    mvwaddstr(ds_win, TCP_DIALOG_Y + 9, TCP_DIALOG_X + 14, title);

    /* Initialise vars from previous device values */
    if (devp->private) {
	DevInfo *di = (DevInfo *)devp->private;

	strcpy(ipaddr, di->ipaddr);
	strcpy(netmask, di->netmask);
	strcpy(extras, di->extras);
    }
    else
	ipaddr[0] = netmask[0] = extras[0] = '\0';

    /* Look up values already recorded with the system, or blank the string variables ready to accept some new data */
    tmp = getenv(VAR_HOSTNAME);
    if (tmp)
	strcpy(hostname, tmp);
    else
	bzero(hostname, sizeof(hostname));
    tmp = getenv(VAR_DOMAINNAME);
    if (tmp)
	strcpy(domainname, tmp);
    else
	bzero(domainname, sizeof(domainname));
    tmp = getenv(VAR_GATEWAY);
    if (tmp)
	strcpy(gateway, tmp);
    else
	bzero(gateway, sizeof(gateway));
    tmp = getenv(VAR_NAMESERVER);
    if (tmp)
	strcpy(nameserver, tmp);
    else
	bzero(nameserver, sizeof(nameserver));

    /* Loop over the layout list, create the objects, and add them
       onto the chain of objects that dialog uses for traversal*/
    n = 0;
#define lt layout[n]
    while (lt.help != NULL) {
	switch (lt.type) {
	case STRINGOBJ:
	    lt.obj = NewStringObj(ds_win, lt.prompt, lt.var,
				  lt.y + TCP_DIALOG_Y, lt.x + TCP_DIALOG_X,
				  lt.len, lt.maxlen);
	    break;

	case BUTTONOBJ:
	    lt.obj = NewButtonObj(ds_win, lt.prompt, lt.var,
				  lt.y + TCP_DIALOG_Y, lt.x + TCP_DIALOG_X);
	    break;

	default:
	    msgFatal("Don't support this object yet!");
	}
	AddObj(&obj, lt.type, (void *) lt.obj);
	n++;
    }
    max = n - 1;

    /* Find the last object we can traverse to */
    last = obj;
    while (last->next)
	last = last->next;

    /* Find the first object in the list */
    first = obj;
    while (first->prev)
	first = first->prev;

    /* Some more initialisation before we go into the main input loop */
    n = 0;
    cancelbutton = okbutton = 0;

    /* Incoming user data - DUCK! */
    while (!quit) {
	char help_line[80];
	int i, len = strlen(lt.help);

	/* Display the help line at the bottom of the screen */
	for (i = 0; i < 79; i++)
	    help_line[i] = (i < len) ? lt.help[i] : ' ';
	help_line[i] = '\0';
	use_helpline(help_line);
	display_helpline(ds_win, LINES - 1, COLS - 1);

	/* Ask for libdialog to do its stuff */
	ret = PollObj(&obj);

	/* We are in the Hostname field - calculate the domainname */
	if (n == 0) {
	    if ((tmp = index(hostname, '.')) != NULL) {
		strncpy(domainname, tmp + 1, strlen(tmp + 1));
		domainname[strlen(tmp+1)] = '\0';
		RefreshStringObj(layout[1].obj);
	    }
	}

	/* Handle special case stuff that libdialog misses. Sigh */
	switch (ret) {
	    /* Bail out */
	case SEL_ESC:
	    quit = TRUE, cancel=TRUE;
	    break;

	    /* This doesn't work for list dialogs. Oh well. Perhaps
	       should special case the move from the OK button ``up''
	       to make it go to the interface list, but then it gets
	       awkward for the user to go back and correct screw up's
	       in the per-interface section */

	case KEY_UP:
	    if (obj->prev !=NULL ) {
		obj = obj->prev;
		--n;
	    } else {
		obj = last;
		n = max;
	    }
	    break;

	case KEY_DOWN:
	    if (obj->next != NULL) {
		obj = obj->next;
		++n;
	    } else {
		obj = first;
		n = 0;
	    }
	    break;

	case SEL_TAB:
	    if (n < max)
		++n;
	    else
		n = 0;
	    break;

	    /* The user has pressed enter over a button object */
	case SEL_BUTTON:
 	    if (cancelbutton)
		cancel = TRUE, quit = TRUE;
	    else {
		if (verifySettings())
		    quit = TRUE;
	    }
	    break;

	    /* Generic CR handler */
	case SEL_CR:
	    if (n < max)
		++n;
	    else
		n = 0;
	    break;

	case SEL_BACKTAB:
	    if (n)
		--n;
	    else
		n = max;
	    break;

	case KEY_F(1):
	    display_helpfile();

	    /* They tried some key combination we don't support - tell them! */
	default:
	    beep();
	}

	/* BODGE ALERT! */
	if (((tmp = index(hostname, '.')) != NULL) && (strlen(domainname)==0)) {
	    strncpy(domainname, tmp + 1, strlen(tmp + 1));
	    domainname[strlen(tmp+1)] = '\0';
	    RefreshStringObj(layout[1].obj);
	}
    }

    /* Clear this crap off the screen */
    dialog_clear();
    refresh();
    use_helpfile(NULL);

    /* We actually need to inform the rest of sysinstall about this
       data now - if the user hasn't selected cancel, save the stuff
       out to the environment via the variable_set layers */

    if (!cancel) {
	DevInfo *di;
	char temp[512], ifn[64];

	variable_set2(VAR_HOSTNAME, hostname);
	variable_set2(VAR_DOMAINNAME, domainname);
	if (gateway[0])
	    variable_set2(VAR_GATEWAY, gateway);
	if (nameserver[0])
	    variable_set2(VAR_NAMESERVER, nameserver);

	if (!devp->private)
	    devp->private = (DevInfo *)malloc(sizeof(DevInfo));
	di = devp->private;
	strcpy(di->ipaddr, ipaddr);
	strcpy(di->netmask, netmask);
	strcpy(di->extras, extras);

	sprintf(temp, "inet %s %s netmask %s", ipaddr, extras, netmask);
	sprintf(ifn, "%s%s", VAR_IFCONFIG, devp->name);
	variable_set2(ifn, temp);
	if (!getenv(VAR_INTERFACES))
	    variable_set2(VAR_INTERFACES, "lo0");
	sprintf(ifn, "%s %s", devp->name, getenv(VAR_INTERFACES));
	variable_set2(VAR_INTERFACES, ifn);
	if (ipaddr[0])
	    variable_set2(VAR_IPADDR, ipaddr);
	return 0;
    }
    return 1;
}

static int
netHook(char *str)
{
    Device **devs;

    /* Clip garbage off the ends */
    string_prune(str);
    str = string_skipwhite(str);
    if (!*str)
	return 0;
    devs = deviceFind(str, DEVICE_TYPE_NETWORK);
    if (devs) {
	tcpOpenDialog(devs[0]);
	mediaDevice = devs[0];
    }
    return devs ? 1 : 0;
}

/* Get a network device */
Boolean
tcpDeviceSelect(void)
{
    DMenu *menu;
    Device **devs;
    int cnt;
    int status;

    devs = deviceFind(NULL, DEVICE_TYPE_NETWORK);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No network devices available!");
	status = FALSE;
    }
    else if (cnt == 1) {
	mediaDevice = devs[0];
	status = TRUE;
    }
    else {

	menu = deviceCreateMenu(&MenuNetworkDevice, DEVICE_TYPE_NETWORK, netHook);
	if (!menu)
	    msgFatal("Unable to create network device menu!  Argh!");
	status = dmenuOpenSimple(menu);
	free(menu);
    }
    return status;
}

/* Do it from a menu that doesn't care about status */
int
tcpMenuSelect(char *str)
{
    (void)tcpDeviceSelect();
    return 0;
}
