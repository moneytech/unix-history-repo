#
# Copyright (c) 1985 Regents of the University of California.
# All rights reserved.
#
# %sccs.include.redist.sh%
#
#	@(#)mcount.sed	5.4 (Berkeley) %G%
#

s/.word	0x.*$/&\
	.data\
	.align 2\
9:	.long 0\
	.text\
	pushal	9b\
	callf	\$8,mcount/
