
/*	@(#)tmscpreg.h	1.1	11/2/84	84/09/25	*/

/************************************************************************
 *									*
 *			Copyright (c) 1985 by				*
 *		Digital Equipment Corporation, Maynard, MA		*
 *									*
 *	Permission to use, copy, modify, and distribute this software	*
 *	and its documentation is hereby granted to licensees of the	*
 *	Regents of the University of California pursuant to their	*
 *	license agreement for the "Fourth Berkeley Software		*
 *	Distribution".							*
 *									*
 *	The information in this software is subject to change without	*
 *	notice and should not be construed as a commitment by Digital	*
 *	Equipment Corporation.  Digital makes no representations	*
 *	about suitability of this software for any purpose. It is	*
 *	supplied "as is" without express or implied warranty.		*
 *									*
 *	This software is not subject to any license of the American	*
 *	Telephone and Telegraph Company.				*
 *									*
 ************************************************************************/
/*
 * TMSCP registers and structures
 */
 
struct tmscpdevice {
	short	tmscpip;	/* initialization and polling */
	short	tmscpsa;	/* status and address */
};
 
#define	TMSCP_ERR		0100000	/* error bit */
#define	TMSCP_STEP4	0040000	/* step 4 has started */
#define	TMSCP_STEP3	0020000	/* step 3 has started */
#define	TMSCP_STEP2	0010000	/* step 2 has started */
#define	TMSCP_STEP1	0004000	/* step 1 has started */
#define	TMSCP_NV		0002000	/* no host settable interrupt vector */
#define	TMSCP_QB		0001000	/* controller supports Q22 bus */
#define	TMSCP_DI		0000400	/* controller implements diagnostics */
#define	TMSCP_OD		0000200	/* port allows odd host addr's in the b
uffer descriptor */
#define	TMSCP_IE		0000200	/* interrupt enable */
#define	TMSCP_MP		0000100	/* port supports address mapping */
#define	TMSCP_LF		0000002	/* host requests last fail response pac
ket */
#define	TMSCP_PI		0000001	/* host requests adapter purge interrup
ts */
#define	TMSCP_GO		0000001	/* start operation, after init */
 
 
/*
 * TMSCP Communications Area
 */
 
struct tmscpca {
	short	ca_xxx1;	/* unused */
	char	ca_xxx2;	/* unused */
	char	ca_bdp;		/* BDP to purge */
	short	ca_cmdint;	/* command queue transition interrupt flag */
	short	ca_rspint;	/* response queue transition interrupt flag */
	long	ca_rspdsc[NRSP];/* response descriptors */
	long	ca_cmddsc[NCMD];/* command descriptors */
};
 
#define	ca_ringbase	ca_rspdsc[0]
 
#define	TMSCP_OWN	0x80000000	/* port owns this descriptor (else host
 owns it) */
#define	TMSCP_INT	0x40000000	/* allow interrupt on ring transition *
/
 
#define	TMSCP_MAP	0x80000000	/* modifier for mapped buffer descripto
rs */
 
/*
 * TMSCP packet info (same as MSCP)
 */
struct mscp_header {
	short	tmscp_msglen;	/* length of MSCP packet */
	char	tmscp_credits;	/* low 4 bits: credits, high 4 bits: msgtype */
	char	tmscp_vcid;	/* virtual circuit id (connection id) */
};

