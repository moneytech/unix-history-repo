#!/bin/sh
#
# Copyright (c) 2005
#	Bill Paul <wpaul@windriver.com>.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by Bill Paul.
# 4. Neither the name of the author nor the names of any co-contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

header () {
clear
echo "	=================================================================="
echo "	------------------ Windows(r) driver converter -------------------"
echo "	=================================================================="
echo ""
}

mainmenu() {
header
echo "	This is script is designed to guide you through the process"
echo "	of converting a Windows(r) binary driver module and .INF"
echo "	specification file into a FreeBSD ELF kernel module for use"
echo "	with the NDIS compatibility system."
echo ""
echo "	The following options are available:"
echo ""
echo "	1] Learn about the NDIS compatibility system"
echo "	2] Convert individual firmware files"
echo "	3] Convert driver"
echo "	4] Exit"
echo ""
echo -n "	Enter your selection here and press return: "
read KEYPRESS
}


help1 () {
header
echo "				General information"
echo ""
echo "	The NDIS compatibility system is designed to let you use Windows(r)"
echo "	binary drivers for networking devices with FreeBSD, in cases where"
echo "	a native FreeBSD driver is not available due to hardware manufacturer"
echo "	oversight or stupidity. NDIS stands for Network Driver Interface"
echo "	Standard, and refers to the programming model used to write Windows(r)"
echo "	network drivers. (These are often called \"NDIS miniport\" drivers.)"
echo ""
echo "	In order to use your network device in NDIS compatibility mode,"
echo "	you need the Windows(r) driver that goes with it. Also, the driver"
echo "	must be compiled for the same architecture as the release of FreeBSD"
echo "	you have installed. At this time, the i386 and amd64 architectures"
echo "	are both supported. Note that you cannot use a Windows/i386 driver"
echo "	with FreeBSD/amd64: you must obtain a Windows/amd64 driver."
echo ""
echo -n "	Press any key to continue... "
read KEYPRESS
}

help2() {
header
echo "				Where to get drivers"
echo ""
echo "	If you purchased your network card separately from your computer,"
echo "	there should have been a driver distribution CD included with the"
echo "	card which contains Windows(r) drivers. The NDIS compatibility"
echo "	system is designed to emulate the NDIS API of a couple of different"
echo "	Windows(r) releases, however it works best with drivers designed"
echo "	for NDIS 5.0 or later. Drivers distributed for Windows 2000 should"
echo "	work, however for best results you should use a driver designed"
echo "	for Windows XP or Windows Server 2003."
echo ""
echo "	If your card was supplied with your computer, or is a built-in device,"
echo "	drivers may have been included on a special driver bundle CD shipped"
echo "	with the computer."
echo ""
echo "	If you don't have a driver CD, you should be able to find a driver"
echo "	kit on the card or computer vendor's web site."
echo ""
echo -n "	Press any key to continue... "
read KEYPRESS
}

help3 () {
header
echo "				What files do I need?"
echo ""
echo "	In most cases, you will need only two files: a .INF file and a .SYS"
echo "	file. The .INF file is a text file used by the Windows(r) installer to"
echo "	perform the driver installation. It contains information that tells"
echo "	the intaller what devices the driver supports and what registry keys"
echo "	should be created to control driver configuration. The .SYS file"
echo "	is the actual driver executable code in Windows(r) Portable Executable"
echo "	(PE) format. Note that sometimes the .INF file is supplied in unicode"
echo "	format. Unicode .INF files must be converted to ASCII form with the"
echo "	iconv(1) utility before this installer script can use them."
echo "	Occasionally, a driver may require firmware or register setup"
echo "	files that are external to the main .SYS file. These are provided"
echo "	on the same CD with the driver itself, and sometimes have a .BIN"
echo "	extension, though they can be named almost anything. You will need"
echo "	these additional files to make your device work with the NDIS"
echo "	compatibility system as well."
echo ""
echo -n "	Press any key to continue... "
read KEYPRESS
}

help4 () {
header
echo "				How does it all work?"
echo ""
echo "	The installer script uses the ndiscvt(1) utility to convert the .INF,"
echo "	.SYS and optional firmware files into a FreeBSD kernel loadable module"
echo "	(.ko) file. This module can be loaded via the kldload(8) utility or"
echo "	loaded automatically via the /boot/loader.conf file. The ndiscvt(1)"
echo "	utility extracts the device ID information and registry key data"
echo "	from the .INF file and converts it into a C header file. It also uses"
echo "	the objcopy(1) utility to convert the .SYS file and optional firmware"
echo "	files into ELF objects. The header file is compiled into a small C"
echo "	stub file which contains a small amount of code to interface with"
echo "	the FreeBSD module system. This stub is linked together with the"
echo "	converted ELF objects to form a FreeBSD kernel module. A static ELF"
echo "	object (.o) file is also created. This file can be linked into a"
echo "	static kernel image for those who want/need a fully linked kernel"
echo "	image (possibly for embedded bootstrap purposes, or just plain old"
echo "	experimentation)."
echo ""
echo -n "	Press any key to continue... "
read KEYPRESS
}

