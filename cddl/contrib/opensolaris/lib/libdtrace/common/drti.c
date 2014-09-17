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
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Copyright 2013 Voxer Inc. All rights reserved.
 * Use is subject to license terms.
 */

#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <link.h>
#include <sys/dtrace.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libelf.h>
#include <gelf.h>

/*
 * In Solaris 10 GA, the only mechanism for communicating helper information
 * is through the DTrace helper pseudo-device node in /devices; there is
 * no /dev link. Because of this, USDT providers and helper actions don't
 * work inside of non-global zones. This issue was addressed by adding
 * the /dev and having this initialization code use that /dev link. If the
 * /dev link doesn't exist it falls back to looking for the /devices node
 * as this code may be embedded in a binary which runs on Solaris 10 GA.
 *
 * Users may set the following environment variable to affect the way
 * helper initialization takes place:
 *
 *	DTRACE_DOF_INIT_DEBUG		enable debugging output
 *	DTRACE_DOF_INIT_DISABLE		disable helper loading
 *	DTRACE_DOF_INIT_DEVNAME		set the path to the helper node
 */

static const char *devnamep = "/dev/dtrace/helper";
#if defined(sun)
static const char *olddevname = "/devices/pseudo/dtrace@0:helper";
#endif

static const char *modname;	/* Name of this load object */
static int gen;			/* DOF helper generation */
#if defined(sun)
extern dof_hdr_t __SUNW_dof;	/* DOF defined in the .SUNW_dof section */
#endif
static boolean_t dof_init_debug = B_FALSE;	/* From DTRACE_DOF_INIT_DEBUG */

static void
dprintf(int debug, const char *fmt, ...)
{
	va_list ap;

	if (debug && !dof_init_debug)
		return;

	va_start(ap, fmt);

	if (modname == NULL)
		(void) fprintf(stderr, "dtrace DOF: ");
	else
		(void) fprintf(stderr, "dtrace DOF %s: ", modname);

	(void) vfprintf(stderr, fmt, ap);

	if (fmt[strlen(fmt) - 1] != '\n')
		(void) fprintf(stderr, ": %s\n", strerror(errno));

	va_end(ap);
}

#if defined(sun)
#pragma init(dtrace_dof_init)
#else
static void dtrace_dof_init(void) __attribute__ ((constructor));
#endif

