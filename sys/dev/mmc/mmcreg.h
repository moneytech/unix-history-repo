/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 * Copyright (c) 2017 Marius Strobl <marius@FreeBSD.org>
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
 * Portions of this software may have been developed with reference to
 * the SD Simplified Specification.  The following disclaimer may apply:
 *
 * The following conditions apply to the release of the simplified
 * specification ("Simplified Specification") by the SD Card Association and
 * the SD Group. The Simplified Specification is a subset of the complete SD
 * Specification which is owned by the SD Card Association and the SD
 * Group. This Simplified Specification is provided on a non-confidential
 * basis subject to the disclaimers below. Any implementation of the
 * Simplified Specification may require a license from the SD Card
 * Association, SD Group, SD-3C LLC or other third parties.
 *
 * Disclaimers:
 *
 * The information contained in the Simplified Specification is presented only
 * as a standard specification for SD Cards and SD Host/Ancillary products and
 * is provided "AS-IS" without any representations or warranties of any
 * kind. No responsibility is assumed by the SD Group, SD-3C LLC or the SD
 * Card Association for any damages, any infringements of patents or other
 * right of the SD Group, SD-3C LLC, the SD Card Association or any third
 * parties, which may result from its use. No license is granted by
 * implication, estoppel or otherwise under any patent or other rights of the
 * SD Group, SD-3C LLC, the SD Card Association or any third party. Nothing
 * herein shall be construed as an obligation by the SD Group, the SD-3C LLC
 * or the SD Card Association to disclose or distribute any technical
 * information, know-how or other confidential information to any third party.
 *
 * $FreeBSD$
 */

#ifndef DEV_MMC_MMCREG_H
#define	DEV_MMC_MMCREG_H

/*
 * This file contains the register definitions for the mmc and sd busses.
 * They are taken from publicly available sources.
 */

struct mmc_data;
struct mmc_request;

struct mmc_command {
	uint32_t	opcode;
	uint32_t	arg;
	uint32_t	resp[4];
	uint32_t	flags;		/* Expected responses */
#define	MMC_RSP_PRESENT	(1ul << 0)	/* Response */
#define	MMC_RSP_136	(1ul << 1)	/* 136 bit response */
#define	MMC_RSP_CRC	(1ul << 2)	/* Expect valid crc */
#define	MMC_RSP_BUSY	(1ul << 3)	/* Card may send busy */
#define	MMC_RSP_OPCODE	(1ul << 4)	/* Response include opcode */
#define	MMC_RSP_MASK	0x1ful
#define	MMC_CMD_AC	(0ul << 5)	/* Addressed Command, no data */
#define	MMC_CMD_ADTC	(1ul << 5)	/* Addressed Data transfer cmd */
#define	MMC_CMD_BC	(2ul << 5)	/* Broadcast command, no response */
#define	MMC_CMD_BCR	(3ul << 5)	/* Broadcast command with response */
#define	MMC_CMD_MASK	(3ul << 5)

/* Possible response types defined in the standard: */
#define	MMC_RSP_NONE	(0)
#define	MMC_RSP_R1	(MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define	MMC_RSP_R1B	(MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_RSP_BUSY)
#define	MMC_RSP_R2	(MMC_RSP_PRESENT | MMC_RSP_136 | MMC_RSP_CRC)
#define	MMC_RSP_R3	(MMC_RSP_PRESENT)
#define	MMC_RSP_R4	(MMC_RSP_PRESENT)
#define	MMC_RSP_R5	(MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define	MMC_RSP_R5B	(MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_RSP_BUSY)
#define	MMC_RSP_R6	(MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define	MMC_RSP_R7	(MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define	MMC_RSP(x)	((x) & MMC_RSP_MASK)
	uint32_t	retries;
	uint32_t	error;
#define	MMC_ERR_NONE	0
#define	MMC_ERR_TIMEOUT	1
#define	MMC_ERR_BADCRC	2
#define	MMC_ERR_FIFO	3
#define	MMC_ERR_FAILED	4
#define	MMC_ERR_INVALID	5
#define	MMC_ERR_NO_MEMORY 6
#define	MMC_ERR_MAX	6
	struct mmc_data	*data;		/* Data segment with cmd */
	struct mmc_request *mrq;	/* backpointer to request */
};

