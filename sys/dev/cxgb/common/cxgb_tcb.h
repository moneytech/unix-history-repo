/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD$

***************************************************************************/

/* This file is automatically generated --- do not edit */

#ifndef _TCB_DEFS_H
#define _TCB_DEFS_H

#define W_TCB_T_STATE    0
#define S_TCB_T_STATE    0
#define M_TCB_T_STATE    0xfULL
#define V_TCB_T_STATE(x) ((x) << S_TCB_T_STATE)

#define W_TCB_TIMER    0
#define S_TCB_TIMER    4
#define M_TCB_TIMER    0x1ULL
#define V_TCB_TIMER(x) ((x) << S_TCB_TIMER)

#define W_TCB_DACK_TIMER    0
#define S_TCB_DACK_TIMER    5
#define M_TCB_DACK_TIMER    0x1ULL
#define V_TCB_DACK_TIMER(x) ((x) << S_TCB_DACK_TIMER)

#define W_TCB_DEL_FLAG    0
#define S_TCB_DEL_FLAG    6
#define M_TCB_DEL_FLAG    0x1ULL
#define V_TCB_DEL_FLAG(x) ((x) << S_TCB_DEL_FLAG)

#define W_TCB_L2T_IX    0
#define S_TCB_L2T_IX    7
#define M_TCB_L2T_IX    0x7ffULL
#define V_TCB_L2T_IX(x) ((x) << S_TCB_L2T_IX)

#define W_TCB_SMAC_SEL    0
#define S_TCB_SMAC_SEL    18
#define M_TCB_SMAC_SEL    0x3ULL
#define V_TCB_SMAC_SEL(x) ((x) << S_TCB_SMAC_SEL)

#define W_TCB_TOS    0
#define S_TCB_TOS    20
#define M_TCB_TOS    0x3fULL
#define V_TCB_TOS(x) ((x) << S_TCB_TOS)

#define W_TCB_MAX_RT    0
#define S_TCB_MAX_RT    26
#define M_TCB_MAX_RT    0xfULL
#define V_TCB_MAX_RT(x) ((x) << S_TCB_MAX_RT)

#define W_TCB_T_RXTSHIFT    0
#define S_TCB_T_RXTSHIFT    30
#define M_TCB_T_RXTSHIFT    0xfULL
#define V_TCB_T_RXTSHIFT(x) ((x) << S_TCB_T_RXTSHIFT)

#define W_TCB_T_DUPACKS    1
#define S_TCB_T_DUPACKS    2
#define M_TCB_T_DUPACKS    0xfULL
#define V_TCB_T_DUPACKS(x) ((x) << S_TCB_T_DUPACKS)

#define W_TCB_T_MAXSEG    1
#define S_TCB_T_MAXSEG    6
#define M_TCB_T_MAXSEG    0xfULL
#define V_TCB_T_MAXSEG(x) ((x) << S_TCB_T_MAXSEG)

#define W_TCB_T_FLAGS1    1
#define S_TCB_T_FLAGS1    10
#define M_TCB_T_FLAGS1    0xffffffffULL
#define V_TCB_T_FLAGS1(x) ((x) << S_TCB_T_FLAGS1)

#define W_TCB_T_FLAGS2    2
#define S_TCB_T_FLAGS2    10
#define M_TCB_T_FLAGS2    0x7fULL
#define V_TCB_T_FLAGS2(x) ((x) << S_TCB_T_FLAGS2)

#define W_TCB_SND_SCALE    2
#define S_TCB_SND_SCALE    17
#define M_TCB_SND_SCALE    0xfULL
#define V_TCB_SND_SCALE(x) ((x) << S_TCB_SND_SCALE)

#define W_TCB_RCV_SCALE    2
#define S_TCB_RCV_SCALE    21
#define M_TCB_RCV_SCALE    0xfULL
#define V_TCB_RCV_SCALE(x) ((x) << S_TCB_RCV_SCALE)

#define W_TCB_SND_UNA_RAW    2
#define S_TCB_SND_UNA_RAW    25
#define M_TCB_SND_UNA_RAW    0x7ffffffULL
#define V_TCB_SND_UNA_RAW(x) ((x) << S_TCB_SND_UNA_RAW)

#define W_TCB_SND_NXT_RAW    3
#define S_TCB_SND_NXT_RAW    20
#define M_TCB_SND_NXT_RAW    0x7ffffffULL
#define V_TCB_SND_NXT_RAW(x) ((x) << S_TCB_SND_NXT_RAW)