static void
dtrace_dof_init(void)
{
#if defined(sun)
	dof_hdr_t *dof = &__SUNW_dof;
#else
	dof_hdr_t *dof = NULL;
#endif
#ifdef _LP64
	Elf64_Ehdr *elf;
#else
	Elf32_Ehdr *elf;
#endif
	dof_helper_t dh;
	Link_map *lmp;
#if defined(sun)
	Lmid_t lmid;
#else
	u_long lmid = 0;
#endif
	int fd;
	const char *p;
#if !defined(sun)
	Elf *e;
	Elf_Scn *scn = NULL;
	Elf_Data *symtabdata = NULL, *dynsymdata = NULL, *dofdata = NULL;
	dof_hdr_t *dof_next = NULL;
	GElf_Shdr shdr;
	int efd;
	char *s;
	size_t shstridx, symtabidx = 0, dynsymidx = 0;
#endif

	if (getenv("DTRACE_DOF_INIT_DISABLE") != NULL)
		return;

	if (getenv("DTRACE_DOF_INIT_DEBUG") != NULL)
		dof_init_debug = B_TRUE;

	if (dlinfo(RTLD_SELF, RTLD_DI_LINKMAP, &lmp) == -1 || lmp == NULL) {
		dprintf(1, "couldn't discover module name or address\n");
		return;
	}

#if defined(sun)
	if (dlinfo(RTLD_SELF, RTLD_DI_LMID, &lmid) == -1) {
		dprintf(1, "couldn't discover link map ID\n");
		return;
	}
#endif

	if ((modname = strrchr(lmp->l_name, '/')) == NULL)
		modname = lmp->l_name;
	else
		modname++;
#if !defined(sun)
	elf_version(EV_CURRENT);
	if ((efd = open(lmp->l_name, O_RDONLY, 0)) < 0) {
		dprintf(1, "couldn't open file for reading\n");
		return;
	}
	if ((e = elf_begin(efd, ELF_C_READ, NULL)) == NULL) {
		dprintf(1, "elf_begin failed\n");
		close(efd);
		return;
	}
	elf_getshdrstrndx(e, &shstridx);
	dof = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);
		if (shdr.sh_type == SHT_SYMTAB) {
			symtabidx = shdr.sh_link;
			symtabdata = elf_getdata(scn, NULL);
		} else if (shdr.sh_type == SHT_DYNSYM) {
			dynsymidx = shdr.sh_link;
			dynsymdata = elf_getdata(scn, NULL);
		} else if (shdr.sh_type == SHT_SUNW_dof) {
			s = elf_strptr(e, shstridx, shdr.sh_name);
			if  (s != NULL && strcmp(s, ".SUNW_dof") == 0) {
				dofdata = elf_getdata(scn, NULL);
				dof = dofdata->d_buf;
			}
		}
	}
	if (dof == NULL) {
		dprintf(1, "SUNW_dof section not found\n");
		elf_end(e);
		close(efd);
		return;
	}

	while ((char *) dof < (char *) dofdata->d_buf + dofdata->d_size) {
		dof_next = (void *) ((char *) dof + dof->dofh_filesz);
#endif

	if (dof->dofh_ident[DOF_ID_MAG0] != DOF_MAG_MAG0 ||
	    dof->dofh_ident[DOF_ID_MAG1] != DOF_MAG_MAG1 ||
	    dof->dofh_ident[DOF_ID_MAG2] != DOF_MAG_MAG2 ||
	    dof->dofh_ident[DOF_ID_MAG3] != DOF_MAG_MAG3) {
		dprintf(0, ".SUNW_dof section corrupt\n");
		return;
	}

	elf = (void *)lmp->l_addr;

	dh.dofhp_dof = (uintptr_t)dof;
	dh.dofhp_addr = elf->e_type == ET_DYN ? (uintptr_t) lmp->l_addr : 0;

	if (lmid == 0) {
		(void) snprintf(dh.dofhp_mod, sizeof (dh.dofhp_mod),
		    "%s", modname);
	} else {
		(void) snprintf(dh.dofhp_mod, sizeof (dh.dofhp_mod),
		    "LM%lu`%s", lmid, modname);
	}

	if ((p = getenv("DTRACE_DOF_INIT_DEVNAME")) != NULL)
		devnamep = p;

	if ((fd = open64(devnamep, O_RDWR)) < 0) {
		dprintf(1, "failed to open helper device %s", devnamep);
#if defined(sun)
		/*
		 * If the device path wasn't explicitly set, try again with
		 * the old device path.
		 */
		if (p != NULL)
			return;

		devnamep = olddevname;

		if ((fd = open64(devnamep, O_RDWR)) < 0) {
			dprintf(1, "failed to open helper device %s", devnamep);
			return;
		}
#else
		return;
#endif
	}
	if ((gen = ioctl(fd, DTRACEHIOC_ADDDOF, &dh)) == -1)
		dprintf(1, "DTrace ioctl failed for DOF at %p", dof);
	else {
		dprintf(1, "DTrace ioctl succeeded for DOF at %p\n", dof);
#if !defined(sun)
		gen = dh.gen;
#endif
	}

	(void) close(fd);

#if !defined(sun)
		/* End of while loop */
		dof = dof_next;
	}

	elf_end(e);
	(void) close(efd);
#endif
}

#if defined(sun)
#pragma fini(dtrace_dof_fini)
#else
static void dtrace_dof_fini(void) __attribute__ ((destructor));
#endif

static void
dtrace_dof_fini(void)
{
	int fd;

	if ((fd = open64(devnamep, O_RDWR)) < 0) {
		dprintf(1, "failed to open helper device %s", devnamep);
		return;
	}

	if ((gen = ioctl(fd, DTRACEHIOC_REMOVE, &gen)) == -1)
		dprintf(1, "DTrace ioctl failed to remove DOF (%d)\n", gen);
	else
		dprintf(1, "DTrace ioctl removed DOF (%d)\n", gen);

	(void) close(fd);
}