/*
 * R1 responses
 *
 * Types (per SD 2.0 standard)
 *	e : error bit
 *	s : status bit
 *	r : detected and set for the actual command response
 *	x : Detected and set during command execution.  The host can get
 *	    the status by issuing a command with R1 response.
 *
 * Clear Condition (per SD 2.0 standard)
 *	a : according to the card current state.
 *	b : always related to the previous command.  reception of a valid
 *	    command will clear it (with a delay of one command).
 *	c : clear by read
 */
#define	R1_OUT_OF_RANGE (1u << 31)		/* erx, c */
#define	R1_ADDRESS_ERROR (1u << 30)		/* erx, c */
#define	R1_BLOCK_LEN_ERROR (1u << 29)		/* erx, c */
#define	R1_ERASE_SEQ_ERROR (1u << 28)		/* er, c */
#define	R1_ERASE_PARAM (1u << 27)		/* erx, c */
#define	R1_WP_VIOLATION (1u << 26)		/* erx, c */
#define	R1_CARD_IS_LOCKED (1u << 25)		/* sx, a */
#define	R1_LOCK_UNLOCK_FAILED (1u << 24)	/* erx, c */
#define	R1_COM_CRC_ERROR (1u << 23)		/* er, b */
#define	R1_ILLEGAL_COMMAND (1u << 22)		/* er, b */
#define	R1_CARD_ECC_FAILED (1u << 21)		/* erx, c */
#define	R1_CC_ERROR (1u << 20)			/* erx, c */
#define	R1_ERROR (1u << 19)			/* erx, c */
#define	R1_CSD_OVERWRITE (1u << 16)		/* erx, c */
#define	R1_WP_ERASE_SKIP (1u << 15)		/* erx, c */
#define	R1_CARD_ECC_DISABLED (1u << 14)		/* sx, a */
#define	R1_ERASE_RESET (1u << 13)		/* sr, c */
#define	R1_CURRENT_STATE_MASK (0xfu << 9)	/* sx, b */
#define	R1_READY_FOR_DATA (1u << 8)		/* sx, a */
#define	R1_SWITCH_ERROR (1u << 7)		/* sx, c */
#define	R1_APP_CMD (1u << 5)			/* sr, c */
#define	R1_AKE_SEQ_ERROR (1u << 3)		/* er, c */
#define	R1_STATUS(x)		((x) & 0xFFFFE000)
#define	R1_CURRENT_STATE(x)	(((x) & R1_CURRENT_STATE_MASK) >> 9)
#define	R1_STATE_IDLE	0
#define	R1_STATE_READY	1
#define	R1_STATE_IDENT	2
#define	R1_STATE_STBY	3
#define	R1_STATE_TRAN	4
#define	R1_STATE_DATA	5
#define	R1_STATE_RCV	6
#define	R1_STATE_PRG	7
#define	R1_STATE_DIS	8

struct mmc_data {
	size_t len;		/* size of the data */
	size_t xfer_len;
	void *data;		/* data buffer */
	uint32_t	flags;
#define	MMC_DATA_WRITE	(1UL << 0)
#define	MMC_DATA_READ	(1UL << 1)
#define	MMC_DATA_STREAM	(1UL << 2)
#define	MMC_DATA_MULTI	(1UL << 3)
	struct mmc_request *mrq;
};

struct mmc_request {
	struct mmc_command *cmd;
	struct mmc_command *stop;
	void (*done)(struct mmc_request *); /* Completion function */
	void *done_data;		/* requestor set data */
	uint32_t flags;
#define	MMC_REQ_DONE	1
};

/* Command definitions */