help5 () {
header
echo "				Prerequisites"
echo ""
echo "	Converting a driver requires the following utilities:"
echo ""
echo "	- The FreeBSD C compiler, cc(1) (part of the base install)."
echo "	- The FreeBSD linker, ld(1) (part of the base install)."
echo "	- The objcopy(1) utility (part of the base install)."
echo "	- The ndiscvt(1) utility (part of the base install)."
echo ""
echo "	If your happen to end up with a .INF file that's in unicode format,"
echo "	then you'll also need:"
echo ""
echo "	- The iconv(1) utility."
echo ""
echo "	If you have installed the X Window system or some sort of desktop"
echo "	environment, then iconv(1) should already be present. If not, you"
echo "	will need to install the libiconv package or port."
echo ""
echo -n "	Press any key to continue... "
read KEYPRESS
}

infconv () {
header
echo "			INF file validation"
echo ""
echo ""
echo "	A .INF file is most often provided as an ASCII file, however"
echo "	files with multilanguage support are provided in Unicode format."
echo "	Please type in the path to your .INF file now."
echo ""
echo -n "	> "
read INFPATH
if [ $INFPATH ] && [ -e $INFPATH ]; 
then 
	INFTYPE=`${FILE} ${INFPATH}`

	case ${INFTYPE} in
	*ASCII*)
		echo ""
		echo "	This .INF file appears to be ASCII."
		echo ""
		echo -n "	Press any key to continue... "
		read KEYPRESS
		;;
	*text*)
		echo ""
		echo "	This .INF file appears to be ASCII."
		echo ""
		echo -n "	Press any key to continue... "
		read KEYPRESS
		;;
	*nicode*)
		echo ""
		echo "	This .INF file appears to be Unicode."
		if [ -e $ICONVPATH ];
		then
			echo "	Trying to convert to ASCII..."
			${RM} -f /tmp/ascii.inf
			${ICONVPATH} -f utf-16 -t utf-8 ${INFPATH} > /tmp/ascii.inf
			INFPATH=/tmp/ascii.inf
			echo "	Done."
			echo ""
			echo -n "	Press any key to continue... "
			read KEYPRESS
		else
			echo "	The iconv(1) utility does not appear to be installed."
			echo "	Please install this utility or convert the .INF file"
			echo "	to ASCII and run this utility again."
			echo ""
			exit
		fi
		;;
	*)
		echo ""
		echo "	I don't recognize this file format. It may not be a valid .INF file."
		echo ""
		echo -n "	Press enter to try again, or ^C to quit. "
		read KEYPRESS
		INFPATH=""
		;;
	esac
else
	echo ""
	echo "	The file '$INFPATH' was not found."
	echo ""
	echo -n "	Press enter to try again, or ^C to quit. "
	read KEYPRESS
	INFPATH=""
fi
}

sysconv() {
header
echo "			Driver file validation"
echo ""
echo ""
echo "	Now you need to specify the name of the Windows(r) driver .SYS"
echo "	file for your device. Note that if you are running FreeBSD/amd64,"
echo "	then you must provide a driver that has been compiled for the"
echo "	64-bit Windows(r) platform. If a 64-bit driver is not available"
echo "	for your device, you must install FreeBSD/ia32 and use the"
echo "	32-bit driver instead."
echo ""
echo "	Please type in the path to the Windows(r) driver .SYS file now."
echo ""
echo -n "	> "
read SYSPATH
if [ $SYSPATH ] && [ -e $SYSPATH ];
then
	SYSTYPE=`${FILE} ${SYSPATH}`

	case ${SYSTYPE} in
	*Windows*)
		echo ""
		echo "	This .SYS file appears to be in Windows(r) PE format."
		echo ""
		echo -n "	Press any key to continue... "
		read KEYPRESS
		SYSBASE=`basename ${SYSPATH} | ${TR} '.' '_'`
		;;
	*)
		echo ""
		echo "	I don't recognize this file format. It may not be a valid .SYS file."
		echo ""

		echo -n "	Press enter to try again, or ^C to quit. "
		read KEYPRESS
		SYSPATH=""
		;;
	esac
else
	echo ""
	echo "	The file '$SYSPATH' was not found."
	echo ""
	echo -n "	Press enter to try again, or ^C to quit. "
	read KEYPRESS
	SYSPATH=""
fi 
}

ndiscvt() {
header
echo "			Driver file conversion"
echo ""
echo "	The script will now try to convert the .INF and .SYS files"
echo "	using the ndiscvt(1) utility. This utility can handle most"
echo "	.INF files, however occasionally it can fail to parse some files"
echo "	due to subtle syntax issues: the .INF syntax is very complex,"
echo "	and the Windows(r) parser will sometimes allow files with small"
echo "	syntax errors to be processed correctly which ndiscvt(1) will"
echo "	not. If the conversion fails, you may have to edit the .INF"
echo "	file by hand to remove the offending lines."
echo ""
echo -n "	Press enter to try converting the files now: "
read KEYPRESS
if ! ${NDISCVT} -i ${INFPATH} -s ${SYSPATH} -O -o ${DNAME}.h > /dev/null; then
	echo "CONVERSION FAILED"
	exit
else
	echo ""
	echo "	Conversion was successful."
	echo ""
	echo -n "	Press enter to continue... "
	read KEYPRESS
fi
}