#define W_TCB_RCV_NXT    4
#define S_TCB_RCV_NXT    15
#define M_TCB_RCV_NXT    0xffffffffULL
#define V_TCB_RCV_NXT(x) ((x) << S_TCB_RCV_NXT)

#define W_TCB_RCV_ADV    5
#define S_TCB_RCV_ADV    15
#define M_TCB_RCV_ADV    0xffffULL
#define V_TCB_RCV_ADV(x) ((x) << S_TCB_RCV_ADV)

#define W_TCB_SND_MAX_RAW    5
#define S_TCB_SND_MAX_RAW    31
#define M_TCB_SND_MAX_RAW    0x7ffffffULL
#define V_TCB_SND_MAX_RAW(x) ((x) << S_TCB_SND_MAX_RAW)

#define W_TCB_SND_CWND    6
#define S_TCB_SND_CWND    26
#define M_TCB_SND_CWND    0x7ffffffULL
#define V_TCB_SND_CWND(x) ((x) << S_TCB_SND_CWND)

#define W_TCB_SND_SSTHRESH    7
#define S_TCB_SND_SSTHRESH    21
#define M_TCB_SND_SSTHRESH    0x7ffffffULL
#define V_TCB_SND_SSTHRESH(x) ((x) << S_TCB_SND_SSTHRESH)

#define W_TCB_T_RTT_TS_RECENT_AGE    8
#define S_TCB_T_RTT_TS_RECENT_AGE    16
#define M_TCB_T_RTT_TS_RECENT_AGE    0xffffffffULL
#define V_TCB_T_RTT_TS_RECENT_AGE(x) ((x) << S_TCB_T_RTT_TS_RECENT_AGE)

#define W_TCB_T_RTSEQ_RECENT    9
#define S_TCB_T_RTSEQ_RECENT    16
#define M_TCB_T_RTSEQ_RECENT    0xffffffffULL
#define V_TCB_T_RTSEQ_RECENT(x) ((x) << S_TCB_T_RTSEQ_RECENT)

#define W_TCB_T_SRTT    10
#define S_TCB_T_SRTT    16
#define M_TCB_T_SRTT    0xffffULL
#define V_TCB_T_SRTT(x) ((x) << S_TCB_T_SRTT)

#define W_TCB_T_RTTVAR    11
#define S_TCB_T_RTTVAR    0
#define M_TCB_T_RTTVAR    0xffffULL
#define V_TCB_T_RTTVAR(x) ((x) << S_TCB_T_RTTVAR)

#define W_TCB_TS_LAST_ACK_SENT_RAW    11
#define S_TCB_TS_LAST_ACK_SENT_RAW    16
#define M_TCB_TS_LAST_ACK_SENT_RAW    0x7ffffffULL
#define V_TCB_TS_LAST_ACK_SENT_RAW(x) ((x) << S_TCB_TS_LAST_ACK_SENT_RAW)

#define W_TCB_DIP    12
#define S_TCB_DIP    11
#define M_TCB_DIP    0xffffffffULL
#define V_TCB_DIP(x) ((x) << S_TCB_DIP)

#define W_TCB_SIP    13
#define S_TCB_SIP    11
#define M_TCB_SIP    0xffffffffULL
#define V_TCB_SIP(x) ((x) << S_TCB_SIP)

#define W_TCB_DP    14
#define S_TCB_DP    11
#define M_TCB_DP    0xffffULL
#define V_TCB_DP(x) ((x) << S_TCB_DP)

#define W_TCB_SP    14
#define S_TCB_SP    27
#define M_TCB_SP    0xffffULL
#define V_TCB_SP(x) ((x) << S_TCB_SP)

#define W_TCB_TIMESTAMP    15
#define S_TCB_TIMESTAMP    11
#define M_TCB_TIMESTAMP    0xffffffffULL
#define V_TCB_TIMESTAMP(x) ((x) << S_TCB_TIMESTAMP)

#define W_TCB_TIMESTAMP_OFFSET    16
#define S_TCB_TIMESTAMP_OFFSET    11
#define M_TCB_TIMESTAMP_OFFSET    0xfULL
#define V_TCB_TIMESTAMP_OFFSET(x) ((x) << S_TCB_TIMESTAMP_OFFSET)

#define W_TCB_TX_MAX    16
#define S_TCB_TX_MAX    15
#define M_TCB_TX_MAX    0xffffffffULL
#define V_TCB_TX_MAX(x) ((x) << S_TCB_TX_MAX)

