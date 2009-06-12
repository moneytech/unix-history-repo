/*-
 * Copyright (c) 2009, Pyun YongHyeon <yongari@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMATES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMATE.
 *
 * $FreeBSD$
 */

#ifndef	_IF_ALCREG_H
#define	_IF_ALCREG_H

/*
 * Atheros Communucations, Inc. PCI vendor ID
 */
#define	VENDORID_ATHEROS		0x1969

/*
 * Atheros AR8131/AR8132 device ID
 */
#define	DEVICEID_ATHEROS_AR8131		0x1063	/* L1C */
#define	DEVICEID_ATHEROS_AR8132		0x1062	/* L2C */

/* 0x0000 - 0x02FF : PCIe configuration space */

#define	ALC_PEX_UNC_ERR_SEV		0x10C
#define	PEX_UNC_ERR_SEV_TRN		0x00000001
#define	PEX_UNC_ERR_SEV_DLP		0x00000010
#define	PEX_UNC_ERR_SEV_PSN_TLP		0x00001000
#define	PEX_UNC_ERR_SEV_FCP		0x00002000
#define	PEX_UNC_ERR_SEV_CPL_TO		0x00004000
#define	PEX_UNC_ERR_SEV_CA		0x00008000
#define	PEX_UNC_ERR_SEV_UC		0x00010000
#define	PEX_UNC_ERR_SEV_ROV		0x00020000
#define	PEX_UNC_ERR_SEV_MLFP		0x00040000
#define	PEX_UNC_ERR_SEV_ECRC		0x00080000
#define	PEX_UNC_ERR_SEV_UR		0x00100000

#define	ALC_TWSI_CFG			0x218
#define	TWSI_CFG_SW_LD_START		0x00000800
#define	TWSI_CFG_HW_LD_START		0x00001000
#define	TWSI_CFG_LD_EXIST		0x00400000

#define	ALC_PCIE_PHYMISC		0x1000
#define	PCIE_PHYMISC_FORCE_RCV_DET	0x00000004

#define	ALC_TWSI_DEBUG			0x1108
#define	TWSI_DEBUG_DEV_EXIST		0x20000000

#define	ALC_EEPROM_CFG			0x12C0
#define	EEPROM_CFG_DATA_HI_MASK		0x0000FFFF
#define	EEPROM_CFG_ADDR_MASK		0x03FF0000
#define	EEPROM_CFG_ACK			0x40000000
#define	EEPROM_CFG_RW			0x80000000
#define	EEPROM_CFG_DATA_HI_SHIFT	0
#define	EEPROM_CFG_ADDR_SHIFT		16

#define	ALC_EEPROM_DATA_LO		0x12C4

#define	ALC_OPT_CFG			0x12F0
#define	OPT_CFG_CLK_ENB			0x00000002

#define	ALC_PM_CFG			0x12F8
#define	PM_CFG_SERDES_ENB		0x00000001
#define	PM_CFG_RBER_ENB			0x00000002
#define	PM_CFG_CLK_REQ_ENB		0x00000004
#define	PM_CFG_ASPM_L1_ENB		0x00000008
#define	PM_CFG_SERDES_L1_ENB		0x00000010
#define	PM_CFG_SERDES_PLL_L1_ENB	0x00000020
#define	PM_CFG_SERDES_PD_EX_L1		0x00000040
#define	PM_CFG_SERDES_BUDS_RX_L1_ENB	0x00000080
#define	PM_CFG_L0S_ENTRY_TIMER_MASK	0x00000F00
#define	PM_CFG_ASPM_L0S_ENB		0x00001000
#define	PM_CFG_CLK_SWH_L1		0x00002000
#define	PM_CFG_CLK_PWM_VER1_1		0x00004000
#define	PM_CFG_PCIE_RECV		0x00008000
#define	PM_CFG_L1_ENTRY_TIMER_MASK	0x000F0000
#define	PM_CFG_PM_REQ_TIMER_MASK	0x00F00000
#define	PM_CFG_LCKDET_TIMER_MASK	0x3F000000
#define	PM_CFG_MAC_ASPM_CHK		0x40000000
#define	PM_CFG_HOTRST			0x80000000
#define	PM_CFG_L0S_ENTRY_TIMER_SHIFT	8
#define	PM_CFG_L1_ENTRY_TIMER_SHIFT	16
#define	PM_CFG_PM_REQ_TIMER_SHIFT	20
#define	PM_CFG_LCKDET_TIMER_SHIFT	24

#define	ALC_MASTER_CFG			0x1400
#define	MASTER_RESET			0x00000001
#define	MASTER_BERT_START		0x00000010
#define	MASTER_TEST_MODE_MASK		0x000000C0
#define	MASTER_MTIMER_ENB		0x00000100
#define	MASTER_MANUAL_INTR_ENB		0x00000200
#define	MASTER_IM_TX_TIMER_ENB		0x00000400
#define	MASTER_IM_RX_TIMER_ENB		0x00000800
#define	MASTER_CLK_SEL_DIS		0x00001000
#define	MASTER_CLK_SWH_MODE		0x00002000
#define	MASTER_INTR_RD_CLR		0x00004000
#define	MASTER_CHIP_REV_MASK		0x00FF0000
#define	MASTER_CHIP_ID_MASK		0x7F000000
#define	MASTER_OTP_SEL			0x80000000
#define	MASTER_TEST_MODE_SHIFT		2
#define	MASTER_CHIP_REV_SHIFT		16
#define	MASTER_CHIP_ID_SHIFT		24

/* Number of ticks per usec for AR8131/AR8132. */
#define	ALC_TICK_USECS			2
#define	ALC_USECS(x)			((x) / ALC_TICK_USECS)

#define	ALC_MANUAL_TIMER		0x1404

#define	ALC_IM_TIMER			0x1408
#define	IM_TIMER_TX_MASK		0x0000FFFF
#define	IM_TIMER_RX_MASK		0xFFFF0000
#define	IM_TIMER_TX_SHIFT		0
#define	IM_TIMER_RX_SHIFT		16
#define	ALC_IM_TIMER_MIN		0
#define	ALC_IM_TIMER_MAX		130000	/* 130ms */
/*
 * 100us will ensure alc(4) wouldn't generate more than 10000 Rx
 * interrupts in a second.
 */