/* Class 0 and 1: Basic commands & read stream commands */
#define	MMC_GO_IDLE_STATE	0
#define	MMC_SEND_OP_COND	1
#define	MMC_ALL_SEND_CID	2
#define	MMC_SET_RELATIVE_ADDR	3
#define	SD_SEND_RELATIVE_ADDR	3
#define	MMC_SET_DSR		4
#define	MMC_SLEEP_AWAKE		5
#define	MMC_SWITCH_FUNC		6
#define	 MMC_SWITCH_FUNC_CMDS	 0
#define	 MMC_SWITCH_FUNC_SET	 1
#define	 MMC_SWITCH_FUNC_CLR	 2
#define	 MMC_SWITCH_FUNC_WR	 3
#define	MMC_SELECT_CARD		7
#define	MMC_DESELECT_CARD	7
#define	MMC_SEND_EXT_CSD	8
#define	SD_SEND_IF_COND		8
#define	MMC_SEND_CSD		9
#define	MMC_SEND_CID		10
#define	MMC_READ_DAT_UNTIL_STOP	11
#define	MMC_STOP_TRANSMISSION	12
#define	MMC_SEND_STATUS		13
#define	MMC_BUSTEST_R		14
#define	MMC_GO_INACTIVE_STATE	15
#define	MMC_BUSTEST_W		19

/* Class 2: Block oriented read commands */
#define	MMC_SET_BLOCKLEN	16
#define	MMC_READ_SINGLE_BLOCK	17
#define	MMC_READ_MULTIPLE_BLOCK	18
			/* reserved: 19 */

/* Class 3: Stream write commands */
#define	MMC_WRITE_DAT_UNTIL_STOP 20
			/* reserved: 21 */
			/* reserved: 22 */

/* Class 4: Block oriented write commands */
#define	MMC_SET_BLOCK_COUNT	23
#define	MMC_WRITE_BLOCK		24
#define	MMC_WRITE_MULTIPLE_BLOCK 25
#define	MMC_PROGARM_CID		26
#define	MMC_PROGRAM_CSD		27

/* Class 6: Block oriented write protection commands */
#define	MMC_SET_WRITE_PROT	28
#define	MMC_CLR_WRITE_PROT	29
#define	MMC_SEND_WRITE_PROT	30
			/* reserved: 31 */

/* Class 5: Erase commands */
#define	SD_ERASE_WR_BLK_START	32
#define	SD_ERASE_WR_BLK_END	33
			/* 34 -- reserved old command */
#define	MMC_ERASE_GROUP_START	35
#define	MMC_ERASE_GROUP_END	36
			/* 37 -- reserved old command */
#define	MMC_ERASE		38

/* Class 9: I/O mode commands */
#define	MMC_FAST_IO		39
#define	MMC_GO_IRQ_STATE	40
			/* reserved: 41 */

/* Class 7: Lock card */
#define	MMC_LOCK_UNLOCK		42
			/* reserved: 43 */
			/* reserved: 44 */
			/* reserved: 45 */
			/* reserved: 46 */
			/* reserved: 47 */
			/* reserved: 48 */
			/* reserved: 49 */
			/* reserved: 50 */
			/* reserved: 51 */
			/* reserved: 54 */

/* Class 8: Application specific commands */
#define	MMC_APP_CMD		55
#define	MMC_GEN_CMD		56
			/* reserved: 57 */
			/* reserved: 58 */
			/* reserved: 59 */
			/* reserved for mfg: 60 */
			/* reserved for mfg: 61 */
			/* reserved for mfg: 62 */
			/* reserved for mfg: 63 */

/* Class 9: I/O cards (sd) */
#define	SD_IO_RW_DIRECT		52
#define	SD_IO_RW_EXTENDED	53

/* Class 10: Switch function commands */
#define	SD_SWITCH_FUNC		6
			/* reserved: 34 */
			/* reserved: 35 */
			/* reserved: 36 */
			/* reserved: 37 */
			/* reserved: 50 */
			/* reserved: 57 */