#define W_TCB_TX_HDR_PTR_RAW    17
#define S_TCB_TX_HDR_PTR_RAW    15
#define M_TCB_TX_HDR_PTR_RAW    0x1ffffULL
#define V_TCB_TX_HDR_PTR_RAW(x) ((x) << S_TCB_TX_HDR_PTR_RAW)

#define W_TCB_TX_LAST_PTR_RAW    18
#define S_TCB_TX_LAST_PTR_RAW    0
#define M_TCB_TX_LAST_PTR_RAW    0x1ffffULL
#define V_TCB_TX_LAST_PTR_RAW(x) ((x) << S_TCB_TX_LAST_PTR_RAW)

#define W_TCB_TX_COMPACT    18
#define S_TCB_TX_COMPACT    17
#define M_TCB_TX_COMPACT    0x1ULL
#define V_TCB_TX_COMPACT(x) ((x) << S_TCB_TX_COMPACT)

#define W_TCB_RX_COMPACT    18
#define S_TCB_RX_COMPACT    18
#define M_TCB_RX_COMPACT    0x1ULL
#define V_TCB_RX_COMPACT(x) ((x) << S_TCB_RX_COMPACT)

#define W_TCB_RCV_WND    18
#define S_TCB_RCV_WND    19
#define M_TCB_RCV_WND    0x7ffffffULL
#define V_TCB_RCV_WND(x) ((x) << S_TCB_RCV_WND)

#define W_TCB_RX_HDR_OFFSET    19
#define S_TCB_RX_HDR_OFFSET    14
#define M_TCB_RX_HDR_OFFSET    0x7ffffffULL
#define V_TCB_RX_HDR_OFFSET(x) ((x) << S_TCB_RX_HDR_OFFSET)

#define W_TCB_RX_FRAG0_START_IDX_RAW    20
#define S_TCB_RX_FRAG0_START_IDX_RAW    9
#define M_TCB_RX_FRAG0_START_IDX_RAW    0x7ffffffULL
#define V_TCB_RX_FRAG0_START_IDX_RAW(x) ((x) << S_TCB_RX_FRAG0_START_IDX_RAW)

#define W_TCB_RX_FRAG1_START_IDX_OFFSET    21
#define S_TCB_RX_FRAG1_START_IDX_OFFSET    4
#define M_TCB_RX_FRAG1_START_IDX_OFFSET    0x7ffffffULL
#define V_TCB_RX_FRAG1_START_IDX_OFFSET(x) ((x) << S_TCB_RX_FRAG1_START_IDX_OFFSET)

#define W_TCB_RX_FRAG0_LEN    21
#define S_TCB_RX_FRAG0_LEN    31
#define M_TCB_RX_FRAG0_LEN    0x7ffffffULL
#define V_TCB_RX_FRAG0_LEN(x) ((x) << S_TCB_RX_FRAG0_LEN)

#define W_TCB_RX_FRAG1_LEN    22
#define S_TCB_RX_FRAG1_LEN    26
#define M_TCB_RX_FRAG1_LEN    0x7ffffffULL
#define V_TCB_RX_FRAG1_LEN(x) ((x) << S_TCB_RX_FRAG1_LEN)

#define W_TCB_NEWRENO_RECOVER    23
#define S_TCB_NEWRENO_RECOVER    21
#define M_TCB_NEWRENO_RECOVER    0x7ffffffULL
#define V_TCB_NEWRENO_RECOVER(x) ((x) << S_TCB_NEWRENO_RECOVER)

#define W_TCB_PDU_HAVE_LEN    24
#define S_TCB_PDU_HAVE_LEN    16
#define M_TCB_PDU_HAVE_LEN    0x1ULL
#define V_TCB_PDU_HAVE_LEN(x) ((x) << S_TCB_PDU_HAVE_LEN)

#define W_TCB_PDU_LEN    24
#define S_TCB_PDU_LEN    17
#define M_TCB_PDU_LEN    0xffffULL
#define V_TCB_PDU_LEN(x) ((x) << S_TCB_PDU_LEN)

#define W_TCB_RX_QUIESCE    25
#define S_TCB_RX_QUIESCE    1
#define M_TCB_RX_QUIESCE    0x1ULL
#define V_TCB_RX_QUIESCE(x) ((x) << S_TCB_RX_QUIESCE)

#define W_TCB_RX_PTR_RAW    25
#define S_TCB_RX_PTR_RAW    2
#define M_TCB_RX_PTR_RAW    0x1ffffULL
#define V_TCB_RX_PTR_RAW(x) ((x) << S_TCB_RX_PTR_RAW)