#define	ALC_IM_RX_TIMER_DEFAULT		100	/* 100us */
/*
 * alc(4) does not rely on Tx completion interrupts, so set it
 * somewhat large value to reduce Tx completion interrupts.
 */
#define	ALC_IM_TX_TIMER_DEFAULT		50000	/* 50ms */

#define	ALC_GPHY_CFG			0x140C	/* 16bits */
#define	GPHY_CFG_EXT_RESET		0x0001
#define	GPHY_CFG_RTL_MODE		0x0002
#define	GPHY_CFG_LED_MODE		0x0004
#define	GPHY_CFG_ANEG_NOW		0x0008
#define	GPHY_CFG_RECV_ANEG		0x0010
#define	GPHY_CFG_GATE_25M_ENB		0x0020
#define	GPHY_CFG_LPW_EXIT		0x0040
#define	GPHY_CFG_PHY_IDDQ		0x0080
#define	GPHY_CFG_PHY_IDDQ_DIS		0x0100
#define	GPHY_CFG_PCLK_SEL_DIS		0x0200
#define	GPHY_CFG_HIB_EN			0x0400
#define	GPHY_CFG_HIB_PULSE		0x0800
#define	GPHY_CFG_SEL_ANA_RESET		0x1000
#define	GPHY_CFG_PHY_PLL_ON		0x2000
#define	GPHY_CFG_PWDOWN_HW		0x4000
#define	GPHY_CFG_PHY_PLL_BYPASS		0x8000

#define	ALC_IDLE_STATUS			0x1410
#define	IDLE_STATUS_RXMAC		0x00000001
#define	IDLE_STATUS_TXMAC		0x00000002
#define	IDLE_STATUS_RXQ			0x00000004
#define	IDLE_STATUS_TXQ			0x00000008
#define	IDLE_STATUS_DMARD		0x00000010
#define	IDLE_STATUS_DMAWR		0x00000020
#define	IDLE_STATUS_SMB			0x00000040
#define	IDLE_STATUS_CMB			0x00000080

#define	ALC_MDIO			0x1414
#define	MDIO_DATA_MASK			0x0000FFFF
#define	MDIO_REG_ADDR_MASK		0x001F0000
#define	MDIO_OP_READ			0x00200000
#define	MDIO_OP_WRITE			0x00000000
#define	MDIO_SUP_PREAMBLE		0x00400000
#define	MDIO_OP_EXECUTE			0x00800000
#define	MDIO_CLK_25_4			0x00000000
#define	MDIO_CLK_25_6			0x02000000
#define	MDIO_CLK_25_8			0x03000000
#define	MDIO_CLK_25_10			0x04000000
#define	MDIO_CLK_25_14			0x05000000
#define	MDIO_CLK_25_20			0x06000000
#define	MDIO_CLK_25_28			0x07000000
#define	MDIO_OP_BUSY			0x08000000
#define	MDIO_AP_ENB			0x10000000
#define	MDIO_DATA_SHIFT			0
#define	MDIO_REG_ADDR_SHIFT		16

#define	MDIO_REG_ADDR(x)	\
	(((x) << MDIO_REG_ADDR_SHIFT) & MDIO_REG_ADDR_MASK)
/* Default PHY address. */
#define	ALC_PHY_ADDR			0

#define	ALC_PHY_STATUS			0x1418
#define	PHY_STATUS_RECV_ENB		0x00000001
#define	PHY_STATUS_GENERAL_MASK		0x0000FFFF
#define	PHY_STATUS_OE_PWSP_MASK		0x07FF0000
#define	PHY_STATUS_LPW_STATE		0x80000000
#define	PHY_STATIS_OE_PWSP_SHIFT	16

/* Packet memory BIST. */
#define	ALC_BIST0			0x141C
#define	BIST0_ENB			0x00000001
#define	BIST0_SRAM_FAIL			0x00000002
#define	BIST0_FUSE_FLAG			0x00000004

/* PCIe retry buffer BIST. */
#define	ALC_BIST1			0x1420
#define	BIST1_ENB			0x00000001
#define	BIST1_SRAM_FAIL			0x00000002
#define	BIST1_FUSE_FLAG			0x00000004

#define	ALC_SERDES_LOCK			0x1424
#define	SERDES_LOCK_DET			0x00000001
#define	SERDES_LOCK_DET_ENB		0x00000002

#define	ALC_MAC_CFG			0x1480
#define	MAC_CFG_TX_ENB			0x00000001
#define	MAC_CFG_RX_ENB			0x00000002
#define	MAC_CFG_TX_FC			0x00000004
#define	MAC_CFG_RX_FC			0x00000008
#define	MAC_CFG_LOOP			0x00000010
#define	MAC_CFG_FULL_DUPLEX		0x00000020
#define	MAC_CFG_TX_CRC_ENB		0x00000040
#define	MAC_CFG_TX_AUTO_PAD		0x00000080
#define	MAC_CFG_TX_LENCHK		0x00000100
#define	MAC_CFG_RX_JUMBO_ENB		0x00000200
#define	MAC_CFG_PREAMBLE_MASK		0x00003C00
#define	MAC_CFG_VLAN_TAG_STRIP		0x00004000
#define	MAC_CFG_PROMISC			0x00008000
#define	MAC_CFG_TX_PAUSE		0x00010000
#define	MAC_CFG_SCNT			0x00020000
#define	MAC_CFG_SYNC_RST_TX		0x00040000
#define	MAC_CFG_SIM_RST_TX		0x00080000
#define	MAC_CFG_SPEED_MASK		0x00300000
#define	MAC_CFG_SPEED_10_100		0x00100000
#define	MAC_CFG_SPEED_1000		0x00200000
#define	MAC_CFG_DBG_TX_BACKOFF		0x00400000
#define	MAC_CFG_TX_JUMBO_ENB		0x00800000
#define	MAC_CFG_RXCSUM_ENB		0x01000000
#define	MAC_CFG_ALLMULTI		0x02000000
#define	MAC_CFG_BCAST			0x04000000
#define	MAC_CFG_DBG			0x08000000
#define	MAC_CFG_SINGLE_PAUSE_ENB	0x10000000
#define	MAC_CFG_PREAMBLE_SHIFT		10
#define	MAC_CFG_PREAMBLE_DEFAULT	7