/* Application specific commands for SD */
#define	ACMD_SET_BUS_WIDTH	6
#define	ACMD_SD_STATUS		13
#define	ACMD_SEND_NUM_WR_BLOCKS	22
#define	ACMD_SET_WR_BLK_ERASE_COUNT 23
#define	ACMD_SD_SEND_OP_COND	41
#define	ACMD_SET_CLR_CARD_DETECT 42
#define	ACMD_SEND_SCR		51

/*
 * EXT_CSD fields
 */
#define	EXT_CSD_EXT_PART_ATTR	52	/* R/W, 2 bytes */
#define	EXT_CSD_ENH_START_ADDR	136	/* R/W, 4 bytes */
#define	EXT_CSD_ENH_SIZE_MULT	140	/* R/W, 3 bytes */
#define	EXT_CSD_GP_SIZE_MULT	143	/* R/W, 12 bytes */
#define	EXT_CSD_PART_SET	155	/* R/W */
#define	EXT_CSD_PART_ATTR	156	/* R/W */
#define	EXT_CSD_PART_SUPPORT	160	/* RO */
#define	EXT_CSD_RPMB_MULT	168	/* RO */
#define	EXT_CSD_BOOT_WP_STATUS	174	/* RO */
#define	EXT_CSD_ERASE_GRP_DEF	175	/* R/W */
#define	EXT_CSD_PART_CONFIG	179	/* R/W */
#define	EXT_CSD_BUS_WIDTH	183	/* R/W */
#define	EXT_CSD_HS_TIMING	185	/* R/W */
#define	EXT_CSD_CARD_TYPE	196	/* RO */
#define	EXT_CSD_REV		192	/* RO */
#define	EXT_CSD_PART_SWITCH_TO	199	/* RO */
#define	EXT_CSD_SEC_CNT		212	/* RO, 4 bytes */
#define	EXT_CSD_HC_WP_GRP_SIZE	221	/* RO */
#define	EXT_CSD_ERASE_TO_MULT	223	/* RO */
#define	EXT_CSD_ERASE_GRP_SIZE	224	/* RO */
#define	EXT_CSD_BOOT_SIZE_MULT	226	/* RO */
#define	EXT_CSD_GEN_CMD6_TIME	248	/* RO */

/*
 * EXT_CSD field definitions
 */
#define	EXT_CSD_EXT_PART_ATTR_DEFAULT		0x0
#define	EXT_CSD_EXT_PART_ATTR_SYSTEMCODE	0x1
#define	EXT_CSD_EXT_PART_ATTR_NPERSISTENT	0x2

#define	EXT_CSD_PART_SET_COMPLETED		0x01

#define	EXT_CSD_PART_ATTR_ENH_USR		0x01
#define	EXT_CSD_PART_ATTR_ENH_GP0		0x02
#define	EXT_CSD_PART_ATTR_ENH_GP1		0x04
#define	EXT_CSD_PART_ATTR_ENH_GP2		0x08
#define	EXT_CSD_PART_ATTR_ENH_GP3		0x10
#define	EXT_CSD_PART_ATTR_ENH_MASK		0x1f

#define	EXT_CSD_PART_SUPPORT_EN			0x01
#define	EXT_CSD_PART_SUPPORT_ENH_ATTR_EN	0x02
#define	EXT_CSD_PART_SUPPORT_EXT_ATTR_EN	0x04

#define	EXT_CSD_BOOT_WP_STATUS_BOOT0_PWR	0x01
#define	EXT_CSD_BOOT_WP_STATUS_BOOT0_PERM	0x02
#define	EXT_CSD_BOOT_WP_STATUS_BOOT0_MASK	0x03
#define	EXT_CSD_BOOT_WP_STATUS_BOOT1_PWR	0x04
#define	EXT_CSD_BOOT_WP_STATUS_BOOT1_PERM	0x08
#define	EXT_CSD_BOOT_WP_STATUS_BOOT1_MASK	0x0c

#define	EXT_CSD_ERASE_GRP_DEF_EN	0x01

