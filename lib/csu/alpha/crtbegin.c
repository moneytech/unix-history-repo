/*-
 * Copyright 1996-1998 John D. Polstra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *      $Id: crtbegin.c,v 1.1.1.1 1998/03/07 20:27:10 jdp Exp $
 */

typedef void (*fptr)(void);

static fptr ctor_list[1] __attribute__((section(".ctors"))) = { (fptr) -1 };
static fptr dtor_list[1] __attribute__((section(".dtors"))) = { (fptr) -1 };

/* Both do_ctors() and do_dtors() live in .text (the default) */
static void
do_ctors(void)
{
    fptr *fpp;

    for(fpp = ctor_list + 1;  *fpp != 0;  ++fpp)
	(**fpp)();
}

static void
do_dtors(void)
{
    fptr *fpp;

    for(fpp = dtor_list + 1;  *fpp != 0;  ++fpp)
	;
    while(--fpp > dtor_list)
	(**fpp)();
}

extern void _init(void) __attribute__((section(".init")));

void
_init(void)
{
	do_ctors();
}

extern void _fini(void) __attribute__((section(".fini")));

void
_fini(void)
{
	do_dtors();
}
