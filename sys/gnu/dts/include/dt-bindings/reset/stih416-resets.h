/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This header provides constants for the reset controller
 * based peripheral powerdown requests on the STMicroelectronics
 * STiH416 SoC.
 */
#ifndef _DT_BINDINGS_RESET_CONTROLLER_STIH416
#define _DT_BINDINGS_RESET_CONTROLLER_STIH416

#define STIH416_EMISS_POWERDOWN		0
#define STIH416_NAND_POWERDOWN		1
#define STIH416_KEYSCAN_POWERDOWN	2
#define STIH416_USB0_POWERDOWN		3
#define STIH416_USB1_POWERDOWN		4
#define STIH416_USB2_POWERDOWN		5
#define STIH416_USB3_POWERDOWN		6
#define STIH416_SATA0_POWERDOWN		7
#define STIH416_SATA1_POWERDOWN		8
#define STIH416_PCIE0_POWERDOWN		9
#define STIH416_PCIE1_POWERDOWN		10

#define STIH416_ETH0_SOFTRESET		0
#define STIH416_ETH1_SOFTRESET		1
#define STIH416_IRB_SOFTRESET		2
#define STIH416_USB0_SOFTRESET		3
#define STIH416_USB1_SOFTRESET		4
#define STIH416_USB2_SOFTRESET		5
#define STIH416_USB3_SOFTRESET		6
#define STIH416_SATA0_SOFTRESET		7
#define STIH416_SATA1_SOFTRESET		8
#define STIH416_PCIE0_SOFTRESET		9
#define STIH416_PCIE1_SOFTRESET		10
#define STIH416_AUD_DAC_SOFTRESET	11
#define STIH416_HDTVOUT_SOFTRESET	12
#define STIH416_VTAC_M_RX_SOFTRESET	13
#define STIH416_VTAC_A_RX_SOFTRESET	14
#define STIH416_SYNC_HD_SOFTRESET	15
#define STIH416_SYNC_SD_SOFTRESET	16
#define STIH416_BLITTER_SOFTRESET	17
#define STIH416_GPU_SOFTRESET		18
#define STIH416_VTAC_M_TX_SOFTRESET	19
#define STIH416_VTAC_A_TX_SOFTRESET	20
#define STIH416_VTG_AUX_SOFTRESET	21
#define STIH416_JPEG_DEC_SOFTRESET	22
#define STIH416_HVA_SOFTRESET		23
#define STIH416_COMPO_M_SOFTRESET	24
#define STIH416_COMPO_A_SOFTRESET	25
#define STIH416_VP8_DEC_SOFTRESET	26
#define STIH416_VTG_MAIN_SOFTRESET	27
#define STIH416_KEYSCAN_SOFTRESET	28

#endif /* _DT_BINDINGS_RESET_CONTROLLER_STIH416 */