firmcvt() {
	while : ; do
header
echo "			Firmware file conversion"
echo ""
echo "	If your driver uses additional firmware files, please list them"
echo "	below. When you're finished, just press enter to contiue. (If your"
echo "	driver doesn't need any extra firmware files, just press enter"
echo "	to move to the next step.)"
echo ""
		echo -n "	> "
		read FIRMPATH

		if [ $FIRMPATH ] && [ $FIRMPATH != "" ]; then
			if [ ! -e $FIRMPATH ]; then
				echo ""
				echo "	The file '$FIRMPATH' was not found"
				echo ""
				echo -n "	Press enter to try again, or ^C to quit. "
				read KEYPRESS
				continue
			fi
			if ! ${NDISCVT} -f ${FIRMPATH} > /dev/null; then
				echo ""
				echo "CONVERSION FAILED"
			else
				echo ""
				echo "	Conversion was successful."
				echo ""
				FRMBASE=`basename ${FIRMPATH}`
				FRMBASE="${FRMBASE}.o"
				FRMLIST="${FRMLIST} ${FRMBASE}"
			fi
			echo -n "	Press enter to continue... "
			read KEYPRESS
		else
			break
		fi
	done

header
echo ""
echo "	List of files converted firmware files:"
echo ""
for i in $FRMLIST
do
	echo "	"$i
done
echo ""
echo -n "	Press enter to continue... "
read KEYPRESS
}

drvgen () {
header
echo "			Kernel module generation"
echo ""
echo ""
echo "	The script will now try to generate the kernel driver module."
echo "	This is the last step. Once this module is generated, you should"
echo "	be able to load it just like any other FreeBSD driver module."
echo ""
echo "	Press enter to compile the stub module and generate the driver"
echo -n "	module now: "
read KEYPRESS
echo ""
touch bus_if.h
touch device_if.h
echo -n "	Compiling stub... "
if ! ${CC} -D_KERNEL -DDRV_DATA_START=${SYSBASE}_drv_data_start -DDRV_NAME=${SYSBASE} -DDRV_DATA_END=${SYSBASE}_drv_data_end -I. ${STUBFILE} -c -o windrv_stub.o; then
	echo "compilation failed. Exiting."
	echo ""
	exit
else
	echo "done."
fi
echo -n	"	Linking loadable kernel module... "
if ! ${LD} -Bshareable  -d -warn-common -o ${SYSBASE}.ko windrv_stub.o ${FRMLIST} ${DNAME}.o; then
	echo "linking failed. Exiting."
	echo ""
	exit
else
	echo "done."
fi
echo -n	"	Linking static kernel module... "
if ! ${LD} -r  -d -warn-common -o ${SYSBASE}.o windrv_stub.o ${FRMLIST} ${DNAME}.o; then
	echo "linking failed. Exiting."
	echo ""
	exit
else
	echo "done."
fi
echo -n "	Cleaning up... "
${RM} -f bus_if.h device_if.h windrv_stub.o
${RM} -f ${DNAME}.h ${DNAME}.o
echo "done."
echo ""
echo "	The file $SYSBASE.ko has been successfully generated."
echo "	You can kldload this module to get started."
echo ""
echo -n "	Press any key to exit. "
read KEYPRESS
echo ""
echo ""
}

convert_driver () {
	while : ; do
		infconv
		if [ $INFPATH ] && [ $INFPATH != "" ]; then
			break
		fi
	done

	while : ; do
		sysconv
		if [ $SYSPATH ] && [ $SYSPATH != "" ]; then
			break
		fi
	done

	ndiscvt
	firmcvt
	drvgen
}

ICONVPATH=/usr/local/bin/iconv
NDISCVT=/usr/sbin/ndiscvt
STUBFILE=/usr/share/misc/windrv_stub.c
DNAME=windrv
OBJCOPY=/usr/bin/objcopy
CC=/usr/bin/cc
LD=/usr/bin/ld
RM=/bin/rm
TR=/usr/bin/tr
FILE=/usr/bin/file

INFPATH=""
FRMLIST=""
SYSPATH=""
SYSBASE=""
FRMBASE=""

while : ; do
	mainmenu
	case ${KEYPRESS} in
	1)
		help1
		help2
		help3
		help4
		help5
		;;
	2)
		firmcvt
		;;
	3)
		convert_driver
		;;
	4)
		header
		echo ""
		echo "	Be seeing you!"
		echo ""
		exit
		;;
	*)
		header
		echo ""
		echo -n "	Sorry, I didn't underatand that. Press enter to try again: "
		read KEYPRESS
		;;
	esac
done
