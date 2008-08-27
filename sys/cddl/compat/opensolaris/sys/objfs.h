/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * $FreeBSD$
 */

#ifndef _COMPAT_OPENSOLARIS_SYS_OBJFS_H
#define	_COMPAT_OPENSOLARIS_SYS_OBJFS_H

/*
 * Private data structure found in '.info' section
 */
typedef struct objfs_info {
        int             objfs_info_primary;
} objfs_info_t;


#endif	/* _COMPAT_OPENSOLARIS_SYS_OBJFS_H */