#define W_TCB_CPU_NO    25
#define S_TCB_CPU_NO    19
#define M_TCB_CPU_NO    0x7fULL
#define V_TCB_CPU_NO(x) ((x) << S_TCB_CPU_NO)

#define W_TCB_ULP_TYPE    25
#define S_TCB_ULP_TYPE    26
#define M_TCB_ULP_TYPE    0xfULL
#define V_TCB_ULP_TYPE(x) ((x) << S_TCB_ULP_TYPE)

#define W_TCB_RX_FRAG1_PTR_RAW    25
#define S_TCB_RX_FRAG1_PTR_RAW    30
#define M_TCB_RX_FRAG1_PTR_RAW    0x1ffffULL
#define V_TCB_RX_FRAG1_PTR_RAW(x) ((x) << S_TCB_RX_FRAG1_PTR_RAW)

#define W_TCB_RX_FRAG2_START_IDX_OFFSET_RAW    26
#define S_TCB_RX_FRAG2_START_IDX_OFFSET_RAW    15
#define M_TCB_RX_FRAG2_START_IDX_OFFSET_RAW    0x7ffffffULL
#define V_TCB_RX_FRAG2_START_IDX_OFFSET_RAW(x) ((x) << S_TCB_RX_FRAG2_START_IDX_OFFSET_RAW)

#define W_TCB_RX_FRAG2_PTR_RAW    27
#define S_TCB_RX_FRAG2_PTR_RAW    10
#define M_TCB_RX_FRAG2_PTR_RAW    0x1ffffULL
#define V_TCB_RX_FRAG2_PTR_RAW(x) ((x) << S_TCB_RX_FRAG2_PTR_RAW)

#define W_TCB_RX_FRAG2_LEN_RAW    27
#define S_TCB_RX_FRAG2_LEN_RAW    27
#define M_TCB_RX_FRAG2_LEN_RAW    0x7ffffffULL
#define V_TCB_RX_FRAG2_LEN_RAW(x) ((x) << S_TCB_RX_FRAG2_LEN_RAW)

#define W_TCB_RX_FRAG3_PTR_RAW    28
#define S_TCB_RX_FRAG3_PTR_RAW    22
#define M_TCB_RX_FRAG3_PTR_RAW    0x1ffffULL
#define V_TCB_RX_FRAG3_PTR_RAW(x) ((x) << S_TCB_RX_FRAG3_PTR_RAW)

#define W_TCB_RX_FRAG3_LEN_RAW    29
#define S_TCB_RX_FRAG3_LEN_RAW    7
#define M_TCB_RX_FRAG3_LEN_RAW    0x7ffffffULL
#define V_TCB_RX_FRAG3_LEN_RAW(x) ((x) << S_TCB_RX_FRAG3_LEN_RAW)

#define W_TCB_RX_FRAG3_START_IDX_OFFSET_RAW    30
#define S_TCB_RX_FRAG3_START_IDX_OFFSET_RAW    2
#define M_TCB_RX_FRAG3_START_IDX_OFFSET_RAW    0x7ffffffULL
#define V_TCB_RX_FRAG3_START_IDX_OFFSET_RAW(x) ((x) << S_TCB_RX_FRAG3_START_IDX_OFFSET_RAW)

#define W_TCB_PDU_HDR_LEN    30
#define S_TCB_PDU_HDR_LEN    29
#define M_TCB_PDU_HDR_LEN    0xffULL
#define V_TCB_PDU_HDR_LEN(x) ((x) << S_TCB_PDU_HDR_LEN)

#define W_TCB_SLUSH1    31
#define S_TCB_SLUSH1    5
#define M_TCB_SLUSH1    0x7ffffULL
#define V_TCB_SLUSH1(x) ((x) << S_TCB_SLUSH1)

#define W_TCB_ULP_RAW    31
#define S_TCB_ULP_RAW    24
#define M_TCB_ULP_RAW    0xffULL
#define V_TCB_ULP_RAW(x) ((x) << S_TCB_ULP_RAW)

#define W_TCB_DDP_RDMAP_VERSION    25
#define S_TCB_DDP_RDMAP_VERSION    30
#define M_TCB_DDP_RDMAP_VERSION    0x1ULL
#define V_TCB_DDP_RDMAP_VERSION(x) ((x) << S_TCB_DDP_RDMAP_VERSION)

#define W_TCB_MARKER_ENABLE_RX    25
#define S_TCB_MARKER_ENABLE_RX    31
#define M_TCB_MARKER_ENABLE_RX    0x1ULL
#define V_TCB_MARKER_ENABLE_RX(x) ((x) << S_TCB_MARKER_ENABLE_RX)

