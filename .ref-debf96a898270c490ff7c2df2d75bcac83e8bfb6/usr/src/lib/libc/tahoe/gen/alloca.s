/*
 * Copyright (c) 1988 Regents of the University of California.
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
 *
 * This code is derived from software contributed to Berkeley by
 * Computer Consoles Inc.
 */

#if defined(LIBC_SCCS) && !defined(lint)
	.asciz "@(#)alloca.s	1.3 (Berkeley) %G%"
#endif /* LIBC_SCCS and not lint */

/* like alloc, but automatic automatic free in return */

#include "DEFS.h"

ENTRY(alloca, 0)
	moval	(sp),r0		# current sp
	subl2	4(fp),r0	# allocation size
	andl2	$0xfffffffc,r0	# allignment
	movl	-8(fp),r1	# old pc
	movl	(fp),fp		# old fp
	addl2	$4*4,r0		# reuse space of mscp
	movl	r0,sp		# new sp
	jmp 	(r1)		# funny return