#define	ALC_IPG_IFG_CFG			0x1484
#define	IPG_IFG_IPGT_MASK		0x0000007F
#define	IPG_IFG_MIFG_MASK		0x0000FF00
#define	IPG_IFG_IPG1_MASK		0x007F0000
#define	IPG_IFG_IPG2_MASK		0x7F000000
#define	IPG_IFG_IPGT_SHIFT		0
#define	IPG_IFG_IPGT_DEFAULT		0x60
#define	IPG_IFG_MIFG_SHIFT		8
#define	IPG_IFG_MIFG_DEFAULT		0x50
#define	IPG_IFG_IPG1_SHIFT		16
#define	IPG_IFG_IPG1_DEFAULT		0x40
#define	IPG_IFG_IPG2_SHIFT		24
#define	IPG_IFG_IPG2_DEFAULT		0x60

/* Station address. */
#define	ALC_PAR0			0x1488
#define	ALC_PAR1			0x148C

/* 64bit multicast hash register. */
#define	ALC_MAR0			0x1490
#define	ALC_MAR1			0x1494

/* half-duplex parameter configuration. */
#define	ALC_HDPX_CFG			0x1498
#define	HDPX_CFG_LCOL_MASK		0x000003FF
#define	HDPX_CFG_RETRY_MASK		0x0000F000
#define	HDPX_CFG_EXC_DEF_EN		0x00010000
#define	HDPX_CFG_NO_BACK_C		0x00020000
#define	HDPX_CFG_NO_BACK_P		0x00040000
#define	HDPX_CFG_ABEBE			0x00080000
#define	HDPX_CFG_ABEBT_MASK		0x00F00000
#define	HDPX_CFG_JAMIPG_MASK		0x0F000000
#define	HDPX_CFG_LCOL_SHIFT		0
#define	HDPX_CFG_LCOL_DEFAULT		0x37
#define	HDPX_CFG_RETRY_SHIFT		12
#define	HDPX_CFG_RETRY_DEFAULT		0x0F
#define	HDPX_CFG_ABEBT_SHIFT		20
#define	HDPX_CFG_ABEBT_DEFAULT		0x0A
#define	HDPX_CFG_JAMIPG_SHIFT		24
#define	HDPX_CFG_JAMIPG_DEFAULT		0x07

#define	ALC_FRAME_SIZE			0x149C

#define	ALC_WOL_CFG			0x14A0
#define	WOL_CFG_PATTERN			0x00000001
#define	WOL_CFG_PATTERN_ENB		0x00000002
#define	WOL_CFG_MAGIC			0x00000004
#define	WOL_CFG_MAGIC_ENB		0x00000008
#define	WOL_CFG_LINK_CHG		0x00000010
#define	WOL_CFG_LINK_CHG_ENB		0x00000020
#define	WOL_CFG_PATTERN_DET		0x00000100
#define	WOL_CFG_MAGIC_DET		0x00000200
#define	WOL_CFG_LINK_CHG_DET		0x00000400
#define	WOL_CFG_CLK_SWITCH_ENB		0x00008000
#define	WOL_CFG_PATTERN0		0x00010000
#define	WOL_CFG_PATTERN1		0x00020000
#define	WOL_CFG_PATTERN2		0x00040000
#define	WOL_CFG_PATTERN3		0x00080000
#define	WOL_CFG_PATTERN4		0x00100000
#define	WOL_CFG_PATTERN5		0x00200000
#define	WOL_CFG_PATTERN6		0x00400000

/* WOL pattern length. */
#define	ALC_PATTERN_CFG0		0x14A4
#define	PATTERN_CFG_0_LEN_MASK		0x0000007F
#define	PATTERN_CFG_1_LEN_MASK		0x00007F00
#define	PATTERN_CFG_2_LEN_MASK		0x007F0000
#define	PATTERN_CFG_3_LEN_MASK		0x7F000000

#define	ALC_PATTERN_CFG1		0x14A8
#define	PATTERN_CFG_4_LEN_MASK		0x0000007F
#define	PATTERN_CFG_5_LEN_MASK		0x00007F00
#define	PATTERN_CFG_6_LEN_MASK		0x007F0000

/* RSS */
#define	ALC_RSS_KEY0			0x14B0

#define	ALC_RSS_KEY1			0x14B4

#define	ALC_RSS_KEY2			0x14B8

#define	ALC_RSS_KEY3			0x14BC

#define	ALC_RSS_KEY4			0x14C0

#define	ALC_RSS_KEY5			0x14C4

#define	ALC_RSS_KEY6			0x14C8

#define	ALC_RSS_KEY7			0x14CC

#define	ALC_RSS_KEY8			0x14D0

#define	ALC_RSS_KEY9			0x14D4

#define	ALC_RSS_IDT_TABLE0		0x14E0

#define	ALC_RSS_IDT_TABLE1		0x14E4

#define	ALC_RSS_IDT_TABLE2		0x14E8

#define	ALC_RSS_IDT_TABLE3		0x14EC

#define	ALC_RSS_IDT_TABLE4		0x14F0

#define	ALC_RSS_IDT_TABLE5		0x14F4

#define	ALC_RSS_IDT_TABLE6		0x14F8

#define	ALC_RSS_IDT_TABLE7		0x14FC

#define	ALC_SRAM_RD0_ADDR		0x1500

#define	ALC_SRAM_RD1_ADDR		0x1504

#define	ALC_SRAM_RD2_ADDR		0x1508

#define	ALC_SRAM_RD3_ADDR		0x150C

#define	RD_HEAD_ADDR_MASK		0x000003FF
#define	RD_TAIL_ADDR_MASK		0x03FF0000
#define	RD_HEAD_ADDR_SHIFT		0
#define	RD_TAIL_ADDR_SHIFT		16