#define W_TCB_MARKER_ENABLE_TX    26
#define S_TCB_MARKER_ENABLE_TX    0
#define M_TCB_MARKER_ENABLE_TX    0x1ULL
#define V_TCB_MARKER_ENABLE_TX(x) ((x) << S_TCB_MARKER_ENABLE_TX)

#define W_TCB_CRC_ENABLE    26
#define S_TCB_CRC_ENABLE    1
#define M_TCB_CRC_ENABLE    0x1ULL
#define V_TCB_CRC_ENABLE(x) ((x) << S_TCB_CRC_ENABLE)

#define W_TCB_IRS_ULP    26
#define S_TCB_IRS_ULP    2
#define M_TCB_IRS_ULP    0x1ffULL
#define V_TCB_IRS_ULP(x) ((x) << S_TCB_IRS_ULP)

#define W_TCB_ISS_ULP    26
#define S_TCB_ISS_ULP    11
#define M_TCB_ISS_ULP    0x1ffULL
#define V_TCB_ISS_ULP(x) ((x) << S_TCB_ISS_ULP)

#define W_TCB_TX_PDU_LEN    26
#define S_TCB_TX_PDU_LEN    20
#define M_TCB_TX_PDU_LEN    0x3fffULL
#define V_TCB_TX_PDU_LEN(x) ((x) << S_TCB_TX_PDU_LEN)

#define W_TCB_TX_PDU_OUT    27
#define S_TCB_TX_PDU_OUT    2
#define M_TCB_TX_PDU_OUT    0x1ULL
#define V_TCB_TX_PDU_OUT(x) ((x) << S_TCB_TX_PDU_OUT)

#define W_TCB_CQ_IDX_SQ    27
#define S_TCB_CQ_IDX_SQ    3
#define M_TCB_CQ_IDX_SQ    0xffffULL
#define V_TCB_CQ_IDX_SQ(x) ((x) << S_TCB_CQ_IDX_SQ)

#define W_TCB_CQ_IDX_RQ    27
#define S_TCB_CQ_IDX_RQ    19
#define M_TCB_CQ_IDX_RQ    0xffffULL
#define V_TCB_CQ_IDX_RQ(x) ((x) << S_TCB_CQ_IDX_RQ)

#define W_TCB_QP_ID    28
#define S_TCB_QP_ID    3
#define M_TCB_QP_ID    0xffffULL
#define V_TCB_QP_ID(x) ((x) << S_TCB_QP_ID)

#define W_TCB_PD_ID    28
#define S_TCB_PD_ID    19
#define M_TCB_PD_ID    0xffffULL
#define V_TCB_PD_ID(x) ((x) << S_TCB_PD_ID)

#define W_TCB_STAG    29
#define S_TCB_STAG    3
#define M_TCB_STAG    0xffffffffULL
#define V_TCB_STAG(x) ((x) << S_TCB_STAG)

#define W_TCB_RQ_START    30
#define S_TCB_RQ_START    3
#define M_TCB_RQ_START    0x3ffffffULL
#define V_TCB_RQ_START(x) ((x) << S_TCB_RQ_START)

#define W_TCB_RQ_MSN    30
#define S_TCB_RQ_MSN    29
#define M_TCB_RQ_MSN    0x3ffULL
#define V_TCB_RQ_MSN(x) ((x) << S_TCB_RQ_MSN)

#define W_TCB_RQ_MAX_OFFSET    31
#define S_TCB_RQ_MAX_OFFSET    7
#define M_TCB_RQ_MAX_OFFSET    0xfULL
#define V_TCB_RQ_MAX_OFFSET(x) ((x) << S_TCB_RQ_MAX_OFFSET)

#define W_TCB_RQ_WRITE_PTR    31
#define S_TCB_RQ_WRITE_PTR    11
#define M_TCB_RQ_WRITE_PTR    0x3ffULL
#define V_TCB_RQ_WRITE_PTR(x) ((x) << S_TCB_RQ_WRITE_PTR)

#define W_TCB_INB_WRITE_PERM    31
#define S_TCB_INB_WRITE_PERM    21
#define M_TCB_INB_WRITE_PERM    0x1ULL
#define V_TCB_INB_WRITE_PERM(x) ((x) << S_TCB_INB_WRITE_PERM)

#define W_TCB_INB_READ_PERM    31
#define S_TCB_INB_READ_PERM    22
#define M_TCB_INB_READ_PERM    0x1ULL
#define V_TCB_INB_READ_PERM(x) ((x) << S_TCB_INB_READ_PERM)