#define	EXT_CSD_PART_CONFIG_ACC_DEFAULT	0x00
#define	EXT_CSD_PART_CONFIG_ACC_BOOT0	0x01
#define	EXT_CSD_PART_CONFIG_ACC_BOOT1	0x02
#define	EXT_CSD_PART_CONFIG_ACC_RPMB	0x03
#define	EXT_CSD_PART_CONFIG_ACC_GP0	0x04
#define	EXT_CSD_PART_CONFIG_ACC_GP1	0x05
#define	EXT_CSD_PART_CONFIG_ACC_GP2	0x06
#define	EXT_CSD_PART_CONFIG_ACC_GP3	0x07
#define	EXT_CSD_PART_CONFIG_ACC_MASK	0x07
#define	EXT_CSD_PART_CONFIG_BOOT0	0x08
#define	EXT_CSD_PART_CONFIG_BOOT1	0x10
#define	EXT_CSD_PART_CONFIG_BOOT_USR	0x38
#define	EXT_CSD_PART_CONFIG_BOOT_MASK	0x38
#define	EXT_CSD_PART_CONFIG_BOOT_ACK	0x40

#define	EXT_CSD_CMD_SET_NORMAL		1
#define	EXT_CSD_CMD_SET_SECURE		2
#define	EXT_CSD_CMD_SET_CPSECURE	4

#define	EXT_CSD_CARD_TYPE_26	1
#define	EXT_CSD_CARD_TYPE_52	2

#define	EXT_CSD_BUS_WIDTH_1	0
#define	EXT_CSD_BUS_WIDTH_4	1
#define	EXT_CSD_BUS_WIDTH_8	2

#define	MMC_TYPE_26_MAX_HS	26000000
#define	MMC_TYPE_52_MAX_HS	52000000

/*
 * SD bus widths
 */
#define	SD_BUS_WIDTH_1		0
#define	SD_BUS_WIDTH_4		2

/*
 * SD Switch
 */
#define	SD_SWITCH_MODE_CHECK	0
#define	SD_SWITCH_MODE_SET	1
#define	SD_SWITCH_GROUP1	0
#define	SD_SWITCH_NORMAL_MODE	0
#define	SD_SWITCH_HS_MODE	1
#define	SD_SWITCH_NOCHANGE	0xF

#define	SD_CLR_CARD_DETECT	0
#define	SD_SET_CARD_DETECT	1

#define	SD_MAX_HS		50000000

/* OCR bits */

/*
 * in SD 2.0 spec, bits 8-14 are now marked reserved
 * Low voltage in SD2.0 spec is bit 7, TBD voltage
 * Low voltage in MC 3.31 spec is bit 7, 1.65-1.95V
 * Specs prior to  MMC 3.31 defined bits 0-7 as voltages down to 1.5V.
 * 3.31 redefined them to be reserved and also said that cards had to
 * support the 2.7-3.6V and fixed the OCR to be 0xfff8000 for high voltage
 * cards.  MMC 4.0 says that a dual voltage card responds with 0xfff8080.
 * Looks like the fine-grained control of the voltage tolerance ranges
 * was abandoned.
 *
 * The MMC_OCR_CCS appears to be valid for only SD cards.
 */