#define	ALC_RD_NIC_LEN0			0x1510	/* 8 bytes unit */
#define	RD_NIC_LEN_MASK			0x000003FF

#define	ALC_RD_NIC_LEN1			0x1514

#define	ALC_SRAM_TD_ADDR		0x1518
#define	TD_HEAD_ADDR_MASK		0x000003FF
#define	TD_TAIL_ADDR_MASK		0x03FF0000
#define	TD_HEAD_ADDR_SHIFT		0
#define	TD_TAIL_ADDR_SHIFT		16

#define	ALC_SRAM_TD_LEN			0x151C	/* 8 bytes unit */
#define	SRAM_TD_LEN_MASK		0x000003FF

#define	ALC_SRAM_RX_FIFO_ADDR		0x1520

#define	ALC_SRAM_RX_FIFO_LEN		0x1524

#define	ALC_SRAM_TX_FIFO_ADDR		0x1528

#define	ALC_SRAM_TX_FIFO_LEN		0x152C

#define	ALC_SRAM_TCPH_ADDR		0x1530
#define	SRAM_TCPH_ADDR_MASK		0x00000FFF
#define	SRAM_PATH_ADDR_MASK		0x0FFF0000
#define	SRAM_TCPH_ADDR_SHIFT		0
#define	SRAM_PKTH_ADDR_SHIFT		16

#define	ALC_DMA_BLOCK			0x1534
#define	DMA_BLOCK_LOAD			0x00000001

#define	ALC_RX_BASE_ADDR_HI		0x1540

#define	ALC_TX_BASE_ADDR_HI		0x1544

#define	ALC_SMB_BASE_ADDR_HI		0x1548

#define	ALC_SMB_BASE_ADDR_LO		0x154C

#define	ALC_RD0_HEAD_ADDR_LO		0x1550

#define	ALC_RD1_HEAD_ADDR_LO		0x1554

#define	ALC_RD2_HEAD_ADDR_LO		0x1558

#define	ALC_RD3_HEAD_ADDR_LO		0x155C

#define	ALC_RD_RING_CNT			0x1560
#define	RD_RING_CNT_MASK		0x00000FFF
#define	RD_RING_CNT_SHIFT		0

#define	ALC_RX_BUF_SIZE			0x1564
#define	RX_BUF_SIZE_MASK		0x0000FFFF
/*
 * If larger buffer size than 1536 is specified the controller
 * will be locked up. This is hardware limitation.
 */
#define	RX_BUF_SIZE_MAX			1536

#define	ALC_RRD0_HEAD_ADDR_LO		0x1568

#define	ALC_RRD1_HEAD_ADDR_LO		0x156C

#define	ALC_RRD2_HEAD_ADDR_LO		0x1570

#define	ALC_RRD3_HEAD_ADDR_LO		0x1574

#define	ALC_RRD_RING_CNT		0x1578
#define	RRD_RING_CNT_MASK		0x00000FFF
#define	RRD_RING_CNT_SHIFT		0

#define	ALC_TDH_HEAD_ADDR_LO		0x157C

#define	ALC_TDL_HEAD_ADDR_LO		0x1580

#define	ALC_TD_RING_CNT			0x1584
#define	TD_RING_CNT_MASK		0x0000FFFF
#define	TD_RING_CNT_SHIFT		0

#define	ALC_CMB_BASE_ADDR_LO		0x1588

#define	ALC_TXQ_CFG			0x1590
#define	TXQ_CFG_TD_BURST_MASK		0x0000000F
#define	TXQ_CFG_IP_OPTION_ENB		0x00000010
#define	TXQ_CFG_ENB			0x00000020
#define	TXQ_CFG_ENHANCED_MODE		0x00000040
#define	TXQ_CFG_8023_ENB		0x00000080
#define	TXQ_CFG_TX_FIFO_BURST_MASK	0xFFFF0000
#define	TXQ_CFG_TD_BURST_SHIFT		0
#define	TXQ_CFG_TD_BURST_DEFAULT	5
#define	TXQ_CFG_TX_FIFO_BURST_SHIFT	16

#define	ALC_TSO_OFFLOAD_THRESH		0x1594	/* 8 bytes unit */
#define	TSO_OFFLOAD_THRESH_MASK		0x000007FF
#define	TSO_OFFLOAD_THRESH_SHIFT	0
#define	TSO_OFFLOAD_THRESH_UNIT		8
#define	TSO_OFFLOAD_THRESH_UNIT_SHIFT	3

#define	ALC_TXF_WATER_MARK		0x1598	/* 8 bytes unit */
#define	TXF_WATER_MARK_HI_MASK		0x00000FFF
#define	TXF_WATER_MARK_LO_MASK		0x0FFF0000
#define	TXF_WATER_MARK_BURST_ENB	0x80000000
#define	TXF_WATER_MARK_LO_SHIFT		0
#define	TXF_WATER_MARK_HI_SHIFT		16

#define	ALC_THROUGHPUT_MON		0x159C
#define	THROUGHPUT_MON_RATE_MASK	0x00000003
#define	THROUGHPUT_MON_ENB		0x00000080
#define	THROUGHPUT_MON_RATE_SHIFT	0