#define W_TCB_ORD_L_BIT_VLD    31
#define S_TCB_ORD_L_BIT_VLD    23
#define M_TCB_ORD_L_BIT_VLD    0x1ULL
#define V_TCB_ORD_L_BIT_VLD(x) ((x) << S_TCB_ORD_L_BIT_VLD)

#define W_TCB_RDMAP_OPCODE    31
#define S_TCB_RDMAP_OPCODE    24
#define M_TCB_RDMAP_OPCODE    0xfULL
#define V_TCB_RDMAP_OPCODE(x) ((x) << S_TCB_RDMAP_OPCODE)

#define W_TCB_TX_FLUSH    31
#define S_TCB_TX_FLUSH    28
#define M_TCB_TX_FLUSH    0x1ULL
#define V_TCB_TX_FLUSH(x) ((x) << S_TCB_TX_FLUSH)

#define W_TCB_TX_OOS_RXMT    31
#define S_TCB_TX_OOS_RXMT    29
#define M_TCB_TX_OOS_RXMT    0x1ULL
#define V_TCB_TX_OOS_RXMT(x) ((x) << S_TCB_TX_OOS_RXMT)

#define W_TCB_TX_OOS_TXMT    31
#define S_TCB_TX_OOS_TXMT    30
#define M_TCB_TX_OOS_TXMT    0x1ULL
#define V_TCB_TX_OOS_TXMT(x) ((x) << S_TCB_TX_OOS_TXMT)

#define W_TCB_SLUSH_AUX2    31
#define S_TCB_SLUSH_AUX2    31
#define M_TCB_SLUSH_AUX2    0x1ULL
#define V_TCB_SLUSH_AUX2(x) ((x) << S_TCB_SLUSH_AUX2)

#define W_TCB_RX_FRAG1_PTR_RAW2    25
#define S_TCB_RX_FRAG1_PTR_RAW2    30
#define M_TCB_RX_FRAG1_PTR_RAW2    0x1ffffULL
#define V_TCB_RX_FRAG1_PTR_RAW2(x) ((x) << S_TCB_RX_FRAG1_PTR_RAW2)

#define W_TCB_RX_DDP_FLAGS    26
#define S_TCB_RX_DDP_FLAGS    15
#define M_TCB_RX_DDP_FLAGS    0xffffULL
#define V_TCB_RX_DDP_FLAGS(x) ((x) << S_TCB_RX_DDP_FLAGS)

#define W_TCB_SLUSH_AUX3    26
#define S_TCB_SLUSH_AUX3    31
#define M_TCB_SLUSH_AUX3    0x1ffULL
#define V_TCB_SLUSH_AUX3(x) ((x) << S_TCB_SLUSH_AUX3)

#define W_TCB_RX_DDP_BUF0_OFFSET    27
#define S_TCB_RX_DDP_BUF0_OFFSET    8
#define M_TCB_RX_DDP_BUF0_OFFSET    0x3fffffULL
#define V_TCB_RX_DDP_BUF0_OFFSET(x) ((x) << S_TCB_RX_DDP_BUF0_OFFSET)

#define W_TCB_RX_DDP_BUF0_LEN    27
#define S_TCB_RX_DDP_BUF0_LEN    30
#define M_TCB_RX_DDP_BUF0_LEN    0x3fffffULL
#define V_TCB_RX_DDP_BUF0_LEN(x) ((x) << S_TCB_RX_DDP_BUF0_LEN)

#define W_TCB_RX_DDP_BUF1_OFFSET    28
#define S_TCB_RX_DDP_BUF1_OFFSET    20
#define M_TCB_RX_DDP_BUF1_OFFSET    0x3fffffULL
#define V_TCB_RX_DDP_BUF1_OFFSET(x) ((x) << S_TCB_RX_DDP_BUF1_OFFSET)

#define W_TCB_RX_DDP_BUF1_LEN    29
#define S_TCB_RX_DDP_BUF1_LEN    10
#define M_TCB_RX_DDP_BUF1_LEN    0x3fffffULL
#define V_TCB_RX_DDP_BUF1_LEN(x) ((x) << S_TCB_RX_DDP_BUF1_LEN)

#define W_TCB_RX_DDP_BUF0_TAG    30
#define S_TCB_RX_DDP_BUF0_TAG    0
#define M_TCB_RX_DDP_BUF0_TAG    0xffffffffULL
#define V_TCB_RX_DDP_BUF0_TAG(x) ((x) << S_TCB_RX_DDP_BUF0_TAG)

