/*
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <machine/frame.h>

int
cpu_ptrace(struct thread *td, int req, void *addr, int data)
{
	struct trapframe *tf;
	uint64_t *kstack;
	int error;

	error = 0;
	switch (req) {
	case PT_GETKSTACK:
		tf = td->td_frame;
		if (data >= 0 && (data << 3) < tf->tf_special.ndirty) {
			kstack = (uint64_t*)td->td_kstack;
			error = copyout(kstack + data, addr, 8);
		} else
			error = EINVAL;
		break;
	case PT_SETKSTACK:
		tf = td->td_frame;
		if (data >= 0 && (data << 3) < tf->tf_special.ndirty) {
			kstack = (uint64_t*)td->td_kstack;
			error = copyin(addr, kstack + data, 8);
		} else
			error = EINVAL;
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}