#define	ALC_RXQ_CFG			0x15A0
#define	RXQ_CFG_ASPM_THROUGHPUT_LIMIT_MASK	0x00000003
#define	RXQ_CFG_ASPM_THROUGHPUT_LIMIT_NONE	0x00000000
#define	RXQ_CFG_ASPM_THROUGHPUT_LIMIT_1M	0x00000001
#define	RXQ_CFG_ASPM_THROUGHPUT_LIMIT_10M	0x00000002
#define	RXQ_CFG_ASPM_THROUGHPUT_LIMIT_100M	0x00000003
#define	RXQ_CFG_QUEUE1_ENB		0x00000010
#define	RXQ_CFG_QUEUE2_ENB		0x00000020
#define	RXQ_CFG_QUEUE3_ENB		0x00000040
#define	RXQ_CFG_IPV6_CSUM_ENB		0x00000080
#define	RXQ_CFG_RSS_HASH_TBL_LEN_MASK	0x0000FF00
#define	RXQ_CFG_RSS_HASH_IPV4		0x00010000
#define	RXQ_CFG_RSS_HASH_IPV4_TCP	0x00020000
#define	RXQ_CFG_RSS_HASH_IPV6		0x00040000
#define	RXQ_CFG_RSS_HASH_IPV6_TCP	0x00080000
#define	RXQ_CFG_RD_BURST_MASK		0x03F00000
#define	RXQ_CFG_RSS_MODE_DIS		0x00000000
#define	RXQ_CFG_RSS_MODE_SQSINT		0x04000000
#define	RXQ_CFG_RSS_MODE_MQUESINT	0x08000000
#define	RXQ_CFG_RSS_MODE_MQUEMINT	0x0C000000
#define	RXQ_CFG_NIP_QUEUE_SEL_TBL	0x10000000
#define	RXQ_CFG_RSS_HASH_ENB		0x20000000
#define	RXQ_CFG_CUT_THROUGH_ENB		0x40000000
#define	RXQ_CFG_QUEUE0_ENB		0x80000000
#define	RXQ_CFG_RSS_HASH_TBL_LEN_SHIFT	8
#define	RXQ_CFG_RD_BURST_DEFAULT	8
#define	RXQ_CFG_RD_BURST_SHIFT		20
#define	RXQ_CFG_ENB					\
	(RXQ_CFG_QUEUE0_ENB | RXQ_CFG_QUEUE1_ENB |	\
	 RXQ_CFG_QUEUE2_ENB | RXQ_CFG_QUEUE3_ENB)

#define	ALC_RX_RD_FREE_THRESH		0x15A4	/* 8 bytes unit. */
#define	RX_RD_FREE_THRESH_HI_MASK	0x0000003F
#define	RX_RD_FREE_THRESH_LO_MASK	0x00000FC0
#define	RX_RD_FREE_THRESH_HI_SHIFT	0
#define	RX_RD_FREE_THRESH_LO_SHIFT	6
#define	RX_RD_FREE_THRESH_HI_DEFAULT	16
#define	RX_RD_FREE_THRESH_LO_DEFAULT	8

#define	ALC_RX_FIFO_PAUSE_THRESH	0x15A8
#define	RX_FIFO_PAUSE_THRESH_LO_MASK	0x00000FFF
#define	RX_FIFO_PAUSE_THRESH_HI_MASK	0x0FFF0000
#define	RX_FIFO_PAUSE_THRESH_LO_SHIFT	0
#define	RX_FIFO_PAUSE_THRESH_HI_SHIFT	16

#define	ALC_RD_DMA_CFG			0x15AC
#define	RD_DMA_CFG_THRESH_MASK		0x00000FFF	/* 8 bytes unit */
#define	RD_DMA_CFG_TIMER_MASK		0xFFFF0000
#define	RD_DMA_CFG_THRESH_SHIFT		0
#define	RD_DMA_CFG_TIMER_SHIFT		16
#define	RD_DMA_CFG_THRESH_DEFAULT	0x100
#define	RD_DMA_CFG_TIMER_DEFAULT	0
#define	RD_DMA_CFG_TICK_USECS		8
#define	ALC_RD_DMA_CFG_USECS(x)		((x) / RD_DMA_CFG_TICK_USECS)

#define	ALC_RSS_HASH_VALUE		0x15B0

#define	ALC_RSS_HASH_FLAG		0x15B4

#define	ALC_RSS_CPU			0x15B8

#define	ALC_DMA_CFG			0x15C0
#define	DMA_CFG_IN_ORDER		0x00000001
#define	DMA_CFG_ENH_ORDER		0x00000002
#define	DMA_CFG_OUT_ORDER		0x00000004
#define	DMA_CFG_RCB_64			0x00000000
#define	DMA_CFG_RCB_128			0x00000008
#define	DMA_CFG_RD_BURST_128		0x00000000
#define	DMA_CFG_RD_BURST_256		0x00000010
#define	DMA_CFG_RD_BURST_512		0x00000020
#define	DMA_CFG_RD_BURST_1024		0x00000030
#define	DMA_CFG_RD_BURST_2048		0x00000040
#define	DMA_CFG_RD_BURST_4096		0x00000050
#define	DMA_CFG_WR_BURST_128		0x00000000
#define	DMA_CFG_WR_BURST_256		0x00000080
#define	DMA_CFG_WR_BURST_512		0x00000100
#define	DMA_CFG_WR_BURST_1024		0x00000180
#define	DMA_CFG_WR_BURST_2048		0x00000200
#define	DMA_CFG_WR_BURST_4096		0x00000280
#define	DMA_CFG_RD_REQ_PRI		0x00000400
#define	DMA_CFG_RD_DELAY_CNT_MASK	0x0000F800
#define	DMA_CFG_WR_DELAY_CNT_MASK	0x000F0000
#define	DMA_CFG_CMB_ENB			0x00100000
#define	DMA_CFG_SMB_ENB			0x00200000
#define	DMA_CFG_CMB_NOW			0x00400000
#define	DMA_CFG_SMB_DIS			0x01000000
#define	DMA_CFG_SMB_NOW			0x80000000
#define	DMA_CFG_RD_BURST_MASK		0x07
#define	DMA_CFG_RD_BURST_SHIFT		4
#define	DMA_CFG_WR_BURST_MASK		0x07
#define	DMA_CFG_WR_BURST_SHIFT		7
#define	DMA_CFG_RD_DELAY_CNT_SHIFT	11
#define	DMA_CFG_WR_DELAY_CNT_SHIFT	16
#define	DMA_CFG_RD_DELAY_CNT_DEFAULT	15
#define	DMA_CFG_WR_DELAY_CNT_DEFAULT	4

#define	ALC_SMB_STAT_TIMER		0x15C4
#define	SMB_STAT_TIMER_MASK		0x00FFFFFF
#define	SMB_STAT_TIMER_SHIFT		0