#define W_TCB_RX_DDP_BUF1_TAG    31
#define S_TCB_RX_DDP_BUF1_TAG    0
#define M_TCB_RX_DDP_BUF1_TAG    0xffffffffULL
#define V_TCB_RX_DDP_BUF1_TAG(x) ((x) << S_TCB_RX_DDP_BUF1_TAG)

#define S_TF_DACK    10
#define V_TF_DACK(x) ((x) << S_TF_DACK)

#define S_TF_NAGLE    11
#define V_TF_NAGLE(x) ((x) << S_TF_NAGLE)

#define S_TF_RECV_SCALE    12
#define V_TF_RECV_SCALE(x) ((x) << S_TF_RECV_SCALE)

#define S_TF_RECV_TSTMP    13
#define V_TF_RECV_TSTMP(x) ((x) << S_TF_RECV_TSTMP)

#define S_TF_RECV_SACK    14
#define V_TF_RECV_SACK(x) ((x) << S_TF_RECV_SACK)

#define S_TF_TURBO    15
#define V_TF_TURBO(x) ((x) << S_TF_TURBO)

#define S_TF_KEEPALIVE    16
#define V_TF_KEEPALIVE(x) ((x) << S_TF_KEEPALIVE)

#define S_TF_TCAM_BYPASS    17
#define V_TF_TCAM_BYPASS(x) ((x) << S_TF_TCAM_BYPASS)

#define S_TF_CORE_FIN    18
#define V_TF_CORE_FIN(x) ((x) << S_TF_CORE_FIN)

#define S_TF_CORE_MORE    19
#define V_TF_CORE_MORE(x) ((x) << S_TF_CORE_MORE)

#define S_TF_MIGRATING    20
#define V_TF_MIGRATING(x) ((x) << S_TF_MIGRATING)

#define S_TF_ACTIVE_OPEN    21
#define V_TF_ACTIVE_OPEN(x) ((x) << S_TF_ACTIVE_OPEN)

#define S_TF_ASK_MODE    22
#define V_TF_ASK_MODE(x) ((x) << S_TF_ASK_MODE)

#define S_TF_NON_OFFLOAD    23
#define V_TF_NON_OFFLOAD(x) ((x) << S_TF_NON_OFFLOAD)

#define S_TF_MOD_SCHD    24
#define V_TF_MOD_SCHD(x) ((x) << S_TF_MOD_SCHD)

#define S_TF_MOD_SCHD_REASON0    25
#define V_TF_MOD_SCHD_REASON0(x) ((x) << S_TF_MOD_SCHD_REASON0)

#define S_TF_MOD_SCHD_REASON1    26
#define V_TF_MOD_SCHD_REASON1(x) ((x) << S_TF_MOD_SCHD_REASON1)

#define S_TF_MOD_SCHD_RX    27
#define V_TF_MOD_SCHD_RX(x) ((x) << S_TF_MOD_SCHD_RX)

#define S_TF_CORE_PUSH    28
#define V_TF_CORE_PUSH(x) ((x) << S_TF_CORE_PUSH)

#define S_TF_RCV_COALESCE_ENABLE    29
#define V_TF_RCV_COALESCE_ENABLE(x) ((x) << S_TF_RCV_COALESCE_ENABLE)

#define S_TF_RCV_COALESCE_PUSH    30
#define V_TF_RCV_COALESCE_PUSH(x) ((x) << S_TF_RCV_COALESCE_PUSH)

#define S_TF_RCV_COALESCE_LAST_PSH    31
#define V_TF_RCV_COALESCE_LAST_PSH(x) ((x) << S_TF_RCV_COALESCE_LAST_PSH)

#define S_TF_RCV_COALESCE_HEARTBEAT    32
#define V_TF_RCV_COALESCE_HEARTBEAT(x) ((x) << S_TF_RCV_COALESCE_HEARTBEAT)

#define S_TF_LOCK_TID    33
#define V_TF_LOCK_TID(x) ((x) << S_TF_LOCK_TID)

#define S_TF_DACK_MSS    34
#define V_TF_DACK_MSS(x) ((x) << S_TF_DACK_MSS)

#define S_TF_CCTRL_SEL0    35
#define V_TF_CCTRL_SEL0(x) ((x) << S_TF_CCTRL_SEL0)

#define S_TF_CCTRL_SEL1    36
#define V_TF_CCTRL_SEL1(x) ((x) << S_TF_CCTRL_SEL1)

#define S_TF_TCP_NEWRENO_FAST_RECOVERY    37
#define V_TF_TCP_NEWRENO_FAST_RECOVERY(x) ((x) << S_TF_TCP_NEWRENO_FAST_RECOVERY)

