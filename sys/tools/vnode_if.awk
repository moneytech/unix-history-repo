#!/usr/bin/awk -f

#-
# Copyright (c) 1992, 1993
#	The Regents of the University of California.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

#
#	@(#)vnode_if.sh	8.1 (Berkeley) 6/10/93
# $FreeBSD$
#
# Script to produce VFS front-end sugar.
#
# usage: vnode_if.awk <srcfile> [-c | -h]
#	(where <srcfile> is currently /sys/kern/vnode_if.src)
#

function usage()
{
	print "usage: vnode_if.awk <srcfile> [-c|-h|-p|-q]";
	exit 1;
}

function die(msg, what)
{
	printf msg "\n", what > "/dev/stderr";
	exit 1;
}

function t_spc(type)
{
	# Append a space if the type is not a pointer
	return (type ~ /\*$/) ? type : type " ";
}

# These are just for convenience ...
function printc(s) {print s > cfile;}
function printh(s) {print s > hfile;}
function printp(s) {print s > pfile;}
function printq(s) {print s > qfile;}

function add_debug_code(name, arg, pos, ind)
{
	if (arg == "vpp")
		star = "*";
	else
		star = "";
	if (lockdata[name, arg, pos] && (lockdata[name, arg, pos] != "-")) {
		if (arg ~ /^\*/) {
			printc(ind"if ("substr(arg, 2)" != NULL) {");
		}
		printc(ind"ASSERT_VI_UNLOCKED("star"a->a_"arg", \""uname"\");");
		# Add assertions for locking
		if (lockdata[name, arg, pos] == "L")
			printc(ind"ASSERT_VOP_LOCKED(" star "a->a_"arg", \""uname"\");");
		else if (lockdata[name, arg, pos] == "U")
			printc(ind"ASSERT_VOP_UNLOCKED(" star "a->a_"arg", \""uname"\");");
		else if (0) {
			# XXX More checks!
		}
		if (arg ~ /^\*/) {
			printc("ind}");
		}
	}
}

function add_debug_pre(name)
{
	if (lockdata[name, "pre"]) {
		printc("#ifdef	DEBUG_VFS_LOCKS");
		printc("\t"lockdata[name, "pre"]"(a);");
		printc("#endif");
	}
}

function add_debug_post(name)
{
	if (lockdata[name, "post"]) {
		printc("#ifdef	DEBUG_VFS_LOCKS");
		printc("\t"lockdata[name, "post"]"(a, rc);");
		printc("#endif");
	}
}

function find_arg_with_type (type)
{
	for (jj = 0; jj < numargs; jj++) {
		if (types[jj] == type) {
			return "VOPARG_OFFSETOF(struct " \
			    name "_args,a_" args[jj] ")";
		}
	}

	return "VDESC_NO_OFFSET";
}