#define	MMC_OCR_VOLTAGE	0x3fffffffU	/* Vdd Voltage mask */
#define	MMC_OCR_LOW_VOLTAGE (1u << 7)	/* Low Voltage Range -- tbd */
#define	MMC_OCR_MIN_VOLTAGE_SHIFT	7
#define	MMC_OCR_200_210	(1U << 8)	/* Vdd voltage 2.00 ~ 2.10 */
#define	MMC_OCR_210_220	(1U << 9)	/* Vdd voltage 2.10 ~ 2.20 */
#define	MMC_OCR_220_230	(1U << 10)	/* Vdd voltage 2.20 ~ 2.30 */
#define	MMC_OCR_230_240	(1U << 11)	/* Vdd voltage 2.30 ~ 2.40 */
#define	MMC_OCR_240_250	(1U << 12)	/* Vdd voltage 2.40 ~ 2.50 */
#define	MMC_OCR_250_260	(1U << 13)	/* Vdd voltage 2.50 ~ 2.60 */
#define	MMC_OCR_260_270	(1U << 14)	/* Vdd voltage 2.60 ~ 2.70 */
#define	MMC_OCR_270_280	(1U << 15)	/* Vdd voltage 2.70 ~ 2.80 */
#define	MMC_OCR_280_290	(1U << 16)	/* Vdd voltage 2.80 ~ 2.90 */
#define	MMC_OCR_290_300	(1U << 17)	/* Vdd voltage 2.90 ~ 3.00 */
#define	MMC_OCR_300_310	(1U << 18)	/* Vdd voltage 3.00 ~ 3.10 */
#define	MMC_OCR_310_320	(1U << 19)	/* Vdd voltage 3.10 ~ 3.20 */
#define	MMC_OCR_320_330	(1U << 20)	/* Vdd voltage 3.20 ~ 3.30 */
#define	MMC_OCR_330_340	(1U << 21)	/* Vdd voltage 3.30 ~ 3.40 */
#define	MMC_OCR_340_350	(1U << 22)	/* Vdd voltage 3.40 ~ 3.50 */
#define	MMC_OCR_350_360	(1U << 23)	/* Vdd voltage 3.50 ~ 3.60 */
#define	MMC_OCR_MAX_VOLTAGE_SHIFT	23
#define	MMC_OCR_CCS	(1u << 30)	/* Card Capacity status (SD vs SDHC) */
#define	MMC_OCR_CARD_BUSY (1U << 31)	/* Card Power up status */

/* CSD -- decoded structure */
struct mmc_cid {
	uint32_t mid;
	char pnm[8];
	uint32_t psn;
	uint16_t oid;
	uint16_t mdt_year;
	uint8_t mdt_month;
	uint8_t prv;
	uint8_t fwrev;
};

struct mmc_csd
{
	uint8_t csd_structure;
	uint8_t spec_vers;
	uint16_t ccc;
	uint16_t tacc;
	uint32_t nsac;
	uint32_t r2w_factor;
	uint32_t tran_speed;
	uint32_t read_bl_len;
	uint32_t write_bl_len;
	uint32_t vdd_r_curr_min;
	uint32_t vdd_r_curr_max;
	uint32_t vdd_w_curr_min;
	uint32_t vdd_w_curr_max;
	uint32_t wp_grp_size;
	uint32_t erase_sector;
	uint64_t capacity;
	unsigned int read_bl_partial:1,
	    read_blk_misalign:1,
	    write_bl_partial:1,
	    write_blk_misalign:1,
	    dsr_imp:1,
	    erase_blk_en:1,
	    wp_grp_enable:1;
};

struct mmc_scr
{
	unsigned char		sda_vsn;
	unsigned char		bus_widths;
#define	SD_SCR_BUS_WIDTH_1	(1 << 0)
#define	SD_SCR_BUS_WIDTH_4	(1 << 2)
};

struct mmc_sd_status
{
	uint8_t			bus_width;
	uint8_t			secured_mode;
	uint16_t		card_type;
	uint16_t		prot_area;
	uint8_t			speed_class;
	uint8_t			perf_move;
	uint8_t			au_size;
	uint16_t		erase_size;
	uint8_t			erase_timeout;
	uint8_t			erase_offset;
};

/*
 * Various MMC/SD constants
 */
#define	MMC_BOOT_RPMB_BLOCK_SIZE	(128 * 1024)

#define	MMC_EXTCSD_SIZE	512

#define	MMC_PART_GP_MAX	4
#define	MMC_PART_MAX	8

/*
 * Older versions of the MMC standard had a variable sector size.  However,
 * I've been able to find no old MMC or SD cards that have a non 512
 * byte sector size anywhere, so we assume that such cards are very rare
 * and only note their existence in passing here...
 */
#define	MMC_SECTOR_SIZE	512

#endif /* DEV_MMCREG_H */