#define S_TF_TX_PACE_AUTO    38
#define V_TF_TX_PACE_AUTO(x) ((x) << S_TF_TX_PACE_AUTO)

#define S_TF_PEER_FIN_HELD    39
#define V_TF_PEER_FIN_HELD(x) ((x) << S_TF_PEER_FIN_HELD)

#define S_TF_CORE_URG    40
#define V_TF_CORE_URG(x) ((x) << S_TF_CORE_URG)

#define S_TF_RDMA_ERROR    41
#define V_TF_RDMA_ERROR(x) ((x) << S_TF_RDMA_ERROR)

#define S_TF_SSWS_DISABLED    42
#define V_TF_SSWS_DISABLED(x) ((x) << S_TF_SSWS_DISABLED)

#define S_TF_DUPACK_COUNT_ODD    43
#define V_TF_DUPACK_COUNT_ODD(x) ((x) << S_TF_DUPACK_COUNT_ODD)

#define S_TF_TX_CHANNEL    44
#define V_TF_TX_CHANNEL(x) ((x) << S_TF_TX_CHANNEL)

#define S_TF_RX_CHANNEL    45
#define V_TF_RX_CHANNEL(x) ((x) << S_TF_RX_CHANNEL)

#define S_TF_TX_PACE_FIXED    46
#define V_TF_TX_PACE_FIXED(x) ((x) << S_TF_TX_PACE_FIXED)

#define S_TF_RDMA_FLM_ERROR    47
#define V_TF_RDMA_FLM_ERROR(x) ((x) << S_TF_RDMA_FLM_ERROR)

#define S_TF_RX_FLOW_CONTROL_DISABLE    48
#define V_TF_RX_FLOW_CONTROL_DISABLE(x) ((x) << S_TF_RX_FLOW_CONTROL_DISABLE)

#define S_TF_DDP_INDICATE_OUT    15
#define V_TF_DDP_INDICATE_OUT(x) ((x) << S_TF_DDP_INDICATE_OUT)

#define S_TF_DDP_ACTIVE_BUF    16
#define V_TF_DDP_ACTIVE_BUF(x) ((x) << S_TF_DDP_ACTIVE_BUF)

#define S_TF_DDP_BUF0_VALID    17
#define V_TF_DDP_BUF0_VALID(x) ((x) << S_TF_DDP_BUF0_VALID)

#define S_TF_DDP_BUF1_VALID    18
#define V_TF_DDP_BUF1_VALID(x) ((x) << S_TF_DDP_BUF1_VALID)

#define S_TF_DDP_BUF0_INDICATE    19
#define V_TF_DDP_BUF0_INDICATE(x) ((x) << S_TF_DDP_BUF0_INDICATE)

#define S_TF_DDP_BUF1_INDICATE    20
#define V_TF_DDP_BUF1_INDICATE(x) ((x) << S_TF_DDP_BUF1_INDICATE)

#define S_TF_DDP_PUSH_DISABLE_0    21
#define V_TF_DDP_PUSH_DISABLE_0(x) ((x) << S_TF_DDP_PUSH_DISABLE_0)

#define S_TF_DDP_PUSH_DISABLE_1    22
#define V_TF_DDP_PUSH_DISABLE_1(x) ((x) << S_TF_DDP_PUSH_DISABLE_1)

#define S_TF_DDP_OFF    23
#define V_TF_DDP_OFF(x) ((x) << S_TF_DDP_OFF)

#define S_TF_DDP_WAIT_FRAG    24
#define V_TF_DDP_WAIT_FRAG(x) ((x) << S_TF_DDP_WAIT_FRAG)

#define S_TF_DDP_BUF_INF    25
#define V_TF_DDP_BUF_INF(x) ((x) << S_TF_DDP_BUF_INF)

#define S_TF_DDP_RX2TX    26
#define V_TF_DDP_RX2TX(x) ((x) << S_TF_DDP_RX2TX)

#define S_TF_DDP_BUF0_FLUSH    27
#define V_TF_DDP_BUF0_FLUSH(x) ((x) << S_TF_DDP_BUF0_FLUSH)

#define S_TF_DDP_BUF1_FLUSH    28
#define V_TF_DDP_BUF1_FLUSH(x) ((x) << S_TF_DDP_BUF1_FLUSH)

#define S_TF_DDP_PSH_NO_INVALIDATE    29
#define V_TF_DDP_PSH_NO_INVALIDATE(x) ((x) << S_TF_DDP_PSH_NO_INVALIDATE)

#endif /* _TCB_DEFS_H */