#define	ALC_CMB_TD_THRESH		0x15C8
#define	CMB_TD_THRESH_MASK		0x0000FFFF
#define	CMB_TD_THRESH_SHIFT		0

#define	ALC_CMB_TX_TIMER		0x15CC
#define	CMB_TX_TIMER_MASK		0x0000FFFF
#define	CMB_TX_TIMER_SHIFT		0

#define	ALC_MBOX_RD0_PROD_IDX		0x15E0

#define	ALC_MBOX_RD1_PROD_IDX		0x15E4

#define	ALC_MBOX_RD2_PROD_IDX		0x15E8

#define	ALC_MBOX_RD3_PROD_IDX		0x15EC

#define	ALC_MBOX_RD_PROD_MASK		0x0000FFFF
#define	MBOX_RD_PROD_SHIFT		0

#define	ALC_MBOX_TD_PROD_IDX		0x15F0
#define	MBOX_TD_PROD_HI_IDX_MASK	0x0000FFFF
#define	MBOX_TD_PROD_LO_IDX_MASK	0xFFFF0000
#define	MBOX_TD_PROD_HI_IDX_SHIFT	0
#define	MBOX_TD_PROD_LO_IDX_SHIFT	16

#define	ALC_MBOX_TD_CONS_IDX		0x15F4
#define	MBOX_TD_CONS_HI_IDX_MASK	0x0000FFFF
#define	MBOX_TD_CONS_LO_IDX_MASK	0xFFFF0000
#define	MBOX_TD_CONS_HI_IDX_SHIFT	0
#define	MBOX_TD_CONS_LO_IDX_SHIFT	16

#define	ALC_MBOX_RD01_CONS_IDX		0x15F8
#define	MBOX_RD0_CONS_IDX_MASK		0x0000FFFF
#define	MBOX_RD1_CONS_IDX_MASK		0xFFFF0000
#define	MBOX_RD0_CONS_IDX_SHIFT		0
#define	MBOX_RD1_CONS_IDX_SHIFT		16

#define	ALC_MBOX_RD23_CONS_IDX		0x15FC
#define	MBOX_RD2_CONS_IDX_MASK		0x0000FFFF
#define	MBOX_RD3_CONS_IDX_MASK		0xFFFF0000
#define	MBOX_RD2_CONS_IDX_SHIFT		0
#define	MBOX_RD3_CONS_IDX_SHIFT		16

#define	ALC_INTR_STATUS			0x1600
#define	INTR_SMB			0x00000001
#define	INTR_TIMER			0x00000002
#define	INTR_MANUAL_TIMER		0x00000004
#define	INTR_RX_FIFO_OFLOW		0x00000008
#define	INTR_RD0_UNDERRUN		0x00000010
#define	INTR_RD1_UNDERRUN		0x00000020
#define	INTR_RD2_UNDERRUN		0x00000040
#define	INTR_RD3_UNDERRUN		0x00000080
#define	INTR_TX_FIFO_UNDERRUN		0x00000100
#define	INTR_DMA_RD_TO_RST		0x00000200
#define	INTR_DMA_WR_TO_RST		0x00000400
#define	INTR_TX_CREDIT			0x00000800
#define	INTR_GPHY			0x00001000
#define	INTR_GPHY_LOW_PW		0x00002000
#define	INTR_TXQ_TO_RST			0x00004000
#define	INTR_TX_PKT			0x00008000
#define	INTR_RX_PKT0			0x00010000
#define	INTR_RX_PKT1			0x00020000
#define	INTR_RX_PKT2			0x00040000
#define	INTR_RX_PKT3			0x00080000
#define	INTR_MAC_RX			0x00100000
#define	INTR_MAC_TX			0x00200000
#define	INTR_UNDERRUN			0x00400000
#define	INTR_FRAME_ERROR		0x00800000
#define	INTR_FRAME_OK			0x01000000
#define	INTR_CSUM_ERROR			0x02000000
#define	INTR_PHY_LINK_DOWN		0x04000000
#define	INTR_DIS_INT			0x80000000

/* Interrupt Mask Register */
#define	ALC_INTR_MASK			0x1604

#ifdef	notyet
#define	INTR_RX_PKT					\
	(INTR_RX_PKT0 | INTR_RX_PKT1 | INTR_RX_PKT2 |	\
	 INTR_RX_PKT3)
#define	INTR_RD_UNDERRUN				\
	(INTR_RD0_UNDERRUN | INTR_RD1_UNDERRUN |	\
	INTR_RD2_UNDERRUN | INTR_RD3_UNDERRUN)
#else
#define	INTR_RX_PKT			INTR_RX_PKT0
#define	INTR_RD_UNDERRUN		INTR_RD0_UNDERRUN
#endif

#define	ALC_INTRS					\
	(INTR_DMA_RD_TO_RST | INTR_DMA_WR_TO_RST |	\
	INTR_TXQ_TO_RST	| INTR_RX_PKT | INTR_TX_PKT |	\
	INTR_RX_FIFO_OFLOW | INTR_RD_UNDERRUN |		\
	INTR_TX_FIFO_UNDERRUN)

#define	ALC_INTR_RETRIG_TIMER		0x1608
#define	INTR_RETRIG_TIMER_MASK		0x0000FFFF
#define	INTR_RETRIG_TIMER_SHIFT		0

#define	ALC_HDS_CFG			0x160C
#define	HDS_CFG_ENB			0x00000001
#define	HDS_CFG_BACKFILLSIZE_MASK	0x000FFF00
#define	HDS_CFG_MAX_HDRSIZE_MASK	0xFFF00000
#define	HDS_CFG_BACKFILLSIZE_SHIFT	8
#define	HDS_CFG_MAX_HDRSIZE_SHIFT	20

/* AR8131/AR8132 registers for MAC statistics */
#define	ALC_RX_MIB_BASE			0x1700

#define	ALC_TX_MIB_BASE			0x1760

#define	ALC_DEBUG_DATA0			0x1900

#define	ALC_DEBUG_DATA1			0x1904