BEGIN{

# Process the command line
for (i = 1; i < ARGC; i++) {
	arg = ARGV[i];
	if (arg !~ /^-[chpq]+$/ && arg !~ /\.src$/)
		usage();
	if (arg ~ /^-.*c/)
		cfile = "vnode_if.c";
	if (arg ~ /^-.*h/)
		hfile = "vnode_if.h";
	if (arg ~ /^-.*p/)
		pfile = "vnode_if_newproto.h";
	if (arg ~ /^-.*q/)
		qfile = "vnode_if_typedef.h";
	if (arg ~ /\.src$/)
		srcfile = arg;
}
ARGC = 1;

if (!cfile && !hfile && !pfile && !qfile)
	exit 0;

if (!srcfile)
	usage();

common_head = \
    "/*\n" \
    " * This file is produced automatically.\n" \
    " * Do not modify anything in here by hand.\n" \
    " *\n" \
    " * Created from $FreeBSD$\n" \
    " */\n" \
    "\n";

if (pfile) {
	printp(common_head)
	printp("struct vop_vector {")
	printp("\tstruct vop_vector\t*vop_default;")
	printp("\tvop_bypass_t\t*vop_bypass;")
}

if (qfile) {
	printq(common_head)
}

if (hfile) {
	printh(common_head "extern struct vnodeop_desc vop_default_desc;");
	printh("#include \"vnode_if_typedef.h\"")
	printh("#include \"vnode_if_newproto.h\"")
}

if (cfile) {
	printc(common_head \
	    "#include <sys/param.h>\n" \
	    "#include <sys/systm.h>\n" \
	    "#include <sys/vnode.h>\n" \
	    "\n" \
	    "struct vnodeop_desc vop_default_desc = {\n" \
	    "	\"default\",\n" \
	    "	0,\n" \
	    "	(void *)(uintptr_t)vop_panic,\n" \
	    "	NULL,\n" \
	    "	VDESC_NO_OFFSET,\n" \
	    "	VDESC_NO_OFFSET,\n" \
	    "	VDESC_NO_OFFSET,\n" \
	    "	VDESC_NO_OFFSET,\n" \
	    "	NULL,\n" \
	    "};\n");
}

while ((getline < srcfile) > 0) {
	if (NF == 0)
		continue;
	if ($1 ~ /^#%/) {
		if (NF != 6  ||  $1 != "#%"  || \
		    $2 !~ /^[a-z]+$/  ||  $3 !~ /^[a-z]+$/  || \
		    $4 !~ /^.$/  ||  $5 !~ /^.$/  ||  $6 !~ /^.$/)
			continue;
		lockdata["vop_" $2, $3, "Entry"] = $4;
		lockdata["vop_" $2, $3, "OK"]    = $5;
		lockdata["vop_" $2, $3, "Error"] = $6;			
		continue;
	}

	if ($1 ~ /^#!/) {
		if (NF != 4 || $1 != "#!")
			continue;
		if ($3 != "pre" && $3 != "post")
			continue;
		lockdata["vop_" $2, $3] = $4;
		continue;
	}
	if ($1 ~ /^#/)
		continue;

	# Get the function name.
	name = $1;
	uname = toupper(name);

	# Start constructing a ktrpoint string
	ctrstr = "\"" uname;
	# Get the function arguments.
	for (numargs = 0; ; ++numargs) {
		if ((getline < srcfile) <= 0) {
			die("Unable to read through the arguments for \"%s\"",
			    name);
		}
		if ($1 ~ /^\};/)
			break;

		# Delete comments, if any.
		gsub (/\/\*.*\*\//, "");

		# Condense whitespace and delete leading/trailing space.
		gsub(/[[:space:]]+/, " ");
		sub(/^ /, "");
		sub(/ $/, "");

		# Pick off direction.
		if ($1 != "INOUT" && $1 != "IN" && $1 != "OUT")
			die("No IN/OUT direction for \"%s\".", $0);
		dirs[numargs] = $1;
		sub(/^[A-Z]* /, "");

		if ((reles[numargs] = $1) == "WILLRELE")
			sub(/^[A-Z]* /, "");
		else
			reles[numargs] = "WONTRELE";

		# kill trailing ;
		if (sub(/;$/, "") < 1)
			die("Missing end-of-line ; in \"%s\".", $0);

		# pick off variable name
		if ((argp = match($0, /[A-Za-z0-9_]+$/)) < 1)
			die("Missing var name \"a_foo\" in \"%s\".", $0);
		args[numargs] = substr($0, argp);
		$0 = substr($0, 1, argp - 1);

		# what is left must be type
		# remove trailing space (if any)
		sub(/ $/, "");
		types[numargs] = $0;

		# We can do a maximum of 6 arguments to CTR*
		if (numargs <= 6) {
			if (numargs == 0)
				ctrstr = ctrstr "(" args[numargs];
			else
				ctrstr = ctrstr ", " args[numargs];
			if (types[numargs] ~ /\*/)
				ctrstr = ctrstr " 0x%lX";
			else
				ctrstr = ctrstr " %ld";
		}
	}
	if (numargs > 6)
		ctrargs = 6;
	else
		ctrargs = numargs;
	ctrstr = "\tCTR" ctrargs "(KTR_VOP,\n\t    " ctrstr ")\",\n\t    ";
	ctrstr = ctrstr "a->a_" args[0];
	for (i = 1; i < ctrargs; ++i)
		ctrstr = ctrstr ", a->a_" args[i];
	ctrstr = ctrstr ");";

	if (pfile) {
		printp("\t"name"_t\t*"name";")
	}
	if (qfile) {
		printq("struct "name"_args;")
		printq("typedef int "name"_t(struct "name"_args *);\n")
	}

	if (hfile) {
		# Print out the vop_F_args structure.
		printh("struct "name"_args {\n\tstruct vop_generic_args a_gen;");
		for (i = 0; i < numargs; ++i)
			printh("\t" t_spc(types[i]) "a_" args[i] ";");
		printh("};");
		printh("");

		# Print out extern declaration.
		printh("extern struct vnodeop_desc " name "_desc;");
		printh("");

		# Print out function prototypes.
		printh("int " uname "_AP(struct " name "_args *);");
		printh("");
		printh("static __inline int " uname "(");
		for (i = 0; i < numargs; ++i) {
			printh("\t" t_spc(types[i]) args[i] \
			    (i < numargs - 1 ? "," : ")"));
		}
		printh("{");
		printh("\tstruct " name "_args a;");
		printh("");
		printh("\ta.a_gen.a_desc = &" name "_desc;");
		for (i = 0; i < numargs; ++i)
			printh("\ta.a_" args[i] " = " args[i] ";");
		printh("\treturn (" uname "_AP(&a));");
		printh("}");

		printh("");
	}

	if (cfile) {
		# Print out the vop_F_vp_offsets structure.  This all depends
		# on naming conventions and nothing else.
		printc("static int " name "_vp_offsets[] = {");
		# as a side effect, figure out the releflags
		releflags = "";
		vpnum = 0;
		for (i = 0; i < numargs; i++) {
			if (types[i] == "struct vnode *") {
				printc("\tVOPARG_OFFSETOF(struct " name \
				    "_args,a_" args[i] "),");
				if (reles[i] == "WILLRELE") {
					releflags = releflags \
					    "|VDESC_VP" vpnum "_WILLRELE";
				}
				vpnum++;
			}
		}

		sub(/^\|/, "", releflags);
		printc("\tVDESC_NO_OFFSET");
		printc("};");

		# Print out function.
		printc("\nint\n" uname "_AP(struct " name "_args *a)");
		printc("{");
		printc("\tint rc;");
		printc("\tstruct vnode *vp = a->a_" args[0]";");
		printc("\tstruct vop_vector *vop = vp->v_op;");
		printc("");
		printc("\tKASSERT(a->a_gen.a_desc == &" name "_desc,");
		printc("\t    (\"Wrong a_desc in " name "(%p, %p)\", vp, a));");
		printc("\twhile(vop != NULL && \\");
		printc("\t    vop->"name" == NULL && vop->vop_bypass == NULL)")
		printc("\t\tvop = vop->vop_default;")
		printc("\tKASSERT(vop != NULL, (\"No "name"(%p, %p)\", vp, a));")
		for (i = 0; i < numargs; ++i)
			add_debug_code(name, args[i], "Entry", "\t");
		add_debug_pre(name);
		printc("\tif (vop->"name" != NULL)")
		printc("\t\trc = vop->"name"(a);")
		printc("\telse")
		printc("\t\trc = vop->vop_bypass(&a->a_gen);")
		printc(ctrstr);
		printc("\tif (rc == 0) {");
		for (i = 0; i < numargs; ++i)
			add_debug_code(name, args[i], "OK", "\t\t");
		printc("\t} else {");
		for (i = 0; i < numargs; ++i)
			add_debug_code(name, args[i], "Error", "\t\t");
		printc("\t}");
		add_debug_post(name);
		printc("\treturn (rc);");
		printc("}\n");

		# Print out the vnodeop_desc structure.
		printc("struct vnodeop_desc " name "_desc = {");
		# printable name
		printc("\t\"" name "\",");
		# flags
		vppwillrele = "";
		for (i = 0; i < numargs; i++) {
			if (types[i] == "struct vnode **" && \
			    reles[i] == "WILLRELE") {
				vppwillrele = "|VDESC_VPP_WILLRELE";
			}
		}

		if (!releflags)
			releflags = "0";
		printc("\t" releflags vppwillrele ",");

		# function to call
		printc("\t(void*)(uintptr_t)" uname "_AP,");
		# vp offsets
		printc("\t" name "_vp_offsets,");
		# vpp (if any)
		printc("\t" find_arg_with_type("struct vnode **") ",");
		# cred (if any)
		printc("\t" find_arg_with_type("struct ucred *") ",");
		# thread (if any)
		printc("\t" find_arg_with_type("struct thread *") ",");
		# componentname
		printc("\t" find_arg_with_type("struct componentname *") ",");
		# transport layer information
		printc("\tNULL,\n};\n");
	}
}
 
if (pfile)
	printp("};")
 
if (hfile)
	close(hfile);
if (cfile)
	close(cfile);
if (pfile)
	close(pfile);
close(srcfile);

exit 0;

}
