.\" Copyright (c) 1986 Regents of the University of California.
.\" All rights reserved.  The Berkeley software License Agreement
.\" specifies the terms and conditions for redistribution.
.\"
.\"	@(#)0.t	1.2 (Berkeley) %G%
.\"
.rm CM
.TL
A New Virtual Memory Implementation for Berkeley
.UX
.AU
Marshall Kirk McKusick
Michael J. Karels
.AI
Computer Systems Research Group
Computer Science Division
Department of Electrical Engineering and Computer Science
University of California, Berkeley
Berkeley, California  94720
.AB
With the cost per byte of memory approaching that of the cost per byte
for disks, and with file systems increasingly distant from the host
machines, a new approach to the implementation of virtual memory is
necessary. Rather than preallocating swap space which limits the
maximum virtual memory that can be supported to the size of the swap
area, the system should support virtual memory up to the sum of the
sizes of physical memory plus swap space. For systems with a local swap
disk, but remote file systems, it may be useful to use some of the memory
to keep track of the contents of the swap space to avoid multiple fetches
of the same data from the file system.
.PP
The new implementation should also add new functionality.  Processes
should be allowed to have a large sparse address space, to map files
into their address space, to map device memory into their address
space, and to share memory with other processes. The shared address
space may either be obtained by mapping a file into (possibly
different) parts of their address space, or by arranging to share
``anonymous memory'' (that is memory that is zero fill on demand, and
whose contents are lost when the last process unmaps the memory) with
another process as is done in System V.
.PP
One use of shared memory is to provide a high speed
Inter-Process Communication (IPC) mechanism between two or more
cooperating processes. To insure the integrity of data structures
in a shared region, processes must be able to use semaphores to
coordinate their access to these shared structures. In System V,
these semaphores are provided as a set of system calls. Unfortunately,
the use of system calls reduces the throughput of the shared memory
IPC to that of existing IPC mechanisms.  We are proposing a scheme
that places the semaphores in the shared memory segment, so that
machines that have a test-and-set instruction can handle the usual
uncontested lock and unlock without doing a system call. Only in
the unusual case of trying to lock an already locked lock or in
releasing a wanted lock will a system call be required.  The
interface will allow a user level implementation of the System V
semaphore interface on most machines with a much lower runtime cost.
.AE
.LP
.bp