#define	ALC_MII_DBG_ADDR		0x1D
#define	ALC_MII_DBG_DATA		0x1E

#define	MII_ANA_CFG0			0x00
#define	ANA_RESTART_CAL			0x0001
#define	ANA_MANUL_SWICH_ON_MASK		0x001E
#define	ANA_MAN_ENABLE			0x0020
#define	ANA_SEL_HSP			0x0040
#define	ANA_EN_HB			0x0080
#define	ANA_EN_HBIAS			0x0100
#define	ANA_OEN_125M			0x0200
#define	ANA_EN_LCKDT			0x0400
#define	ANA_LCKDT_PHY			0x0800
#define	ANA_AFE_MODE			0x1000
#define	ANA_VCO_SLOW			0x2000
#define	ANA_VCO_FAST			0x4000
#define	ANA_SEL_CLK125M_DSP		0x8000
#define	ANA_MANUL_SWICH_ON_SHIFT	1

#define	MII_ANA_CFG4			0x04
#define	ANA_IECHO_ADJ_MASK		0x0F
#define	ANA_IECHO_ADJ_3_MASK		0x000F
#define	ANA_IECHO_ADJ_2_MASK		0x00F0
#define	ANA_IECHO_ADJ_1_MASK		0x0F00
#define	ANA_IECHO_ADJ_0_MASK		0xF000
#define	ANA_IECHO_ADJ_3_SHIFT		0
#define	ANA_IECHO_ADJ_2_SHIFT		4
#define	ANA_IECHO_ADJ_1_SHIFT		8
#define	ANA_IECHO_ADJ_0_SHIFT		12

#define	MII_ANA_CFG5			0x05
#define	ANA_SERDES_CDR_BW_MASK		0x0003
#define	ANA_MS_PAD_DBG			0x0004
#define	ANA_SPEEDUP_DBG			0x0008
#define	ANA_SERDES_TH_LOS_MASK		0x0030
#define	ANA_SERDES_EN_DEEM		0x0040
#define	ANA_SERDES_TXELECIDLE		0x0080
#define	ANA_SERDES_BEACON		0x0100
#define	ANA_SERDES_HALFTXDR		0x0200
#define	ANA_SERDES_SEL_HSP		0x0400
#define	ANA_SERDES_EN_PLL		0x0800
#define	ANA_SERDES_EN			0x1000
#define	ANA_SERDES_EN_LCKDT		0x2000
#define	ANA_SERDES_CDR_BW_SHIFT		0
#define	ANA_SERDES_TH_LOS_SHIFT		4

#define	MII_ANA_CFG11			0x0B
#define	ANA_PS_HIB_EN			0x8000

#define	MII_ANA_CFG18			0x12
#define	ANA_TEST_MODE_10BT_01MASK	0x0003
#define	ANA_LOOP_SEL_10BT		0x0004
#define	ANA_RGMII_MODE_SW		0x0008
#define	ANA_EN_LONGECABLE		0x0010
#define	ANA_TEST_MODE_10BT_2		0x0020
#define	ANA_EN_10BT_IDLE		0x0400
#define	ANA_EN_MASK_TB			0x0800
#define	ANA_TRIGGER_SEL_TIMER_MASK	0x3000
#define	ANA_INTERVAL_SEL_TIMER_MASK	0xC000
#define	ANA_TEST_MODE_10BT_01SHIFT	0
#define	ANA_TRIGGER_SEL_TIMER_SHIFT	12
#define	ANA_INTERVAL_SEL_TIMER_SHIFT	14

#define	MII_ANA_CFG41			0x29
#define	ANA_TOP_PS_EN			0x8000

#define	MII_ANA_CFG54			0x36
#define	ANA_LONG_CABLE_TH_100_MASK	0x003F
#define	ANA_DESERVED			0x0040
#define	ANA_EN_LIT_CH			0x0080
#define	ANA_SHORT_CABLE_TH_100_MASK	0x3F00
#define	ANA_BP_BAD_LINK_ACCUM		0x4000
#define	ANA_BP_SMALL_BW			0x8000
#define	ANA_LONG_CABLE_TH_100_SHIFT	0
#define	ANA_SHORT_CABLE_TH_100_SHIFT	8

/* Statistics counters collected by the MAC. */
struct smb {
	/* Rx stats. */
	uint32_t rx_frames;
	uint32_t rx_bcast_frames;
	uint32_t rx_mcast_frames;
	uint32_t rx_pause_frames;
	uint32_t rx_control_frames;
	uint32_t rx_crcerrs;
	uint32_t rx_lenerrs;
	uint32_t rx_bytes;
	uint32_t rx_runts;
	uint32_t rx_fragments;
	uint32_t rx_pkts_64;
	uint32_t rx_pkts_65_127;
	uint32_t rx_pkts_128_255;
	uint32_t rx_pkts_256_511;
	uint32_t rx_pkts_512_1023;
	uint32_t rx_pkts_1024_1518;
	uint32_t rx_pkts_1519_max;
	uint32_t rx_pkts_truncated;
	uint32_t rx_fifo_oflows;
	uint32_t rx_rrs_errs;
	uint32_t rx_alignerrs;
	uint32_t rx_bcast_bytes;
	uint32_t rx_mcast_bytes;
	uint32_t rx_pkts_filtered;
	/* Tx stats. */
	uint32_t tx_frames;
	uint32_t tx_bcast_frames;
	uint32_t tx_mcast_frames;
	uint32_t tx_pause_frames;
	uint32_t tx_excess_defer;
	uint32_t tx_control_frames;
	uint32_t tx_deferred;
	uint32_t tx_bytes;
	uint32_t tx_pkts_64;
	uint32_t tx_pkts_65_127;
	uint32_t tx_pkts_128_255;
	uint32_t tx_pkts_256_511;
	uint32_t tx_pkts_512_1023;
	uint32_t tx_pkts_1024_1518;
	uint32_t tx_pkts_1519_max;
	uint32_t tx_single_colls;
	uint32_t tx_multi_colls;
	uint32_t tx_late_colls;
	uint32_t tx_excess_colls;
	uint32_t tx_abort;
	uint32_t tx_underrun;
	uint32_t tx_desc_underrun;
	uint32_t tx_lenerrs;
	uint32_t tx_pkts_truncated;
	uint32_t tx_bcast_bytes;
	uint32_t tx_mcast_bytes;
	uint32_t updated;
};

