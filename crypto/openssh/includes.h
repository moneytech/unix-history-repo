/*
 *
 * includes.h
 *
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 *
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * Created: Thu Mar 23 16:29:37 1995 ylo
 *
 * This file includes most of the needed system headers.
 *
 * $FreeBSD$
 */

#ifndef INCLUDES_H
#define INCLUDES_H

#define RCSID(msg) \
static /**/const char *const rcsid[] = { (char *)rcsid, "\100(#)" msg }

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <machine/endian.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <time.h>
#include <paths.h>
#include <dirent.h>

#include "version.h"

/* Define this to be the path of the xauth program. */
#ifndef XAUTH_PATH
#define XAUTH_PATH "/usr/X11R6/bin/xauth"
#endif

/*
 * Define this to use pipes instead of socketpairs for communicating with the
 * client program.  Socketpairs do not seem to work on all systems.
 * Although pipes are bi-directional in FreeBSD, using pipes here will
 * make <stdin> uni-directional !
 */
/* #define USE_PIPES 1 */

#endif				/* INCLUDES_H */