/* CMB(Coalesing message block) */
struct cmb {
	uint32_t cons;
};

/* Rx free descriptor */
struct rx_desc {
	uint64_t addr;
};

/* Rx return descriptor */
struct rx_rdesc {
	uint32_t rdinfo;
#define	RRD_CSUM_MASK			0x0000FFFF
#define	RRD_RD_CNT_MASK			0x000F0000
#define	RRD_RD_IDX_MASK			0xFFF00000
#define	RRD_CSUM_SHIFT			0
#define	RRD_RD_CNT_SHIFT		16
#define	RRD_RD_IDX_SHIFT		20
#define	RRD_CSUM(x)			\
	(((x) & RRD_CSUM_MASK) >> RRD_CSUM_SHIFT)
#define	RRD_RD_CNT(x)			\
	(((x) & RRD_RD_CNT_MASK) >> RRD_RD_CNT_SHIFT)
#define	RRD_RD_IDX(x)			\
	(((x) & RRD_RD_IDX_MASK) >> RRD_RD_IDX_SHIFT)
	uint32_t rss;
	uint32_t vtag;
#define	RRD_VLAN_MASK			0x0000FFFF
#define	RRD_HEAD_LEN_MASK		0x00FF0000
#define	RRD_HDS_MASK			0x03000000
#define	RRD_HDS_NONE			0x00000000
#define	RRD_HDS_HEAD			0x01000000
#define	RRD_HDS_DATA			0x02000000
#define	RRD_CPU_MASK			0x0C000000
#define	RRD_HASH_FLAG_MASK		0xF0000000
#define	RRD_VLAN_SHIFT			0
#define	RRD_HEAD_LEN_SHIFT		16
#define	RRD_HDS_SHIFT			24
#define	RRD_CPU_SHIFT			26
#define	RRD_HASH_FLAG_SHIFT		28
#define	RRD_VLAN(x)			\
	(((x) & RRD_VLAN_MASK) >> RRD_VLAN_SHIFT)
#define	RRD_HEAD_LEN(x)			\
	(((x) & RRD_HEAD_LEN_MASK) >> RRD_HEAD_LEN_SHIFT)
#define	RRD_CPU(x)			\
	(((x) & RRD_CPU_MASK) >> RRD_CPU_SHIFT)
	uint32_t status;
#define	RRD_LEN_MASK			0x00003FFF
#define	RRD_LEN_SHIFT			0
#define	RRD_TCP_UDPCSUM_NOK		0x00004000
#define	RRD_IPCSUM_NOK			0x00008000
#define	RRD_VLAN_TAG			0x00010000
#define	RRD_PROTO_MASK			0x000E0000
#define	RRD_PROTO_IPV4			0x00020000
#define	RRD_PROTO_IPV6			0x000C0000
#define	RRD_ERR_SUM			0x00100000
#define	RRD_ERR_CRC			0x00200000
#define	RRD_ERR_ALIGN			0x00400000
#define	RRD_ERR_TRUNC			0x00800000
#define	RRD_ERR_RUNT			0x01000000
#define	RRD_ERR_ICMP			0x02000000
#define	RRD_BCAST			0x04000000
#define	RRD_MCAST			0x08000000
#define	RRD_SNAP_LLC			0x10000000
#define	RRD_ETHER			0x00000000
#define	RRD_FIFO_FULL			0x20000000
#define	RRD_ERR_LENGTH			0x40000000
#define	RRD_VALID			0x80000000
#define	RRD_BYTES(x)			\
	(((x) & RRD_LEN_MASK) >> RRD_LEN_SHIFT)
#define	RRD_IPV4(x)			\
	(((x) & RRD_PROTO_MASK) == RRD_PROTO_IPV4)
};

/* Tx descriptor */
struct tx_desc {
	uint32_t len;
#define	TD_BUFLEN_MASK			0x00003FFF
#define	TD_VLAN_MASK			0xFFFF0000
#define	TD_BUFLEN_SHIFT			0
#define	TX_BYTES(x)			\
	(((x) << TD_BUFLEN_SHIFT) & TD_BUFLEN_MASK)
#define	TD_VLAN_SHIFT			16
	uint32_t flags;
#define	TD_L4HDR_OFFSET_MASK		0x000000FF	/* byte unit */
#define	TD_TCPHDR_OFFSET_MASK		0x000000FF	/* byte unit */
#define	TD_PLOAD_OFFSET_MASK		0x000000FF	/* 2 bytes unit */
#define	TD_CUSTOM_CSUM			0x00000100
#define	TD_IPCSUM			0x00000200
#define	TD_TCPCSUM			0x00000400
#define	TD_UDPCSUM			0x00000800
#define	TD_TSO				0x00001000
#define	TD_TSO_DESCV1			0x00000000
#define	TD_TSO_DESCV2			0x00002000
#define	TD_CON_VLAN_TAG			0x00004000
#define	TD_INS_VLAN_TAG			0x00008000
#define	TD_IPV4_DESCV2			0x00010000
#define	TD_LLC_SNAP			0x00020000
#define	TD_ETHERNET			0x00000000
#define	TD_CUSTOM_CSUM_OFFSET_MASK	0x03FC0000	/* 2 bytes unit */
#define	TD_CUSTOM_CSUM_EVEN_PAD		0x40000000
#define	TD_MSS_MASK			0x7FFC0000
#define	TD_EOP				0x80000000
#define	TD_L4HDR_OFFSET_SHIFT		0
#define	TD_TCPHDR_OFFSET_SHIFT		0
#define	TD_PLOAD_OFFSET_SHIFT		0
#define	TD_CUSTOM_CSUM_OFFSET_SHIFT	18
#define	TD_MSS_SHIFT			18
	uint64_t addr;
};

#endif	/* _IF_ALCREG_H */
