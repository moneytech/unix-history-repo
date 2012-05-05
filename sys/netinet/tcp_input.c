/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2007-2008,2010
 *	Swinburne University of Technology, Melbourne, Australia.
 * Copyright (c) 2009-2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Lawrence Stewart,
 * James Healy and David Hayes, made possible in part by a grant from the Cisco
 * University Research Program Fund at Community Foundation Silicon Valley.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by David Hayes under sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to Juniper Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tcp_input.c	8.12 (Berkeley) 5/24/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ipfw.h"		/* for ipfw_fwd	*/
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/hhook.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>		/* for proc0 declaration */
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <machine/cpu.h>	/* before tcp_seq.h, for tcp_random18() */

#include <vm/uma.h>

#include <net/if.h>
#include <net/route.h>
#include <net/vnet.h>

#define TCPSTATES		/* for logging */

#include <netinet/cc.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>	/* required for icmp_var.h */
#include <netinet/icmp_var.h>	/* for ICMP_BANDLIM */
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet6/tcp6_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_syncache.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif /* TCPDEBUG */

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#endif /*IPSEC*/

#include <machine/in_cksum.h>

#include <security/mac/mac_framework.h>

const int tcprexmtthresh = 3;

VNET_DEFINE(struct tcpstat, tcpstat);
SYSCTL_VNET_STRUCT(_net_inet_tcp, TCPCTL_STATS, stats, CTLFLAG_RW,
    &VNET_NAME(tcpstat), tcpstat,
    "TCP statistics (struct tcpstat, netinet/tcp_var.h)");

int tcp_log_in_vain = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, log_in_vain, CTLFLAG_RW,
    &tcp_log_in_vain, 0,
    "Log all incoming TCP segments to closed ports");

VNET_DEFINE(int, blackhole) = 0;
#define	V_blackhole		VNET(blackhole)
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, blackhole, CTLFLAG_RW,
    &VNET_NAME(blackhole), 0,
    "Do not send RST on segments to closed ports");

VNET_DEFINE(int, tcp_delack_enabled) = 1;
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, delayed_ack, CTLFLAG_RW,
    &VNET_NAME(tcp_delack_enabled), 0,
    "Delay ACK to try and piggyback it onto a data packet");

VNET_DEFINE(int, drop_synfin) = 0;
#define	V_drop_synfin		VNET(drop_synfin)
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, drop_synfin, CTLFLAG_RW,
    &VNET_NAME(drop_synfin), 0,
    "Drop TCP packets with SYN+FIN set");

VNET_DEFINE(int, tcp_do_rfc3042) = 1;
#define	V_tcp_do_rfc3042	VNET(tcp_do_rfc3042)
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, rfc3042, CTLFLAG_RW,
    &VNET_NAME(tcp_do_rfc3042), 0,
    "Enable RFC 3042 (Limited Transmit)");

VNET_DEFINE(int, tcp_do_rfc3390) = 1;
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, rfc3390, CTLFLAG_RW,
    &VNET_NAME(tcp_do_rfc3390), 0,
    "Enable RFC 3390 (Increasing TCP's Initial Congestion Window)");

VNET_DEFINE(int, tcp_do_rfc3465) = 1;
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, rfc3465, CTLFLAG_RW,
    &VNET_NAME(tcp_do_rfc3465), 0,
    "Enable RFC 3465 (Appropriate Byte Counting)");

VNET_DEFINE(int, tcp_abc_l_var) = 2;
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, abc_l_var, CTLFLAG_RW,
    &VNET_NAME(tcp_abc_l_var), 2,
    "Cap the max cwnd increment during slow-start to this number of segments");

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, ecn, CTLFLAG_RW, 0, "TCP ECN");

VNET_DEFINE(int, tcp_do_ecn) = 0;
SYSCTL_VNET_INT(_net_inet_tcp_ecn, OID_AUTO, enable, CTLFLAG_RW,
    &VNET_NAME(tcp_do_ecn), 0,
    "TCP ECN support");

VNET_DEFINE(int, tcp_ecn_maxretries) = 1;
SYSCTL_VNET_INT(_net_inet_tcp_ecn, OID_AUTO, maxretries, CTLFLAG_RW,
    &VNET_NAME(tcp_ecn_maxretries), 0,
    "Max retries before giving up on ECN");

VNET_DEFINE(int, tcp_insecure_rst) = 0;
#define	V_tcp_insecure_rst	VNET(tcp_insecure_rst)
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, insecure_rst, CTLFLAG_RW,
    &VNET_NAME(tcp_insecure_rst), 0,
    "Follow the old (insecure) criteria for accepting RST packets");

VNET_DEFINE(int, tcp_do_autorcvbuf) = 1;
#define	V_tcp_do_autorcvbuf	VNET(tcp_do_autorcvbuf)
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, recvbuf_auto, CTLFLAG_RW,
    &VNET_NAME(tcp_do_autorcvbuf), 0,
    "Enable automatic receive buffer sizing");

VNET_DEFINE(int, tcp_autorcvbuf_inc) = 16*1024;
#define	V_tcp_autorcvbuf_inc	VNET(tcp_autorcvbuf_inc)
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, recvbuf_inc, CTLFLAG_RW,
    &VNET_NAME(tcp_autorcvbuf_inc), 0,
    "Incrementor step size of automatic receive buffer");

VNET_DEFINE(int, tcp_autorcvbuf_max) = 2*1024*1024;
#define	V_tcp_autorcvbuf_max	VNET(tcp_autorcvbuf_max)
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, recvbuf_max, CTLFLAG_RW,
    &VNET_NAME(tcp_autorcvbuf_max), 0,
    "Max size of automatic receive buffer");

VNET_DEFINE(struct inpcbhead, tcb);
#define	tcb6	tcb  /* for KAME src sync over BSD*'s */
VNET_DEFINE(struct inpcbinfo, tcbinfo);

static void	 tcp_dooptions(struct tcpopt *, u_char *, int, int);
static void	 tcp_do_segment(struct mbuf *, struct tcphdr *,
		     struct socket *, struct tcpcb *, int, int, uint8_t,
		     int);
static void	 tcp_dropwithreset(struct mbuf *, struct tcphdr *,
		     struct tcpcb *, int, int);
static void	 tcp_pulloutofband(struct socket *,
		     struct tcphdr *, struct mbuf *, int);
static void	 tcp_xmit_timer(struct tcpcb *, int);
static void	 tcp_newreno_partial_ack(struct tcpcb *, struct tcphdr *);
static void inline 	tcp_fields_to_host(struct tcphdr *);
#ifdef TCP_SIGNATURE
static void inline 	tcp_fields_to_net(struct tcphdr *);
static int inline	tcp_signature_verify_input(struct mbuf *, int, int,
			    int, struct tcpopt *, struct tcphdr *, u_int);
#endif
static void inline	cc_ack_received(struct tcpcb *tp, struct tcphdr *th,
			    uint16_t type);
static void inline	cc_conn_init(struct tcpcb *tp);
static void inline	cc_post_recovery(struct tcpcb *tp, struct tcphdr *th);
static void inline	hhook_run_tcp_est_in(struct tcpcb *tp,
			    struct tcphdr *th, struct tcpopt *to);

/*
 * Kernel module interface for updating tcpstat.  The argument is an index
 * into tcpstat treated as an array of u_long.  While this encodes the
 * general layout of tcpstat into the caller, it doesn't encode its location,
 * so that future changes to add, for example, per-CPU stats support won't
 * cause binary compatibility problems for kernel modules.
 */
void
kmod_tcpstat_inc(int statnum)
{

	(*((u_long *)&V_tcpstat + statnum))++;
}

/*
 * Wrapper for the TCP established input helper hook.
 */
static void inline
hhook_run_tcp_est_in(struct tcpcb *tp, struct tcphdr *th, struct tcpopt *to)
{
	struct tcp_hhook_data hhook_data;

	if (V_tcp_hhh[HHOOK_TCP_EST_IN]->hhh_nhooks > 0) {
		hhook_data.tp = tp;
		hhook_data.th = th;
		hhook_data.to = to;

		hhook_run_hooks(V_tcp_hhh[HHOOK_TCP_EST_IN], &hhook_data,
		    tp->osd);
	}
}

/*
 * CC wrapper hook functions
 */
static void inline
cc_ack_received(struct tcpcb *tp, struct tcphdr *th, uint16_t type)
{
	INP_WLOCK_ASSERT(tp->t_inpcb);

	tp->ccv->bytes_this_ack = BYTES_THIS_ACK(tp, th);
	if (tp->snd_cwnd == min(tp->snd_cwnd, tp->snd_wnd))
		tp->ccv->flags |= CCF_CWND_LIMITED;
	else
		tp->ccv->flags &= ~CCF_CWND_LIMITED;

	if (type == CC_ACK) {
		if (tp->snd_cwnd > tp->snd_ssthresh) {
			tp->t_bytes_acked += min(tp->ccv->bytes_this_ack,
			     V_tcp_abc_l_var * tp->t_maxseg);
			if (tp->t_bytes_acked >= tp->snd_cwnd) {
				tp->t_bytes_acked -= tp->snd_cwnd;
				tp->ccv->flags |= CCF_ABC_SENTAWND;
			}
		} else {
				tp->ccv->flags &= ~CCF_ABC_SENTAWND;
				tp->t_bytes_acked = 0;
		}
	}

	if (CC_ALGO(tp)->ack_received != NULL) {
		/* XXXLAS: Find a way to live without this */
		tp->ccv->curack = th->th_ack;
		CC_ALGO(tp)->ack_received(tp->ccv, type);
	}
}

static void inline
cc_conn_init(struct tcpcb *tp)
{
	struct hc_metrics_lite metrics;
	struct inpcb *inp = tp->t_inpcb;
	int rtt;
#ifdef INET6
	int isipv6 = ((inp->inp_vflag & INP_IPV6) != 0) ? 1 : 0;
#endif

	INP_WLOCK_ASSERT(tp->t_inpcb);

	tcp_hc_get(&inp->inp_inc, &metrics);

	if (tp->t_srtt == 0 && (rtt = metrics.rmx_rtt)) {
		tp->t_srtt = rtt;
		tp->t_rttbest = tp->t_srtt + TCP_RTT_SCALE;
		TCPSTAT_INC(tcps_usedrtt);
		if (metrics.rmx_rttvar) {
			tp->t_rttvar = metrics.rmx_rttvar;
			TCPSTAT_INC(tcps_usedrttvar);
		} else {
			/* default variation is +- 1 rtt */
			tp->t_rttvar =
			    tp->t_srtt * TCP_RTTVAR_SCALE / TCP_RTT_SCALE;
		}
		TCPT_RANGESET(tp->t_rxtcur,
		    ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1,
		    tp->t_rttmin, TCPTV_REXMTMAX);
	}
	if (metrics.rmx_ssthresh) {
		/*
		 * There's some sort of gateway or interface
		 * buffer limit on the path.  Use this to set
		 * the slow start threshhold, but set the
		 * threshold to no less than 2*mss.
		 */
		tp->snd_ssthresh = max(2 * tp->t_maxseg, metrics.rmx_ssthresh);
		TCPSTAT_INC(tcps_usedssthresh);
	}

	/*
	 * Set the slow-start flight size depending on whether this
	 * is a local network or not.
	 *
	 * Extend this so we cache the cwnd too and retrieve it here.
	 * Make cwnd even bigger than RFC3390 suggests but only if we
	 * have previous experience with the remote host. Be careful
	 * not make cwnd bigger than remote receive window or our own
	 * send socket buffer. Maybe put some additional upper bound
	 * on the retrieved cwnd. Should do incremental updates to
	 * hostcache when cwnd collapses so next connection doesn't
	 * overloads the path again.
	 *
	 * XXXAO: Initializing the CWND from the hostcache is broken
	 * and in its current form not RFC conformant.  It is disabled
	 * until fixed or removed entirely.
	 *
	 * RFC3390 says only do this if SYN or SYN/ACK didn't got lost.
	 * We currently check only in syncache_socket for that.
	 */
/* #define TCP_METRICS_CWND */
#ifdef TCP_METRICS_CWND
	if (metrics.rmx_cwnd)
		tp->snd_cwnd = max(tp->t_maxseg, min(metrics.rmx_cwnd / 2,
		    min(tp->snd_wnd, so->so_snd.sb_hiwat)));
	else
#endif
	if (V_tcp_do_rfc3390)
		tp->snd_cwnd = min(4 * tp->t_maxseg,
		    max(2 * tp->t_maxseg, 4380));
#ifdef INET6
	else if (isipv6 && in6_localaddr(&inp->in6p_faddr))
		tp->snd_cwnd = tp->t_maxseg * V_ss_fltsz_local;
#endif
#if defined(INET) && defined(INET6)
	else if (!isipv6 && in_localaddr(inp->inp_faddr))
		tp->snd_cwnd = tp->t_maxseg * V_ss_fltsz_local;
#endif
#ifdef INET
	else if (in_localaddr(inp->inp_faddr))
		tp->snd_cwnd = tp->t_maxseg * V_ss_fltsz_local;
#endif
	else
		tp->snd_cwnd = tp->t_maxseg * V_ss_fltsz;

	if (CC_ALGO(tp)->conn_init != NULL)
		CC_ALGO(tp)->conn_init(tp->ccv);
}

void inline
cc_cong_signal(struct tcpcb *tp, struct tcphdr *th, uint32_t type)
{
	INP_WLOCK_ASSERT(tp->t_inpcb);

	switch(type) {
	case CC_NDUPACK:
		if (!IN_FASTRECOVERY(tp->t_flags)) {
			tp->snd_recover = tp->snd_max;
			if (tp->t_flags & TF_ECN_PERMIT)
				tp->t_flags |= TF_ECN_SND_CWR;
		}
		break;
	case CC_ECN:
		if (!IN_CONGRECOVERY(tp->t_flags)) {
			TCPSTAT_INC(tcps_ecn_rcwnd);
			tp->snd_recover = tp->snd_max;
			if (tp->t_flags & TF_ECN_PERMIT)
				tp->t_flags |= TF_ECN_SND_CWR;
		}
		break;
	case CC_RTO:
		tp->t_dupacks = 0;
		tp->t_bytes_acked = 0;
		EXIT_RECOVERY(tp->t_flags);
		tp->snd_ssthresh = max(2, min(tp->snd_wnd, tp->snd_cwnd) / 2 /
		    tp->t_maxseg) * tp->t_maxseg;
		tp->snd_cwnd = tp->t_maxseg;
		break;
	case CC_RTO_ERR:
		TCPSTAT_INC(tcps_sndrexmitbad);
		/* RTO was unnecessary, so reset everything. */
		tp->snd_cwnd = tp->snd_cwnd_prev;
		tp->snd_ssthresh = tp->snd_ssthresh_prev;
		tp->snd_recover = tp->snd_recover_prev;
		if (tp->t_flags & TF_WASFRECOVERY)
			ENTER_FASTRECOVERY(tp->t_flags);
		if (tp->t_flags & TF_WASCRECOVERY)
			ENTER_CONGRECOVERY(tp->t_flags);
		tp->snd_nxt = tp->snd_max;
		tp->t_flags &= ~TF_PREVVALID;
		tp->t_badrxtwin = 0;
		break;
	}

	if (CC_ALGO(tp)->cong_signal != NULL) {
		if (th != NULL)
			tp->ccv->curack = th->th_ack;
		CC_ALGO(tp)->cong_signal(tp->ccv, type);
	}
}

static void inline
cc_post_recovery(struct tcpcb *tp, struct tcphdr *th)
{
	INP_WLOCK_ASSERT(tp->t_inpcb);

	/* XXXLAS: KASSERT that we're in recovery? */

	if (CC_ALGO(tp)->post_recovery != NULL) {
		tp->ccv->curack = th->th_ack;
		CC_ALGO(tp)->post_recovery(tp->ccv);
	}
	/* XXXLAS: EXIT_RECOVERY ? */
	tp->t_bytes_acked = 0;
}

static inline void
tcp_fields_to_host(struct tcphdr *th)
{

	th->th_seq = ntohl(th->th_seq);
	th->th_ack = ntohl(th->th_ack);
	th->th_win = ntohs(th->th_win);
	th->th_urp = ntohs(th->th_urp);
}

#ifdef TCP_SIGNATURE
static inline void
tcp_fields_to_net(struct tcphdr *th)
{

	th->th_seq = htonl(th->th_seq);
	th->th_ack = htonl(th->th_ack);
	th->th_win = htons(th->th_win);
	th->th_urp = htons(th->th_urp);
}

static inline int
tcp_signature_verify_input(struct mbuf *m, int off0, int tlen, int optlen,
    struct tcpopt *to, struct tcphdr *th, u_int tcpbflag)
{
	int ret;

	tcp_fields_to_net(th);
	ret = tcp_signature_verify(m, off0, tlen, optlen, to, th, tcpbflag);
	tcp_fields_to_host(th);
	return (ret);
}
#endif

/* Neighbor Discovery, Neighbor Unreachability Detection Upper layer hint. */
#ifdef INET6
#define ND6_HINT(tp) \
do { \
	if ((tp) && (tp)->t_inpcb && \
	    ((tp)->t_inpcb->inp_vflag & INP_IPV6) != 0) \
		nd6_nud_hint(NULL, NULL, 0); \
} while (0)
#else
#define ND6_HINT(tp)
#endif

/*
 * Indicate whether this ack should be delayed.  We can delay the ack if
 *	- there is no delayed ack timer in progress and
 *	- our last ack wasn't a 0-sized window.  We never want to delay
 *	  the ack that opens up a 0-sized window and
 *		- delayed acks are enabled or
 *		- this is a half-synchronized T/TCP connection.
 */
#define DELAY_ACK(tp)							\
	((!tcp_timer_active(tp, TT_DELACK) &&				\
	    (tp->t_flags & TF_RXWIN0SENT) == 0) &&			\
	    (V_tcp_delack_enabled || (tp->t_flags & TF_NEEDSYN)))

/*
 * TCP input handling is split into multiple parts:
 *   tcp6_input is a thin wrapper around tcp_input for the extended
 *	ip6_protox[] call format in ip6_input
 *   tcp_input handles primary segment validation, inpcb lookup and
 *	SYN processing on listen sockets
 *   tcp_do_segment processes the ACK and text of the segment for
 *	establishing, established and closing connections
 */
#ifdef INET6
int
tcp6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct in6_ifaddr *ia6;

	IP6_EXTHDR_CHECK(m, *offp, sizeof(struct tcphdr), IPPROTO_DONE);

	/*
	 * draft-itojun-ipv6-tcp-to-anycast
	 * better place to put this in?
	 */
	ia6 = ip6_getdstifaddr(m);
	if (ia6 && (ia6->ia6_flags & IN6_IFF_ANYCAST)) {
		struct ip6_hdr *ip6;

		ifa_free(&ia6->ia_ifa);
		ip6 = mtod(m, struct ip6_hdr *);
		icmp6_error(m, ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_ADDR,
			    (caddr_t)&ip6->ip6_dst - (caddr_t)ip6);
		return IPPROTO_DONE;
	}

	tcp_input(m, *offp);
	return IPPROTO_DONE;
}
#endif /* INET6 */

void
tcp_input(struct mbuf *m, int off0)
{
	struct tcphdr *th = NULL;
	struct ip *ip = NULL;
#ifdef INET
	struct ipovly *ipov;
#endif
	struct inpcb *inp = NULL;
	struct tcpcb *tp = NULL;
	struct socket *so = NULL;
	u_char *optp = NULL;
	int optlen = 0;
#ifdef INET
	int len;
#endif
	int tlen = 0, off;
	int drop_hdrlen;
	int thflags;
	int rstreason = 0;	/* For badport_bandlim accounting purposes */
#ifdef TCP_SIGNATURE
	uint8_t sig_checked = 0;
#endif
	uint8_t iptos = 0;
#ifdef IPFIREWALL_FORWARD
	struct m_tag *fwd_tag;
#endif
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
	int isipv6;
#else
	const void *ip6 = NULL;
#endif /* INET6 */
	struct tcpopt to;		/* options in this segment */
	char *s = NULL;			/* address and port logging */
	int ti_locked;
#define	TI_UNLOCKED	1
#define	TI_WLOCKED	2

#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[IP6_HDR_LEN];
	struct tcphdr tcp_savetcp;
	short ostate = 0;
#endif

#ifdef INET6
	isipv6 = (mtod(m, struct ip *)->ip_v == 6) ? 1 : 0;
#endif

	to.to_flags = 0;
	TCPSTAT_INC(tcps_rcvtotal);

#ifdef INET6
	if (isipv6) {
		/* IP6_EXTHDR_CHECK() is already done at tcp6_input(). */
		ip6 = mtod(m, struct ip6_hdr *);
		tlen = sizeof(*ip6) + ntohs(ip6->ip6_plen) - off0;
		if (in6_cksum(m, IPPROTO_TCP, off0, tlen)) {
			TCPSTAT_INC(tcps_rcvbadsum);
			goto drop;
		}
		th = (struct tcphdr *)((caddr_t)ip6 + off0);

		/*
		 * Be proactive about unspecified IPv6 address in source.
		 * As we use all-zero to indicate unbounded/unconnected pcb,
		 * unspecified IPv6 address can be used to confuse us.
		 *
		 * Note that packets with unspecified IPv6 destination is
		 * already dropped in ip6_input.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
			/* XXX stat */
			goto drop;
		}
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		/*
		 * Get IP and TCP header together in first mbuf.
		 * Note: IP leaves IP header in first mbuf.
		 */
		if (off0 > sizeof (struct ip)) {
			ip_stripoptions(m, (struct mbuf *)0);
			off0 = sizeof(struct ip);
		}
		if (m->m_len < sizeof (struct tcpiphdr)) {
			if ((m = m_pullup(m, sizeof (struct tcpiphdr)))
			    == NULL) {
				TCPSTAT_INC(tcps_rcvshort);
				return;
			}
		}
		ip = mtod(m, struct ip *);
		ipov = (struct ipovly *)ip;
		th = (struct tcphdr *)((caddr_t)ip + off0);
		tlen = ip->ip_len;

		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
				th->th_sum = m->m_pkthdr.csum_data;
			else
				th->th_sum = in_pseudo(ip->ip_src.s_addr,
						ip->ip_dst.s_addr,
						htonl(m->m_pkthdr.csum_data +
							ip->ip_len +
							IPPROTO_TCP));
			th->th_sum ^= 0xffff;
#ifdef TCPDEBUG
			ipov->ih_len = (u_short)tlen;
			ipov->ih_len = htons(ipov->ih_len);
#endif
		} else {
			/*
			 * Checksum extended TCP header and data.
			 */
			len = sizeof (struct ip) + tlen;
			bzero(ipov->ih_x1, sizeof(ipov->ih_x1));
			ipov->ih_len = (u_short)tlen;
			ipov->ih_len = htons(ipov->ih_len);
			th->th_sum = in_cksum(m, len);
		}
		if (th->th_sum) {
			TCPSTAT_INC(tcps_rcvbadsum);
			goto drop;
		}
		/* Re-initialization for later version check */
		ip->ip_v = IPVERSION;
	}
#endif /* INET */

#ifdef INET6
	if (isipv6)
		iptos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
		iptos = ip->ip_tos;
#endif

	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.		XXX
	 */
	off = th->th_off << 2;
	if (off < sizeof (struct tcphdr) || off > tlen) {
		TCPSTAT_INC(tcps_rcvbadoff);
		goto drop;
	}
	tlen -= off;	/* tlen is used instead of ti->ti_len */
	if (off > sizeof (struct tcphdr)) {
#ifdef INET6
		if (isipv6) {
			IP6_EXTHDR_CHECK(m, off0, off, );
			ip6 = mtod(m, struct ip6_hdr *);
			th = (struct tcphdr *)((caddr_t)ip6 + off0);
		}
#endif
#if defined(INET) && defined(INET6)
		else
#endif
#ifdef INET
		{
			if (m->m_len < sizeof(struct ip) + off) {
				if ((m = m_pullup(m, sizeof (struct ip) + off))
				    == NULL) {
					TCPSTAT_INC(tcps_rcvshort);
					return;
				}
				ip = mtod(m, struct ip *);
				ipov = (struct ipovly *)ip;
				th = (struct tcphdr *)((caddr_t)ip + off0);
			}
		}
#endif
		optlen = off - sizeof (struct tcphdr);
		optp = (u_char *)(th + 1);
	}
	thflags = th->th_flags;

	/*
	 * Convert TCP protocol specific fields to host format.
	 */
	tcp_fields_to_host(th);

	/*
	 * Delay dropping TCP, IP headers, IPv6 ext headers, and TCP options.
	 */
	drop_hdrlen = off0 + off;

	/*
	 * Locate pcb for segment; if we're likely to add or remove a
	 * connection then first acquire pcbinfo lock.  There are two cases
	 * where we might discover later we need a write lock despite the
	 * flags: ACKs moving a connection out of the syncache, and ACKs for
	 * a connection in TIMEWAIT.
	 */
	if ((thflags & (TH_SYN | TH_FIN | TH_RST)) != 0) {
		INP_INFO_WLOCK(&V_tcbinfo);
		ti_locked = TI_WLOCKED;
	} else
		ti_locked = TI_UNLOCKED;

findpcb:
#ifdef INVARIANTS
	if (ti_locked == TI_WLOCKED) {
		INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	} else {
		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	}
#endif

#ifdef IPFIREWALL_FORWARD
	/*
	 * Grab info from PACKET_TAG_IPFORWARD tag prepended to the chain.
	 */
	fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL);
#endif /* IPFIREWALL_FORWARD */

#ifdef INET6
#ifdef IPFIREWALL_FORWARD
	if (isipv6 && fwd_tag != NULL) {
		struct sockaddr_in6 *next_hop6;

		next_hop6 = (struct sockaddr_in6 *)(fwd_tag + 1);
		/*
		 * Transparently forwarded. Pretend to be the destination.
		 * Already got one like this?
		 */
		inp = in6_pcblookup_mbuf(&V_tcbinfo,
		    &ip6->ip6_src, th->th_sport, &ip6->ip6_dst, th->th_dport,
		    INPLOOKUP_WLOCKPCB, m->m_pkthdr.rcvif, m);
		if (!inp) {
			/*
			 * It's new.  Try to find the ambushing socket.
			 * Because we've rewritten the destination address,
			 * any hardware-generated hash is ignored.
			 */
			inp = in6_pcblookup(&V_tcbinfo, &ip6->ip6_src,
			    th->th_sport, &next_hop6->sin6_addr,
			    next_hop6->sin6_port ? ntohs(next_hop6->sin6_port) :
			    th->th_dport, INPLOOKUP_WILDCARD |
			    INPLOOKUP_WLOCKPCB, m->m_pkthdr.rcvif);
		}
		/* Remove the tag from the packet.  We don't need it anymore. */
		m_tag_delete(m, fwd_tag);
	} else
#endif /* IPFIREWALL_FORWARD */
	if (isipv6) {
		inp = in6_pcblookup_mbuf(&V_tcbinfo, &ip6->ip6_src,
		    th->th_sport, &ip6->ip6_dst, th->th_dport,
		    INPLOOKUP_WILDCARD | INPLOOKUP_WLOCKPCB,
		    m->m_pkthdr.rcvif, m);
	}
#endif /* INET6 */
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
#ifdef IPFIREWALL_FORWARD
	if (fwd_tag != NULL) {
		struct sockaddr_in *next_hop;

		next_hop = (struct sockaddr_in *)(fwd_tag+1);
		/*
		 * Transparently forwarded. Pretend to be the destination.
		 * already got one like this?
		 */
		inp = in_pcblookup_mbuf(&V_tcbinfo, ip->ip_src, th->th_sport,
		    ip->ip_dst, th->th_dport, INPLOOKUP_WLOCKPCB,
		    m->m_pkthdr.rcvif, m);
		if (!inp) {
			/*
			 * It's new.  Try to find the ambushing socket.
			 * Because we've rewritten the destination address,
			 * any hardware-generated hash is ignored.
			 */
			inp = in_pcblookup(&V_tcbinfo, ip->ip_src,
			    th->th_sport, next_hop->sin_addr,
			    next_hop->sin_port ? ntohs(next_hop->sin_port) :
			    th->th_dport, INPLOOKUP_WILDCARD |
			    INPLOOKUP_WLOCKPCB, m->m_pkthdr.rcvif);
		}
		/* Remove the tag from the packet.  We don't need it anymore. */
		m_tag_delete(m, fwd_tag);
	} else
#endif /* IPFIREWALL_FORWARD */
		inp = in_pcblookup_mbuf(&V_tcbinfo, ip->ip_src,
		    th->th_sport, ip->ip_dst, th->th_dport,
		    INPLOOKUP_WILDCARD | INPLOOKUP_WLOCKPCB,
		    m->m_pkthdr.rcvif, m);
#endif /* INET */

	/*
	 * If the INPCB does not exist then all data in the incoming
	 * segment is discarded and an appropriate RST is sent back.
	 * XXX MRT Send RST using which routing table?
	 */
	if (inp == NULL) {
		/*
		 * Log communication attempts to ports that are not
		 * in use.
		 */
		if ((tcp_log_in_vain == 1 && (thflags & TH_SYN)) ||
		    tcp_log_in_vain == 2) {
			if ((s = tcp_log_vain(NULL, th, (void *)ip, ip6)))
				log(LOG_INFO, "%s; %s: Connection attempt "
				    "to closed port\n", s, __func__);
		}
		/*
		 * When blackholing do not respond with a RST but
		 * completely ignore the segment and drop it.
		 */
		if ((V_blackhole == 1 && (thflags & TH_SYN)) ||
		    V_blackhole == 2)
			goto dropunlock;

		rstreason = BANDLIM_RST_CLOSEDPORT;
		goto dropwithreset;
	}
	INP_WLOCK_ASSERT(inp);
	if (!(inp->inp_flags & INP_HW_FLOWID)
	    && (m->m_flags & M_FLOWID)
	    && ((inp->inp_socket == NULL)
		|| !(inp->inp_socket->so_options & SO_ACCEPTCONN))) {
		inp->inp_flags |= INP_HW_FLOWID;
		inp->inp_flags &= ~INP_SW_FLOWID;
		inp->inp_flowid = m->m_pkthdr.flowid;
	}
#ifdef IPSEC
#ifdef INET6
	if (isipv6 && ipsec6_in_reject(m, inp)) {
		V_ipsec6stat.in_polvio++;
		goto dropunlock;
	} else
#endif /* INET6 */
	if (ipsec4_in_reject(m, inp) != 0) {
		V_ipsec4stat.in_polvio++;
		goto dropunlock;
	}
#endif /* IPSEC */

	/*
	 * Check the minimum TTL for socket.
	 */
	if (inp->inp_ip_minttl != 0) {
#ifdef INET6
		if (isipv6 && inp->inp_ip_minttl > ip6->ip6_hlim)
			goto dropunlock;
		else
#endif
		if (inp->inp_ip_minttl > ip->ip_ttl)
			goto dropunlock;
	}

	/*
	 * A previous connection in TIMEWAIT state is supposed to catch stray
	 * or duplicate segments arriving late.  If this segment was a
	 * legitimate new connection attempt the old INPCB gets removed and
	 * we can try again to find a listening socket.
	 *
	 * At this point, due to earlier optimism, we may hold only an inpcb
	 * lock, and not the inpcbinfo write lock.  If so, we need to try to
	 * acquire it, or if that fails, acquire a reference on the inpcb,
	 * drop all locks, acquire a global write lock, and then re-acquire
	 * the inpcb lock.  We may at that point discover that another thread
	 * has tried to free the inpcb, in which case we need to loop back
	 * and try to find a new inpcb to deliver to.
	 *
	 * XXXRW: It may be time to rethink timewait locking.
	 */
relocked:
	if (inp->inp_flags & INP_TIMEWAIT) {
		if (ti_locked == TI_UNLOCKED) {
			if (INP_INFO_TRY_WLOCK(&V_tcbinfo) == 0) {
				in_pcbref(inp);
				INP_WUNLOCK(inp);
				INP_INFO_WLOCK(&V_tcbinfo);
				ti_locked = TI_WLOCKED;
				INP_WLOCK(inp);
				if (in_pcbrele_wlocked(inp)) {
					inp = NULL;
					goto findpcb;
				}
			} else
				ti_locked = TI_WLOCKED;
		}
		INP_INFO_WLOCK_ASSERT(&V_tcbinfo);

		if (thflags & TH_SYN)
			tcp_dooptions(&to, optp, optlen, TO_SYN);
		/*
		 * NB: tcp_twcheck unlocks the INP and frees the mbuf.
		 */
		if (tcp_twcheck(inp, &to, th, m, tlen))
			goto findpcb;
		INP_INFO_WUNLOCK(&V_tcbinfo);
		return;
	}
	/*
	 * The TCPCB may no longer exist if the connection is winding
	 * down or it is in the CLOSED state.  Either way we drop the
	 * segment and send an appropriate response.
	 */
	tp = intotcpcb(inp);
	if (tp == NULL || tp->t_state == TCPS_CLOSED) {
		rstreason = BANDLIM_RST_CLOSEDPORT;
		goto dropwithreset;
	}

	/*
	 * We've identified a valid inpcb, but it could be that we need an
	 * inpcbinfo write lock but don't hold it.  In this case, attempt to
	 * acquire using the same strategy as the TIMEWAIT case above.  If we
	 * relock, we have to jump back to 'relocked' as the connection might
	 * now be in TIMEWAIT.
	 */
#ifdef INVARIANTS
	if ((thflags & (TH_SYN | TH_FIN | TH_RST)) != 0)
		INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
#endif
	if (tp->t_state != TCPS_ESTABLISHED) {
		if (ti_locked == TI_UNLOCKED) {
			if (INP_INFO_TRY_WLOCK(&V_tcbinfo) == 0) {
				in_pcbref(inp);
				INP_WUNLOCK(inp);
				INP_INFO_WLOCK(&V_tcbinfo);
				ti_locked = TI_WLOCKED;
				INP_WLOCK(inp);
				if (in_pcbrele_wlocked(inp)) {
					inp = NULL;
					goto findpcb;
				}
				goto relocked;
			} else
				ti_locked = TI_WLOCKED;
		}
		INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	}

#ifdef MAC
	INP_WLOCK_ASSERT(inp);
	if (mac_inpcb_check_deliver(inp, m))
		goto dropunlock;
#endif
	so = inp->inp_socket;
	KASSERT(so != NULL, ("%s: so == NULL", __func__));
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG) {
		ostate = tp->t_state;
#ifdef INET6
		if (isipv6) {
			bcopy((char *)ip6, (char *)tcp_saveipgen, sizeof(*ip6));
		} else
#endif
			bcopy((char *)ip, (char *)tcp_saveipgen, sizeof(*ip));
		tcp_savetcp = *th;
	}
#endif /* TCPDEBUG */
	/*
	 * When the socket is accepting connections (the INPCB is in LISTEN
	 * state) we look into the SYN cache if this is a new connection
	 * attempt or the completion of a previous one.  Because listen
	 * sockets are never in TCPS_ESTABLISHED, the V_tcbinfo lock will be
	 * held in this case.
	 */
	if (so->so_options & SO_ACCEPTCONN) {
		struct in_conninfo inc;

		KASSERT(tp->t_state == TCPS_LISTEN, ("%s: so accepting but "
		    "tp not listening", __func__));
		INP_INFO_WLOCK_ASSERT(&V_tcbinfo);

		bzero(&inc, sizeof(inc));
#ifdef INET6
		if (isipv6) {
			inc.inc_flags |= INC_ISIPV6;
			inc.inc6_faddr = ip6->ip6_src;
			inc.inc6_laddr = ip6->ip6_dst;
		} else
#endif
		{
			inc.inc_faddr = ip->ip_src;
			inc.inc_laddr = ip->ip_dst;
		}
		inc.inc_fport = th->th_sport;
		inc.inc_lport = th->th_dport;
		inc.inc_fibnum = so->so_fibnum;

		/*
		 * Check for an existing connection attempt in syncache if
		 * the flag is only ACK.  A successful lookup creates a new
		 * socket appended to the listen queue in SYN_RECEIVED state.
		 */
		if ((thflags & (TH_RST|TH_ACK|TH_SYN)) == TH_ACK) {
			/*
			 * Parse the TCP options here because
			 * syncookies need access to the reflected
			 * timestamp.
			 */
			tcp_dooptions(&to, optp, optlen, 0);
			/*
			 * NB: syncache_expand() doesn't unlock
			 * inp and tcpinfo locks.
			 */
			if (!syncache_expand(&inc, &to, th, &so, m)) {
				/*
				 * No syncache entry or ACK was not
				 * for our SYN/ACK.  Send a RST.
				 * NB: syncache did its own logging
				 * of the failure cause.
				 */
				rstreason = BANDLIM_RST_OPENPORT;
				goto dropwithreset;
			}
			if (so == NULL) {
				/*
				 * We completed the 3-way handshake
				 * but could not allocate a socket
				 * either due to memory shortage,
				 * listen queue length limits or
				 * global socket limits.  Send RST
				 * or wait and have the remote end
				 * retransmit the ACK for another
				 * try.
				 */
				if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
					log(LOG_DEBUG, "%s; %s: Listen socket: "
					    "Socket allocation failed due to "
					    "limits or memory shortage, %s\n",
					    s, __func__,
					    V_tcp_sc_rst_sock_fail ?
					    "sending RST" : "try again");
				if (V_tcp_sc_rst_sock_fail) {
					rstreason = BANDLIM_UNLIMITED;
					goto dropwithreset;
				} else
					goto dropunlock;
			}
			/*
			 * Socket is created in state SYN_RECEIVED.
			 * Unlock the listen socket, lock the newly
			 * created socket and update the tp variable.
			 */
			INP_WUNLOCK(inp);	/* listen socket */
			inp = sotoinpcb(so);
			INP_WLOCK(inp);		/* new connection */
			tp = intotcpcb(inp);
			KASSERT(tp->t_state == TCPS_SYN_RECEIVED,
			    ("%s: ", __func__));
#ifdef TCP_SIGNATURE
			if (sig_checked == 0)  {
				tcp_dooptions(&to, optp, optlen,
				    (thflags & TH_SYN) ? TO_SYN : 0);
				if (!tcp_signature_verify_input(m, off0, tlen,
				    optlen, &to, th, tp->t_flags)) {

					/*
					 * In SYN_SENT state if it receives an
					 * RST, it is allowed for further
					 * processing.
					 */
					if ((thflags & TH_RST) == 0 ||
					    (tp->t_state == TCPS_SYN_SENT) == 0)
						goto dropunlock;
				}
				sig_checked = 1;
			}
#endif

			/*
			 * Process the segment and the data it
			 * contains.  tcp_do_segment() consumes
			 * the mbuf chain and unlocks the inpcb.
			 */
			tcp_do_segment(m, th, so, tp, drop_hdrlen, tlen,
			    iptos, ti_locked);
			INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
			return;
		}
		/*
		 * Segment flag validation for new connection attempts:
		 *
		 * Our (SYN|ACK) response was rejected.
		 * Check with syncache and remove entry to prevent
		 * retransmits.
		 *
		 * NB: syncache_chkrst does its own logging of failure
		 * causes.
		 */
		if (thflags & TH_RST) {
			syncache_chkrst(&inc, th);
			goto dropunlock;
		}
		/*
		 * We can't do anything without SYN.
		 */
		if ((thflags & TH_SYN) == 0) {
			if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				log(LOG_DEBUG, "%s; %s: Listen socket: "
				    "SYN is missing, segment ignored\n",
				    s, __func__);
			TCPSTAT_INC(tcps_badsyn);
			goto dropunlock;
		}
		/*
		 * (SYN|ACK) is bogus on a listen socket.
		 */
		if (thflags & TH_ACK) {
			if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				log(LOG_DEBUG, "%s; %s: Listen socket: "
				    "SYN|ACK invalid, segment rejected\n",
				    s, __func__);
			syncache_badack(&inc);	/* XXX: Not needed! */
			TCPSTAT_INC(tcps_badsyn);
			rstreason = BANDLIM_RST_OPENPORT;
			goto dropwithreset;
		}
		/*
		 * If the drop_synfin option is enabled, drop all
		 * segments with both the SYN and FIN bits set.
		 * This prevents e.g. nmap from identifying the
		 * TCP/IP stack.
		 * XXX: Poor reasoning.  nmap has other methods
		 * and is constantly refining its stack detection
		 * strategies.
		 * XXX: This is a violation of the TCP specification
		 * and was used by RFC1644.
		 */
		if ((thflags & TH_FIN) && V_drop_synfin) {
			if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				log(LOG_DEBUG, "%s; %s: Listen socket: "
				    "SYN|FIN segment ignored (based on "
				    "sysctl setting)\n", s, __func__);
			TCPSTAT_INC(tcps_badsyn);
			goto dropunlock;
		}
		/*
		 * Segment's flags are (SYN) or (SYN|FIN).
		 *
		 * TH_PUSH, TH_URG, TH_ECE, TH_CWR are ignored
		 * as they do not affect the state of the TCP FSM.
		 * The data pointed to by TH_URG and th_urp is ignored.
		 */
		KASSERT((thflags & (TH_RST|TH_ACK)) == 0,
		    ("%s: Listen socket: TH_RST or TH_ACK set", __func__));
		KASSERT(thflags & (TH_SYN),
		    ("%s: Listen socket: TH_SYN not set", __func__));
#ifdef INET6
		/*
		 * If deprecated address is forbidden,
		 * we do not accept SYN to deprecated interface
		 * address to prevent any new inbound connection from
		 * getting established.
		 * When we do not accept SYN, we send a TCP RST,
		 * with deprecated source address (instead of dropping
		 * it).  We compromise it as it is much better for peer
		 * to send a RST, and RST will be the final packet
		 * for the exchange.
		 *
		 * If we do not forbid deprecated addresses, we accept
		 * the SYN packet.  RFC2462 does not suggest dropping
		 * SYN in this case.
		 * If we decipher RFC2462 5.5.4, it says like this:
		 * 1. use of deprecated addr with existing
		 *    communication is okay - "SHOULD continue to be
		 *    used"
		 * 2. use of it with new communication:
		 *   (2a) "SHOULD NOT be used if alternate address
		 *        with sufficient scope is available"
		 *   (2b) nothing mentioned otherwise.
		 * Here we fall into (2b) case as we have no choice in
		 * our source address selection - we must obey the peer.
		 *
		 * The wording in RFC2462 is confusing, and there are
		 * multiple description text for deprecated address
		 * handling - worse, they are not exactly the same.
		 * I believe 5.5.4 is the best one, so we follow 5.5.4.
		 */
		if (isipv6 && !V_ip6_use_deprecated) {
			struct in6_ifaddr *ia6;

			ia6 = ip6_getdstifaddr(m);
			if (ia6 != NULL &&
			    (ia6->ia6_flags & IN6_IFF_DEPRECATED)) {
				ifa_free(&ia6->ia_ifa);
				if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				    log(LOG_DEBUG, "%s; %s: Listen socket: "
					"Connection attempt to deprecated "
					"IPv6 address rejected\n",
					s, __func__);
				rstreason = BANDLIM_RST_OPENPORT;
				goto dropwithreset;
			}
			ifa_free(&ia6->ia_ifa);
		}
#endif /* INET6 */
		/*
		 * Basic sanity checks on incoming SYN requests:
		 *   Don't respond if the destination is a link layer
		 *	broadcast according to RFC1122 4.2.3.10, p. 104.
		 *   If it is from this socket it must be forged.
		 *   Don't respond if the source or destination is a
		 *	global or subnet broad- or multicast address.
		 *   Note that it is quite possible to receive unicast
		 *	link-layer packets with a broadcast IP address. Use
		 *	in_broadcast() to find them.
		 */
		if (m->m_flags & (M_BCAST|M_MCAST)) {
			if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
			    log(LOG_DEBUG, "%s; %s: Listen socket: "
				"Connection attempt from broad- or multicast "
				"link layer address ignored\n", s, __func__);
			goto dropunlock;
		}
#ifdef INET6
		if (isipv6) {
			if (th->th_dport == th->th_sport &&
			    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &ip6->ip6_src)) {
				if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				    log(LOG_DEBUG, "%s; %s: Listen socket: "
					"Connection attempt to/from self "
					"ignored\n", s, __func__);
				goto dropunlock;
			}
			if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
			    IN6_IS_ADDR_MULTICAST(&ip6->ip6_src)) {
				if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				    log(LOG_DEBUG, "%s; %s: Listen socket: "
					"Connection attempt from/to multicast "
					"address ignored\n", s, __func__);
				goto dropunlock;
			}
		}
#endif
#if defined(INET) && defined(INET6)
		else
#endif
#ifdef INET
		{
			if (th->th_dport == th->th_sport &&
			    ip->ip_dst.s_addr == ip->ip_src.s_addr) {
				if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				    log(LOG_DEBUG, "%s; %s: Listen socket: "
					"Connection attempt from/to self "
					"ignored\n", s, __func__);
				goto dropunlock;
			}
			if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
			    IN_MULTICAST(ntohl(ip->ip_src.s_addr)) ||
			    ip->ip_src.s_addr == htonl(INADDR_BROADCAST) ||
			    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif)) {
				if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				    log(LOG_DEBUG, "%s; %s: Listen socket: "
					"Connection attempt from/to broad- "
					"or multicast address ignored\n",
					s, __func__);
				goto dropunlock;
			}
		}
#endif
		/*
		 * SYN appears to be valid.  Create compressed TCP state
		 * for syncache.
		 */
#ifdef TCPDEBUG
		if (so->so_options & SO_DEBUG)
			tcp_trace(TA_INPUT, ostate, tp,
			    (void *)tcp_saveipgen, &tcp_savetcp, 0);
#endif
		tcp_dooptions(&to, optp, optlen, TO_SYN);
		syncache_add(&inc, &to, th, inp, &so, m);
		/*
		 * Entry added to syncache and mbuf consumed.
		 * Everything already unlocked by syncache_add().
		 */
		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
		return;
	}

#ifdef TCP_SIGNATURE
	if (sig_checked == 0)  {
		tcp_dooptions(&to, optp, optlen,
		    (thflags & TH_SYN) ? TO_SYN : 0);
		if (!tcp_signature_verify_input(m, off0, tlen, optlen, &to,
		    th, tp->t_flags)) {

			/*
			 * In SYN_SENT state if it receives an RST, it is
			 * allowed for further processing.
			 */
			if ((thflags & TH_RST) == 0 ||
			    (tp->t_state == TCPS_SYN_SENT) == 0)
				goto dropunlock;
		}
		sig_checked = 1;
	}
#endif

	/*
	 * Segment belongs to a connection in SYN_SENT, ESTABLISHED or later
	 * state.  tcp_do_segment() always consumes the mbuf chain, unlocks
	 * the inpcb, and unlocks pcbinfo.
	 */
	tcp_do_segment(m, th, so, tp, drop_hdrlen, tlen, iptos, ti_locked);
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	return;

dropwithreset:
	if (ti_locked == TI_WLOCKED) {
		INP_INFO_WUNLOCK(&V_tcbinfo);
		ti_locked = TI_UNLOCKED;
	}
#ifdef INVARIANTS
	else {
		KASSERT(ti_locked == TI_UNLOCKED, ("%s: dropwithreset "
		    "ti_locked: %d", __func__, ti_locked));
		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	}
#endif

	if (inp != NULL) {
		tcp_dropwithreset(m, th, tp, tlen, rstreason);
		INP_WUNLOCK(inp);
	} else
		tcp_dropwithreset(m, th, NULL, tlen, rstreason);
	m = NULL;	/* mbuf chain got consumed. */
	goto drop;

dropunlock:
	if (ti_locked == TI_WLOCKED) {
		INP_INFO_WUNLOCK(&V_tcbinfo);
		ti_locked = TI_UNLOCKED;
	}
#ifdef INVARIANTS
	else {
		KASSERT(ti_locked == TI_UNLOCKED, ("%s: dropunlock "
		    "ti_locked: %d", __func__, ti_locked));
		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	}
#endif

	if (inp != NULL)
		INP_WUNLOCK(inp);

drop:
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	if (s != NULL)
		free(s, M_TCPLOG);
	if (m != NULL)
		m_freem(m);
}

static void
tcp_do_segment(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, int drop_hdrlen, int tlen, uint8_t iptos,
    int ti_locked)
{
	int thflags, acked, ourfinisacked, needoutput = 0;
	int rstreason, todrop, win;
	u_long tiwin;
	struct tcpopt to;

#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[IP6_HDR_LEN];
	struct tcphdr tcp_savetcp;
	short ostate = 0;
#endif
	thflags = th->th_flags;
	tp->sackhint.last_sack_ack = 0;

	/*
	 * If this is either a state-changing packet or current state isn't
	 * established, we require a write lock on tcbinfo.  Otherwise, we
	 * allow either a read lock or a write lock, as we may have acquired
	 * a write lock due to a race.
	 *
	 * Require a global write lock for SYN/FIN/RST segments or
	 * non-established connections; otherwise accept either a read or
	 * write lock, as we may have conservatively acquired a write lock in
	 * certain cases in tcp_input() (is this still true?).  Currently we
	 * will never enter with no lock, so we try to drop it quickly in the
	 * common pure ack/pure data cases.
	 */
	if ((thflags & (TH_SYN | TH_FIN | TH_RST)) != 0 ||
	    tp->t_state != TCPS_ESTABLISHED) {
		KASSERT(ti_locked == TI_WLOCKED, ("%s ti_locked %d for "
		    "SYN/FIN/RST/!EST", __func__, ti_locked));
		INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	} else {
#ifdef INVARIANTS
		if (ti_locked == TI_WLOCKED)
			INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
		else {
			KASSERT(ti_locked == TI_UNLOCKED, ("%s: EST "
			    "ti_locked: %d", __func__, ti_locked));
			INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
		}
#endif
	}
	INP_WLOCK_ASSERT(tp->t_inpcb);
	KASSERT(tp->t_state > TCPS_LISTEN, ("%s: TCPS_LISTEN",
	    __func__));
	KASSERT(tp->t_state != TCPS_TIME_WAIT, ("%s: TCPS_TIME_WAIT",
	    __func__));

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 * XXX: This should be done after segment
	 * validation to ignore broken/spoofed segs.
	 */
	tp->t_rcvtime = ticks;
	if (TCPS_HAVEESTABLISHED(tp->t_state))
		tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp));

	/*
	 * Unscale the window into a 32-bit value.
	 * For the SYN_SENT state the scale is zero.
	 */
	tiwin = th->th_win << tp->snd_scale;

	/*
	 * TCP ECN processing.
	 */
	if (tp->t_flags & TF_ECN_PERMIT) {
		if (thflags & TH_CWR)
			tp->t_flags &= ~TF_ECN_SND_ECE;
		switch (iptos & IPTOS_ECN_MASK) {
		case IPTOS_ECN_CE:
			tp->t_flags |= TF_ECN_SND_ECE;
			TCPSTAT_INC(tcps_ecn_ce);
			break;
		case IPTOS_ECN_ECT0:
			TCPSTAT_INC(tcps_ecn_ect0);
			break;
		case IPTOS_ECN_ECT1:
			TCPSTAT_INC(tcps_ecn_ect1);
			break;
		}
		/* Congestion experienced. */
		if (thflags & TH_ECE) {
			cc_cong_signal(tp, th, CC_ECN);
		}
	}

	/*
	 * Parse options on any incoming segment.
	 */
	tcp_dooptions(&to, (u_char *)(th + 1),
	    (th->th_off << 2) - sizeof(struct tcphdr),
	    (thflags & TH_SYN) ? TO_SYN : 0);

	/*
	 * If echoed timestamp is later than the current time,
	 * fall back to non RFC1323 RTT calculation.  Normalize
	 * timestamp if syncookies were used when this connection
	 * was established.
	 */
	if ((to.to_flags & TOF_TS) && (to.to_tsecr != 0)) {
		to.to_tsecr -= tp->ts_offset;
		if (TSTMP_GT(to.to_tsecr, ticks))
			to.to_tsecr = 0;
	}

	/*
	 * Process options only when we get SYN/ACK back. The SYN case
	 * for incoming connections is handled in tcp_syncache.
	 * According to RFC1323 the window field in a SYN (i.e., a <SYN>
	 * or <SYN,ACK>) segment itself is never scaled.
	 * XXX this is traditional behavior, may need to be cleaned up.
	 */
	if (tp->t_state == TCPS_SYN_SENT && (thflags & TH_SYN)) {
		if ((to.to_flags & TOF_SCALE) &&
		    (tp->t_flags & TF_REQ_SCALE)) {
			tp->t_flags |= TF_RCVD_SCALE;
			tp->snd_scale = to.to_wscale;
		}
		/*
		 * Initial send window.  It will be updated with
		 * the next incoming segment to the scaled value.
		 */
		tp->snd_wnd = th->th_win;
		if (to.to_flags & TOF_TS) {
			tp->t_flags |= TF_RCVD_TSTMP;
			tp->ts_recent = to.to_tsval;
			tp->ts_recent_age = ticks;
		}
		if (to.to_flags & TOF_MSS)
			tcp_mss(tp, to.to_mss);
		if ((tp->t_flags & TF_SACK_PERMIT) &&
		    (to.to_flags & TOF_SACKPERM) == 0)
			tp->t_flags &= ~TF_SACK_PERMIT;
	}

	/*
	 * Header prediction: check for the two common cases
	 * of a uni-directional data xfer.  If the packet has
	 * no control flags, is in-sequence, the window didn't
	 * change and we're not retransmitting, it's a
	 * candidate.  If the length is zero and the ack moved
	 * forward, we're the sender side of the xfer.  Just
	 * free the data acked & wake any higher level process
	 * that was blocked waiting for space.  If the length
	 * is non-zero and the ack didn't move, we're the
	 * receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data to
	 * the socket buffer and note that we need a delayed ack.
	 * Make sure that the hidden state-flags are also off.
	 * Since we check for TCPS_ESTABLISHED first, it can only
	 * be TH_NEEDSYN.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    th->th_seq == tp->rcv_nxt &&
	    (thflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
	    tp->snd_nxt == tp->snd_max &&
	    tiwin && tiwin == tp->snd_wnd && 
	    ((tp->t_flags & (TF_NEEDSYN|TF_NEEDFIN)) == 0) &&
	    LIST_EMPTY(&tp->t_segq) &&
	    ((to.to_flags & TOF_TS) == 0 ||
	     TSTMP_GEQ(to.to_tsval, tp->ts_recent)) ) {

		/*
		 * If last ACK falls within this segment's sequence numbers,
		 * record the timestamp.
		 * NOTE that the test is modified according to the latest
		 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
		 */
		if ((to.to_flags & TOF_TS) != 0 &&
		    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
			tp->ts_recent_age = ticks;
			tp->ts_recent = to.to_tsval;
		}

		if (tlen == 0) {
			if (SEQ_GT(th->th_ack, tp->snd_una) &&
			    SEQ_LEQ(th->th_ack, tp->snd_max) &&
			    !IN_RECOVERY(tp->t_flags) &&
			    (to.to_flags & TOF_SACK) == 0 &&
			    TAILQ_EMPTY(&tp->snd_holes)) {
				/*
				 * This is a pure ack for outstanding data.
				 */
				if (ti_locked == TI_WLOCKED)
					INP_INFO_WUNLOCK(&V_tcbinfo);
				ti_locked = TI_UNLOCKED;

				TCPSTAT_INC(tcps_predack);

				/*
				 * "bad retransmit" recovery.
				 */
				if (tp->t_rxtshift == 1 &&
				    tp->t_flags & TF_PREVVALID &&
				    (int)(ticks - tp->t_badrxtwin) < 0) {
					cc_cong_signal(tp, th, CC_RTO_ERR);
				}

				/*
				 * Recalculate the transmit timer / rtt.
				 *
				 * Some boxes send broken timestamp replies
				 * during the SYN+ACK phase, ignore
				 * timestamps of 0 or we could calculate a
				 * huge RTT and blow up the retransmit timer.
				 */
				if ((to.to_flags & TOF_TS) != 0 &&
				    to.to_tsecr) {
					if (!tp->t_rttlow ||
					    tp->t_rttlow > ticks - to.to_tsecr)
						tp->t_rttlow = ticks - to.to_tsecr;
					tcp_xmit_timer(tp,
					    ticks - to.to_tsecr + 1);
				} else if (tp->t_rtttime &&
				    SEQ_GT(th->th_ack, tp->t_rtseq)) {
					if (!tp->t_rttlow ||
					    tp->t_rttlow > ticks - tp->t_rtttime)
						tp->t_rttlow = ticks - tp->t_rtttime;
					tcp_xmit_timer(tp,
							ticks - tp->t_rtttime);
				}
				acked = BYTES_THIS_ACK(tp, th);

				/* Run HHOOK_TCP_ESTABLISHED_IN helper hooks. */
				hhook_run_tcp_est_in(tp, th, &to);

				TCPSTAT_INC(tcps_rcvackpack);
				TCPSTAT_ADD(tcps_rcvackbyte, acked);
				sbdrop(&so->so_snd, acked);
				if (SEQ_GT(tp->snd_una, tp->snd_recover) &&
				    SEQ_LEQ(th->th_ack, tp->snd_recover))
					tp->snd_recover = th->th_ack - 1;
				
				/*
				 * Let the congestion control algorithm update
				 * congestion control related information. This
				 * typically means increasing the congestion
				 * window.
				 */
				cc_ack_received(tp, th, CC_ACK);

				tp->snd_una = th->th_ack;
				/*
				 * Pull snd_wl2 up to prevent seq wrap relative
				 * to th_ack.
				 */
				tp->snd_wl2 = th->th_ack;
				tp->t_dupacks = 0;
				m_freem(m);
				ND6_HINT(tp); /* Some progress has been made. */

				/*
				 * If all outstanding data are acked, stop
				 * retransmit timer, otherwise restart timer
				 * using current (possibly backed-off) value.
				 * If process is waiting for space,
				 * wakeup/selwakeup/signal.  If data
				 * are ready to send, let tcp_output
				 * decide between more output or persist.
				 */
#ifdef TCPDEBUG
				if (so->so_options & SO_DEBUG)
					tcp_trace(TA_INPUT, ostate, tp,
					    (void *)tcp_saveipgen,
					    &tcp_savetcp, 0);
#endif
				if (tp->snd_una == tp->snd_max)
					tcp_timer_activate(tp, TT_REXMT, 0);
				else if (!tcp_timer_active(tp, TT_PERSIST))
					tcp_timer_activate(tp, TT_REXMT,
						      tp->t_rxtcur);
				sowwakeup(so);
				if (so->so_snd.sb_cc)
					(void) tcp_output(tp);
				goto check_delack;
			}
		} else if (th->th_ack == tp->snd_una &&
		    tlen <= sbspace(&so->so_rcv)) {
			int newsize = 0;	/* automatic sockbuf scaling */

			/*
			 * This is a pure, in-sequence data packet with
			 * nothing on the reassembly queue and we have enough
			 * buffer space to take it.
			 */
			if (ti_locked == TI_WLOCKED)
				INP_INFO_WUNLOCK(&V_tcbinfo);
			ti_locked = TI_UNLOCKED;

			/* Clean receiver SACK report if present */
			if ((tp->t_flags & TF_SACK_PERMIT) && tp->rcv_numsacks)
				tcp_clean_sackreport(tp);
			TCPSTAT_INC(tcps_preddat);
			tp->rcv_nxt += tlen;
			/*
			 * Pull snd_wl1 up to prevent seq wrap relative to
			 * th_seq.
			 */
			tp->snd_wl1 = th->th_seq;
			/*
			 * Pull rcv_up up to prevent seq wrap relative to
			 * rcv_nxt.
			 */
			tp->rcv_up = tp->rcv_nxt;
			TCPSTAT_INC(tcps_rcvpack);
			TCPSTAT_ADD(tcps_rcvbyte, tlen);
			ND6_HINT(tp);	/* Some progress has been made */
#ifdef TCPDEBUG
			if (so->so_options & SO_DEBUG)
				tcp_trace(TA_INPUT, ostate, tp,
				    (void *)tcp_saveipgen, &tcp_savetcp, 0);
#endif
		/*
		 * Automatic sizing of receive socket buffer.  Often the send
		 * buffer size is not optimally adjusted to the actual network
		 * conditions at hand (delay bandwidth product).  Setting the
		 * buffer size too small limits throughput on links with high
		 * bandwidth and high delay (eg. trans-continental/oceanic links).
		 *
		 * On the receive side the socket buffer memory is only rarely
		 * used to any significant extent.  This allows us to be much
		 * more aggressive in scaling the receive socket buffer.  For
		 * the case that the buffer space is actually used to a large
		 * extent and we run out of kernel memory we can simply drop
		 * the new segments; TCP on the sender will just retransmit it
		 * later.  Setting the buffer size too big may only consume too
		 * much kernel memory if the application doesn't read() from
		 * the socket or packet loss or reordering makes use of the
		 * reassembly queue.
		 *
		 * The criteria to step up the receive buffer one notch are:
		 *  1. the number of bytes received during the time it takes
		 *     one timestamp to be reflected back to us (the RTT);
		 *  2. received bytes per RTT is within seven eighth of the
		 *     current socket buffer size;
		 *  3. receive buffer size has not hit maximal automatic size;
		 *
		 * This algorithm does one step per RTT at most and only if
		 * we receive a bulk stream w/o packet losses or reorderings.
		 * Shrinking the buffer during idle times is not necessary as
		 * it doesn't consume any memory when idle.
		 *
		 * TODO: Only step up if the application is actually serving
		 * the buffer to better manage the socket buffer resources.
		 */
			if (V_tcp_do_autorcvbuf &&
			    to.to_tsecr &&
			    (so->so_rcv.sb_flags & SB_AUTOSIZE)) {
				if (TSTMP_GT(to.to_tsecr, tp->rfbuf_ts) &&
				    to.to_tsecr - tp->rfbuf_ts < hz) {
					if (tp->rfbuf_cnt >
					    (so->so_rcv.sb_hiwat / 8 * 7) &&
					    so->so_rcv.sb_hiwat <
					    V_tcp_autorcvbuf_max) {
						newsize =
						    min(so->so_rcv.sb_hiwat +
						    V_tcp_autorcvbuf_inc,
						    V_tcp_autorcvbuf_max);
					}
					/* Start over with next RTT. */
					tp->rfbuf_ts = 0;
					tp->rfbuf_cnt = 0;
				} else
					tp->rfbuf_cnt += tlen;	/* add up */
			}

			/* Add data to socket buffer. */
			SOCKBUF_LOCK(&so->so_rcv);
			if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
				m_freem(m);
			} else {
				/*
				 * Set new socket buffer size.
				 * Give up when limit is reached.
				 */
				if (newsize)
					if (!sbreserve_locked(&so->so_rcv,
					    newsize, so, NULL))
						so->so_rcv.sb_flags &= ~SB_AUTOSIZE;
				m_adj(m, drop_hdrlen);	/* delayed header drop */
				sbappendstream_locked(&so->so_rcv, m);
			}
			/* NB: sorwakeup_locked() does an implicit unlock. */
			sorwakeup_locked(so);
			if (DELAY_ACK(tp)) {
				tp->t_flags |= TF_DELACK;
			} else {
				tp->t_flags |= TF_ACKNOW;
				tcp_output(tp);
			}
			goto check_delack;
		}
	}

	/*
	 * Calculate amount of space in receive window,
	 * and then do TCP input processing.
	 * Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
	win = sbspace(&so->so_rcv);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = imax(win, (int)(tp->rcv_adv - tp->rcv_nxt));

	/* Reset receive buffer auto scaling when not in bulk receive mode. */
	tp->rfbuf_ts = 0;
	tp->rfbuf_cnt = 0;

	switch (tp->t_state) {

	/*
	 * If the state is SYN_RECEIVED:
	 *	if seg contains an ACK, but not for our SYN/ACK, send a RST.
	 */
	case TCPS_SYN_RECEIVED:
		if ((thflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->snd_una) ||
		     SEQ_GT(th->th_ack, tp->snd_max))) {
				rstreason = BANDLIM_RST_OPENPORT;
				goto dropwithreset;
		}
		break;

	/*
	 * If the state is SYN_SENT:
	 *	if seg contains an ACK, but not for our SYN, drop the input.
	 *	if seg contains a RST, then drop the connection.
	 *	if seg does not contain SYN, then drop it.
	 * Otherwise this is an acceptable SYN segment
	 *	initialize tp->rcv_nxt and tp->irs
	 *	if seg contains ack then advance tp->snd_una
	 *	if seg contains an ECE and ECN support is enabled, the stream
	 *	    is ECN capable.
	 *	if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 *	arrange for segment to be acked (eventually)
	 *	continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_SENT:
		if ((thflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->iss) ||
		     SEQ_GT(th->th_ack, tp->snd_max))) {
			rstreason = BANDLIM_UNLIMITED;
			goto dropwithreset;
		}
		if ((thflags & (TH_ACK|TH_RST)) == (TH_ACK|TH_RST))
			tp = tcp_drop(tp, ECONNREFUSED);
		if (thflags & TH_RST)
			goto drop;
		if (!(thflags & TH_SYN))
			goto drop;

		tp->irs = th->th_seq;
		tcp_rcvseqinit(tp);
		if (thflags & TH_ACK) {
			TCPSTAT_INC(tcps_connects);
			soisconnected(so);
#ifdef MAC
			mac_socketpeer_set_from_mbuf(m, so);
#endif
			/* Do window scaling on this connection? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->rcv_scale = tp->request_r_scale;
			}
			tp->rcv_adv += imin(tp->rcv_wnd,
			    TCP_MAXWIN << tp->rcv_scale);
			tp->snd_una++;		/* SYN is acked */
			/*
			 * If there's data, delay ACK; if there's also a FIN
			 * ACKNOW will be turned on later.
			 */
			if (DELAY_ACK(tp) && tlen != 0)
				tcp_timer_activate(tp, TT_DELACK,
				    tcp_delacktime);
			else
				tp->t_flags |= TF_ACKNOW;

			if ((thflags & TH_ECE) && V_tcp_do_ecn) {
				tp->t_flags |= TF_ECN_PERMIT;
				TCPSTAT_INC(tcps_ecn_shs);
			}
			
			/*
			 * Received <SYN,ACK> in SYN_SENT[*] state.
			 * Transitions:
			 *	SYN_SENT  --> ESTABLISHED
			 *	SYN_SENT* --> FIN_WAIT_1
			 */
			tp->t_starttime = ticks;
			if (tp->t_flags & TF_NEEDFIN) {
				tp->t_state = TCPS_FIN_WAIT_1;
				tp->t_flags &= ~TF_NEEDFIN;
				thflags &= ~TH_SYN;
			} else {
				tp->t_state = TCPS_ESTABLISHED;
				cc_conn_init(tp);
				tcp_timer_activate(tp, TT_KEEP,
				    TP_KEEPIDLE(tp));
			}
		} else {
			/*
			 * Received initial SYN in SYN-SENT[*] state =>
			 * simultaneous open.  If segment contains CC option
			 * and there is a cached CC, apply TAO test.
			 * If it succeeds, connection is * half-synchronized.
			 * Otherwise, do 3-way handshake:
			 *        SYN-SENT -> SYN-RECEIVED
			 *        SYN-SENT* -> SYN-RECEIVED*
			 * If there was no CC option, clear cached CC value.
			 */
			tp->t_flags |= (TF_ACKNOW | TF_NEEDSYN);
			tcp_timer_activate(tp, TT_REXMT, 0);
			tp->t_state = TCPS_SYN_RECEIVED;
		}

		KASSERT(ti_locked == TI_WLOCKED, ("%s: trimthenstep6: "
		    "ti_locked %d", __func__, ti_locked));
		INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
		INP_WLOCK_ASSERT(tp->t_inpcb);

		/*
		 * Advance th->th_seq to correspond to first data byte.
		 * If data, trim to stay within window,
		 * dropping FIN if necessary.
		 */
		th->th_seq++;
		if (tlen > tp->rcv_wnd) {
			todrop = tlen - tp->rcv_wnd;
			m_adj(m, -todrop);
			tlen = tp->rcv_wnd;
			thflags &= ~TH_FIN;
			TCPSTAT_INC(tcps_rcvpackafterwin);
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		}
		tp->snd_wl1 = th->th_seq - 1;
		tp->rcv_up = th->th_seq;
		/*
		 * Client side of transaction: already sent SYN and data.
		 * If the remote host used T/TCP to validate the SYN,
		 * our data will be ACK'd; if so, enter normal data segment
		 * processing in the middle of step 5, ack processing.
		 * Otherwise, goto step 6.
		 */
		if (thflags & TH_ACK)
			goto process_ACK;

		goto step6;

	/*
	 * If the state is LAST_ACK or CLOSING or TIME_WAIT:
	 *      do normal processing.
	 *
	 * NB: Leftover from RFC1644 T/TCP.  Cases to be reused later.
	 */
	case TCPS_LAST_ACK:
	case TCPS_CLOSING:
		break;  /* continue normal processing */
	}

	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check the RST flag and sequence number since reset segments
	 * are exempt from the timestamp and connection count tests.  This
	 * fixes a bug introduced by the Stevens, vol. 2, p. 960 bugfix
	 * below which allowed reset segments in half the sequence space
	 * to fall though and be processed (which gives forged reset
	 * segments with a random sequence number a 50 percent chance of
	 * killing a connection).
	 * Then check timestamp, if present.
	 * Then check the connection count, if present.
	 * Then check that at least some bytes of segment are within
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 *
	 *
	 * If the RST bit is set, check the sequence number to see
	 * if this is a valid reset segment.
	 * RFC 793 page 37:
	 *   In all states except SYN-SENT, all reset (RST) segments
	 *   are validated by checking their SEQ-fields.  A reset is
	 *   valid if its sequence number is in the window.
	 * Note: this does not take into account delayed ACKs, so
	 *   we should test against last_ack_sent instead of rcv_nxt.
	 *   The sequence number in the reset segment is normally an
	 *   echo of our outgoing acknowlegement numbers, but some hosts
	 *   send a reset with the sequence number at the rightmost edge
	 *   of our receive window, and we have to handle this case.
	 * Note 2: Paul Watson's paper "Slipping in the Window" has shown
	 *   that brute force RST attacks are possible.  To combat this,
	 *   we use a much stricter check while in the ESTABLISHED state,
	 *   only accepting RSTs where the sequence number is equal to
	 *   last_ack_sent.  In all other states (the states in which a
	 *   RST is more likely), the more permissive check is used.
	 * If we have multiple segments in flight, the initial reset
	 * segment sequence numbers will be to the left of last_ack_sent,
	 * but they will eventually catch up.
	 * In any case, it never made sense to trim reset segments to
	 * fit the receive window since RFC 1122 says:
	 *   4.2.2.12  RST Segment: RFC-793 Section 3.4
	 *
	 *    A TCP SHOULD allow a received RST segment to include data.
	 *
	 *    DISCUSSION
	 *         It has been suggested that a RST segment could contain
	 *         ASCII text that encoded and explained the cause of the
	 *         RST.  No standard has yet been established for such
	 *         data.
	 *
	 * If the reset segment passes the sequence number test examine
	 * the state:
	 *    SYN_RECEIVED STATE:
	 *	If passive open, return to LISTEN state.
	 *	If active open, inform user that connection was refused.
	 *    ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT STATES:
	 *	Inform user that connection was reset, and close tcb.
	 *    CLOSING, LAST_ACK STATES:
	 *	Close the tcb.
	 *    TIME_WAIT STATE:
	 *	Drop the segment - see Stevens, vol. 2, p. 964 and
	 *      RFC 1337.
	 */
	if (thflags & TH_RST) {
		if (SEQ_GEQ(th->th_seq, tp->last_ack_sent - 1) &&
		    SEQ_LEQ(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) {
			switch (tp->t_state) {

			case TCPS_SYN_RECEIVED:
				so->so_error = ECONNREFUSED;
				goto close;

			case TCPS_ESTABLISHED:
				if (V_tcp_insecure_rst == 0 &&
				    !(SEQ_GEQ(th->th_seq, tp->rcv_nxt - 1) &&
				    SEQ_LEQ(th->th_seq, tp->rcv_nxt + 1)) &&
				    !(SEQ_GEQ(th->th_seq, tp->last_ack_sent - 1) &&
				    SEQ_LEQ(th->th_seq, tp->last_ack_sent + 1))) {
					TCPSTAT_INC(tcps_badrst);
					goto drop;
				}
				/* FALLTHROUGH */
			case TCPS_FIN_WAIT_1:
			case TCPS_FIN_WAIT_2:
			case TCPS_CLOSE_WAIT:
				so->so_error = ECONNRESET;
			close:
				KASSERT(ti_locked == TI_WLOCKED,
				    ("tcp_do_segment: TH_RST 1 ti_locked %d",
				    ti_locked));
				INP_INFO_WLOCK_ASSERT(&V_tcbinfo);

				tp->t_state = TCPS_CLOSED;
				TCPSTAT_INC(tcps_drops);
				tp = tcp_close(tp);
				break;

			case TCPS_CLOSING:
			case TCPS_LAST_ACK:
				KASSERT(ti_locked == TI_WLOCKED,
				    ("tcp_do_segment: TH_RST 2 ti_locked %d",
				    ti_locked));
				INP_INFO_WLOCK_ASSERT(&V_tcbinfo);

				tp = tcp_close(tp);
				break;
			}
		}
		goto drop;
	}

	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if ((to.to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to.to_tsval, tp->ts_recent)) {

		/* Check to see if ts_recent is over 24 days old.  */
		if (ticks - tp->ts_recent_age > TCP_PAWS_IDLE) {
			/*
			 * Invalidate ts_recent.  If this segment updates
			 * ts_recent, the age will be reset later and ts_recent
			 * will get a valid value.  If it does not, setting
			 * ts_recent to zero will at least satisfy the
			 * requirement that zero be placed in the timestamp
			 * echo reply when ts_recent isn't valid.  The
			 * age isn't reset until we get a valid ts_recent
			 * because we don't want out-of-order segments to be
			 * dropped when ts_recent is old.
			 */
			tp->ts_recent = 0;
		} else {
			TCPSTAT_INC(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, tlen);
			TCPSTAT_INC(tcps_pawsdrop);
			if (tlen)
				goto dropafterack;
			goto drop;
		}
	}

	/*
	 * In the SYN-RECEIVED state, validate that the packet belongs to
	 * this connection before trimming the data to fit the receive
	 * window.  Check the sequence number versus IRS since we know
	 * the sequence numbers haven't wrapped.  This is a partial fix
	 * for the "LAND" DoS attack.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && SEQ_LT(th->th_seq, tp->irs)) {
		rstreason = BANDLIM_RST_OPENPORT;
		goto dropwithreset;
	}

	todrop = tp->rcv_nxt - th->th_seq;
	if (todrop > 0) {
		/*
		 * If this is a duplicate SYN for our current connection,
		 * advance over it and pretend and it's not a SYN.
		 */
		if (thflags & TH_SYN && th->th_seq == tp->irs) {
			thflags &= ~TH_SYN;
			th->th_seq++;
			if (th->th_urp > 1)
				th->th_urp--;
			else
				thflags &= ~TH_URG;
			todrop--;
		}
		/*
		 * Following if statement from Stevens, vol. 2, p. 960.
		 */
		if (todrop > tlen
		    || (todrop == tlen && (thflags & TH_FIN) == 0)) {
			/*
			 * Any valid FIN must be to the left of the window.
			 * At this point the FIN must be a duplicate or out
			 * of sequence; drop it.
			 */
			thflags &= ~TH_FIN;

			/*
			 * Send an ACK to resynchronize and drop any data.
			 * But keep on processing for RST or ACK.
			 */
			tp->t_flags |= TF_ACKNOW;
			todrop = tlen;
			TCPSTAT_INC(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, todrop);
		} else {
			TCPSTAT_INC(tcps_rcvpartduppack);
			TCPSTAT_ADD(tcps_rcvpartdupbyte, todrop);
		}
		drop_hdrlen += todrop;	/* drop from the top afterwards */
		th->th_seq += todrop;
		tlen -= todrop;
		if (th->th_urp > todrop)
			th->th_urp -= todrop;
		else {
			thflags &= ~TH_URG;
			th->th_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the
	 * user processes are gone, then RST the other end.
	 */
	if ((so->so_state & SS_NOFDREF) &&
	    tp->t_state > TCPS_CLOSE_WAIT && tlen) {
		char *s;

		KASSERT(ti_locked == TI_WLOCKED, ("%s: SS_NOFDEREF && "
		    "CLOSE_WAIT && tlen ti_locked %d", __func__, ti_locked));
		INP_INFO_WLOCK_ASSERT(&V_tcbinfo);

		if ((s = tcp_log_addrs(&tp->t_inpcb->inp_inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: %s: Received %d bytes of data after socket "
			    "was closed, sending RST and removing tcpcb\n",
			    s, __func__, tcpstates[tp->t_state], tlen);
			free(s, M_TCPLOG);
		}
		tp = tcp_close(tp);
		TCPSTAT_INC(tcps_rcvafterclose);
		rstreason = BANDLIM_UNLIMITED;
		goto dropwithreset;
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (th->th_seq + tlen) - (tp->rcv_nxt + tp->rcv_wnd);
	if (todrop > 0) {
		TCPSTAT_INC(tcps_rcvpackafterwin);
		if (todrop >= tlen) {
			TCPSTAT_ADD(tcps_rcvbyteafterwin, tlen);
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				TCPSTAT_INC(tcps_rcvwinprobe);
			} else
				goto dropafterack;
		} else
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		m_adj(m, -todrop);
		tlen -= todrop;
		thflags &= ~(TH_PUSH|TH_FIN);
	}

	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record its timestamp.
	 * NOTE: 
	 * 1) That the test incorporates suggestions from the latest
	 *    proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 * 2) That updating only on newer timestamps interferes with
	 *    our earlier PAWS tests, so this check should be solely
	 *    predicated on the sequence space of this segment.
	 * 3) That we modify the segment boundary check to be 
	 *        Last.ACK.Sent <= SEG.SEQ + SEG.Len  
	 *    instead of RFC1323's
	 *        Last.ACK.Sent < SEG.SEQ + SEG.Len,
	 *    This modified check allows us to overcome RFC1323's
	 *    limitations as described in Stevens TCP/IP Illustrated
	 *    Vol. 2 p.869. In such cases, we can still calculate the
	 *    RTT correctly when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to.to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
		((thflags & (TH_SYN|TH_FIN)) != 0))) {
		tp->ts_recent_age = ticks;
		tp->ts_recent = to.to_tsval;
	}

	/*
	 * If a SYN is in the window, then this is an
	 * error and we send an RST and drop the connection.
	 */
	if (thflags & TH_SYN) {
		KASSERT(ti_locked == TI_WLOCKED,
		    ("tcp_do_segment: TH_SYN ti_locked %d", ti_locked));
		INP_INFO_WLOCK_ASSERT(&V_tcbinfo);

		tp = tcp_drop(tp, ECONNRESET);
		rstreason = BANDLIM_UNLIMITED;
		goto drop;
	}

	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN
	 * flag is on (half-synchronized state), then queue data for
	 * later processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_state == TCPS_SYN_RECEIVED ||
		    (tp->t_flags & TF_NEEDSYN))
			goto step6;
		else if (tp->t_flags & TF_ACKNOW)
			goto dropafterack;
		else
			goto drop;
	}

	/*
	 * Ack processing.
	 */
	switch (tp->t_state) {

	/*
	 * In SYN_RECEIVED state, the ack ACKs our SYN, so enter
	 * ESTABLISHED state and continue processing.
	 * The ACK was checked above.
	 */
	case TCPS_SYN_RECEIVED:

		TCPSTAT_INC(tcps_connects);
		soisconnected(so);
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE|TF_REQ_SCALE)) {
			tp->rcv_scale = tp->request_r_scale;
			tp->snd_wnd = tiwin;
		}
		/*
		 * Make transitions:
		 *      SYN-RECEIVED  -> ESTABLISHED
		 *      SYN-RECEIVED* -> FIN-WAIT-1
		 */
		tp->t_starttime = ticks;
		if (tp->t_flags & TF_NEEDFIN) {
			tp->t_state = TCPS_FIN_WAIT_1;
			tp->t_flags &= ~TF_NEEDFIN;
		} else {
			tp->t_state = TCPS_ESTABLISHED;
			cc_conn_init(tp);
			tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp));
		}
		/*
		 * If segment contains data or ACK, will call tcp_reass()
		 * later; if not, do so now to pass queued data to user.
		 */
		if (tlen == 0 && (thflags & TH_FIN) == 0)
			(void) tcp_reass(tp, (struct tcphdr *)0, 0,
			    (struct mbuf *)0);
		tp->snd_wl1 = th->th_seq - 1;
		/* FALLTHROUGH */

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < th->th_ack <= tp->snd_max
	 * then advance tp->snd_una to th->th_ack and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up to date window information we update our window information.
	 */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
		if (SEQ_GT(th->th_ack, tp->snd_max)) {
			TCPSTAT_INC(tcps_rcvacktoomuch);
			goto dropafterack;
		}
		if ((tp->t_flags & TF_SACK_PERMIT) &&
		    ((to.to_flags & TOF_SACK) ||
		     !TAILQ_EMPTY(&tp->snd_holes)))
			tcp_sack_doack(tp, &to, th->th_ack);

		/* Run HHOOK_TCP_ESTABLISHED_IN helper hooks. */
		hhook_run_tcp_est_in(tp, th, &to);

		if (SEQ_LEQ(th->th_ack, tp->snd_una)) {
			if (tlen == 0 && tiwin == tp->snd_wnd) {
				TCPSTAT_INC(tcps_rcvdupack);
				/*
				 * If we have outstanding data (other than
				 * a window probe), this is a completely
				 * duplicate ack (ie, window info didn't
				 * change), the ack is the biggest we've
				 * seen and we've seen exactly our rexmt
				 * threshhold of them, assume a packet
				 * has been dropped and retransmit it.
				 * Kludge snd_nxt & the congestion
				 * window so we send only this one
				 * packet.
				 *
				 * We know we're losing at the current
				 * window size so do congestion avoidance
				 * (set ssthresh to half the current window
				 * and pull our congestion window back to
				 * the new ssthresh).
				 *
				 * Dup acks mean that packets have left the
				 * network (they're now cached at the receiver)
				 * so bump cwnd by the amount in the receiver
				 * to keep a constant cwnd packets in the
				 * network.
				 *
				 * When using TCP ECN, notify the peer that
				 * we reduced the cwnd.
				 */
				if (!tcp_timer_active(tp, TT_REXMT) ||
				    th->th_ack != tp->snd_una)
					tp->t_dupacks = 0;
				else if (++tp->t_dupacks > tcprexmtthresh ||
				     IN_FASTRECOVERY(tp->t_flags)) {
					cc_ack_received(tp, th, CC_DUPACK);
					if ((tp->t_flags & TF_SACK_PERMIT) &&
					    IN_FASTRECOVERY(tp->t_flags)) {
						int awnd;
						
						/*
						 * Compute the amount of data in flight first.
						 * We can inject new data into the pipe iff 
						 * we have less than 1/2 the original window's
						 * worth of data in flight.
						 */
						awnd = (tp->snd_nxt - tp->snd_fack) +
							tp->sackhint.sack_bytes_rexmit;
						if (awnd < tp->snd_ssthresh) {
							tp->snd_cwnd += tp->t_maxseg;
							if (tp->snd_cwnd > tp->snd_ssthresh)
								tp->snd_cwnd = tp->snd_ssthresh;
						}
					} else
						tp->snd_cwnd += tp->t_maxseg;
					(void) tcp_output(tp);
					goto drop;
				} else if (tp->t_dupacks == tcprexmtthresh) {
					tcp_seq onxt = tp->snd_nxt;

					/*
					 * If we're doing sack, check to
					 * see if we're already in sack
					 * recovery. If we're not doing sack,
					 * check to see if we're in newreno
					 * recovery.
					 */
					if (tp->t_flags & TF_SACK_PERMIT) {
						if (IN_FASTRECOVERY(tp->t_flags)) {
							tp->t_dupacks = 0;
							break;
						}
					} else {
						if (SEQ_LEQ(th->th_ack,
						    tp->snd_recover)) {
							tp->t_dupacks = 0;
							break;
						}
					}
					/* Congestion signal before ack. */
					cc_cong_signal(tp, th, CC_NDUPACK);
					cc_ack_received(tp, th, CC_DUPACK);
					tcp_timer_activate(tp, TT_REXMT, 0);
					tp->t_rtttime = 0;
					if (tp->t_flags & TF_SACK_PERMIT) {
						TCPSTAT_INC(
						    tcps_sack_recovery_episode);
						tp->sack_newdata = tp->snd_nxt;
						tp->snd_cwnd = tp->t_maxseg;
						(void) tcp_output(tp);
						goto drop;
					}
					tp->snd_nxt = th->th_ack;
					tp->snd_cwnd = tp->t_maxseg;
					(void) tcp_output(tp);
					KASSERT(tp->snd_limited <= 2,
					    ("%s: tp->snd_limited too big",
					    __func__));
					tp->snd_cwnd = tp->snd_ssthresh +
					     tp->t_maxseg *
					     (tp->t_dupacks - tp->snd_limited);
					if (SEQ_GT(onxt, tp->snd_nxt))
						tp->snd_nxt = onxt;
					goto drop;
				} else if (V_tcp_do_rfc3042) {
					cc_ack_received(tp, th, CC_DUPACK);
					u_long oldcwnd = tp->snd_cwnd;
					tcp_seq oldsndmax = tp->snd_max;
					u_int sent;

					KASSERT(tp->t_dupacks == 1 ||
					    tp->t_dupacks == 2,
					    ("%s: dupacks not 1 or 2",
					    __func__));
					if (tp->t_dupacks == 1)
						tp->snd_limited = 0;
					tp->snd_cwnd =
					    (tp->snd_nxt - tp->snd_una) +
					    (tp->t_dupacks - tp->snd_limited) *
					    tp->t_maxseg;
					(void) tcp_output(tp);
					sent = tp->snd_max - oldsndmax;
					if (sent > tp->t_maxseg) {
						KASSERT((tp->t_dupacks == 2 &&
						    tp->snd_limited == 0) ||
						   (sent == tp->t_maxseg + 1 &&
						    tp->t_flags & TF_SENTFIN),
						    ("%s: sent too much",
						    __func__));
						tp->snd_limited = 2;
					} else if (sent > 0)
						++tp->snd_limited;
					tp->snd_cwnd = oldcwnd;
					goto drop;
				}
			} else
				tp->t_dupacks = 0;
			break;
		}

		KASSERT(SEQ_GT(th->th_ack, tp->snd_una),
		    ("%s: th_ack <= snd_una", __func__));

		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets, retract it.
		 */
		if (IN_FASTRECOVERY(tp->t_flags)) {
			if (SEQ_LT(th->th_ack, tp->snd_recover)) {
				if (tp->t_flags & TF_SACK_PERMIT)
					tcp_sack_partialack(tp, th);
				else
					tcp_newreno_partial_ack(tp, th);
			} else
				cc_post_recovery(tp, th);
		}
		tp->t_dupacks = 0;
		/*
		 * If we reach this point, ACK is not a duplicate,
		 *     i.e., it ACKs something we sent.
		 */
		if (tp->t_flags & TF_NEEDSYN) {
			/*
			 * T/TCP: Connection was half-synchronized, and our
			 * SYN has been ACK'd (so connection is now fully
			 * synchronized).  Go to non-starred state,
			 * increment snd_una for ACK of SYN, and check if
			 * we can do window scaling.
			 */
			tp->t_flags &= ~TF_NEEDSYN;
			tp->snd_una++;
			/* Do window scaling? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->rcv_scale = tp->request_r_scale;
				/* Send window already scaled. */
			}
		}

process_ACK:
		INP_WLOCK_ASSERT(tp->t_inpcb);

		acked = BYTES_THIS_ACK(tp, th);
		TCPSTAT_INC(tcps_rcvackpack);
		TCPSTAT_ADD(tcps_rcvackbyte, acked);

		/*
		 * If we just performed our first retransmit, and the ACK
		 * arrives within our recovery window, then it was a mistake
		 * to do the retransmit in the first place.  Recover our
		 * original cwnd and ssthresh, and proceed to transmit where
		 * we left off.
		 */
		if (tp->t_rxtshift == 1 && tp->t_flags & TF_PREVVALID &&
		    (int)(ticks - tp->t_badrxtwin) < 0)
			cc_cong_signal(tp, th, CC_RTO_ERR);

		/*
		 * If we have a timestamp reply, update smoothed
		 * round trip time.  If no timestamp is present but
		 * transmit timer is running and timed sequence
		 * number was acked, update smoothed round trip time.
		 * Since we now have an rtt measurement, cancel the
		 * timer backoff (cf., Phil Karn's retransmit alg.).
		 * Recompute the initial retransmit timer.
		 *
		 * Some boxes send broken timestamp replies
		 * during the SYN+ACK phase, ignore
		 * timestamps of 0 or we could calculate a
		 * huge RTT and blow up the retransmit timer.
		 */
		if ((to.to_flags & TOF_TS) != 0 &&
		    to.to_tsecr) {
			if (!tp->t_rttlow || tp->t_rttlow > ticks - to.to_tsecr)
				tp->t_rttlow = ticks - to.to_tsecr;
			tcp_xmit_timer(tp, ticks - to.to_tsecr + 1);
		} else if (tp->t_rtttime && SEQ_GT(th->th_ack, tp->t_rtseq)) {
			if (!tp->t_rttlow || tp->t_rttlow > ticks - tp->t_rtttime)
				tp->t_rttlow = ticks - tp->t_rtttime;
			tcp_xmit_timer(tp, ticks - tp->t_rtttime);
		}

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (th->th_ack == tp->snd_max) {
			tcp_timer_activate(tp, TT_REXMT, 0);
			needoutput = 1;
		} else if (!tcp_timer_active(tp, TT_PERSIST))
			tcp_timer_activate(tp, TT_REXMT, tp->t_rxtcur);

		/*
		 * If no data (only SYN) was ACK'd,
		 *    skip rest of ACK processing.
		 */
		if (acked == 0)
			goto step6;

		/*
		 * Let the congestion control algorithm update congestion
		 * control related information. This typically means increasing
		 * the congestion window.
		 */
		cc_ack_received(tp, th, CC_ACK);

		SOCKBUF_LOCK(&so->so_snd);
		if (acked > so->so_snd.sb_cc) {
			tp->snd_wnd -= so->so_snd.sb_cc;
			sbdrop_locked(&so->so_snd, (int)so->so_snd.sb_cc);
			ourfinisacked = 1;
		} else {
			sbdrop_locked(&so->so_snd, acked);
			tp->snd_wnd -= acked;
			ourfinisacked = 0;
		}
		/* NB: sowwakeup_locked() does an implicit unlock. */
		sowwakeup_locked(so);
		/* Detect una wraparound. */
		if (!IN_RECOVERY(tp->t_flags) &&
		    SEQ_GT(tp->snd_una, tp->snd_recover) &&
		    SEQ_LEQ(th->th_ack, tp->snd_recover))
			tp->snd_recover = th->th_ack - 1;
		/* XXXLAS: Can this be moved up into cc_post_recovery? */
		if (IN_RECOVERY(tp->t_flags) &&
		    SEQ_GEQ(th->th_ack, tp->snd_recover)) {
			EXIT_RECOVERY(tp->t_flags);
		}
		tp->snd_una = th->th_ack;
		if (tp->t_flags & TF_SACK_PERMIT) {
			if (SEQ_GT(tp->snd_una, tp->snd_recover))
				tp->snd_recover = tp->snd_una;
		}
		if (SEQ_LT(tp->snd_nxt, tp->snd_una))
			tp->snd_nxt = tp->snd_una;

		switch (tp->t_state) {

		/*
		 * In FIN_WAIT_1 STATE in addition to the processing
		 * for the ESTABLISHED state if our FIN is now acknowledged
		 * then enter FIN_WAIT_2.
		 */
		case TCPS_FIN_WAIT_1:
			if (ourfinisacked) {
				/*
				 * If we can't receive any more
				 * data, then closing user can proceed.
				 * Starting the timer is contrary to the
				 * specification, but if we don't get a FIN
				 * we'll hang forever.
				 *
				 * XXXjl:
				 * we should release the tp also, and use a
				 * compressed state.
				 */
				if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
					soisdisconnected(so);
					tcp_timer_activate(tp, TT_2MSL,
					    (tcp_fast_finwait2_recycle ?
					    tcp_finwait2_timeout :
					    TP_MAXIDLE(tp)));
				}
				tp->t_state = TCPS_FIN_WAIT_2;
			}
			break;

		/*
		 * In CLOSING STATE in addition to the processing for
		 * the ESTABLISHED state if the ACK acknowledges our FIN
		 * then enter the TIME-WAIT state, otherwise ignore
		 * the segment.
		 */
		case TCPS_CLOSING:
			if (ourfinisacked) {
				INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
				tcp_twstart(tp);
				INP_INFO_WUNLOCK(&V_tcbinfo);
				m_freem(m);
				return;
			}
			break;

		/*
		 * In LAST_ACK, we may still be waiting for data to drain
		 * and/or to be acked, as well as for the ack of our FIN.
		 * If our FIN is now acknowledged, delete the TCB,
		 * enter the closed state and return.
		 */
		case TCPS_LAST_ACK:
			if (ourfinisacked) {
				INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
				tp = tcp_close(tp);
				goto drop;
			}
			break;
		}
	}

step6:
	INP_WLOCK_ASSERT(tp->t_inpcb);

	/*
	 * Update window information.
	 * Don't look at window if no ACK: TAC's send garbage on first SYN.
	 */
	if ((thflags & TH_ACK) &&
	    (SEQ_LT(tp->snd_wl1, th->th_seq) ||
	    (tp->snd_wl1 == th->th_seq && (SEQ_LT(tp->snd_wl2, th->th_ack) ||
	     (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd))))) {
		/* keep track of pure window updates */
		if (tlen == 0 &&
		    tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd)
			TCPSTAT_INC(tcps_rcvwinupd);
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = th->th_seq;
		tp->snd_wl2 = th->th_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		needoutput = 1;
	}

	/*
	 * Process segments with URG.
	 */
	if ((thflags & TH_URG) && th->th_urp &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		/*
		 * This is a kludge, but if we receive and accept
		 * random urgent pointers, we'll crash in
		 * soreceive.  It's hard to imagine someone
		 * actually wanting to send this much urgent data.
		 */
		SOCKBUF_LOCK(&so->so_rcv);
		if (th->th_urp + so->so_rcv.sb_cc > sb_max) {
			th->th_urp = 0;			/* XXX */
			thflags &= ~TH_URG;		/* XXX */
			SOCKBUF_UNLOCK(&so->so_rcv);	/* XXX */
			goto dodata;			/* XXX */
		}
		/*
		 * If this segment advances the known urgent pointer,
		 * then mark the data stream.  This should not happen
		 * in CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT STATES since
		 * a FIN has been received from the remote side.
		 * In these states we ignore the URG.
		 *
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section as the original
		 * spec states (in one of two places).
		 */
		if (SEQ_GT(th->th_seq+th->th_urp, tp->rcv_up)) {
			tp->rcv_up = th->th_seq + th->th_urp;
			so->so_oobmark = so->so_rcv.sb_cc +
			    (tp->rcv_up - tp->rcv_nxt) - 1;
			if (so->so_oobmark == 0)
				so->so_rcv.sb_state |= SBS_RCVATMARK;
			sohasoutofband(so);
			tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		}
		SOCKBUF_UNLOCK(&so->so_rcv);
		/*
		 * Remove out of band data so doesn't get presented to user.
		 * This can happen independent of advancing the URG pointer,
		 * but if two URG's are pending at once, some out-of-band
		 * data may creep in... ick.
		 */
		if (th->th_urp <= (u_long)tlen &&
		    !(so->so_options & SO_OOBINLINE)) {
			/* hdr drop is delayed */
			tcp_pulloutofband(so, th, m, drop_hdrlen);
		}
	} else {
		/*
		 * If no out of band data is expected,
		 * pull receive urgent pointer along
		 * with the receive window.
		 */
		if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
			tp->rcv_up = tp->rcv_nxt;
	}
dodata:							/* XXX */
	INP_WLOCK_ASSERT(tp->t_inpcb);

	/*
	 * Process the segment text, merging it into the TCP sequencing queue,
	 * and arranging for acknowledgment of receipt if necessary.
	 * This process logically involves adjusting tp->rcv_wnd as data
	 * is presented to the user (this happens in tcp_usrreq.c,
	 * case PRU_RCVD).  If a FIN has already been received on this
	 * connection then we just ignore the text.
	 */
	if ((tlen || (thflags & TH_FIN)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		tcp_seq save_start = th->th_seq;
		m_adj(m, drop_hdrlen);	/* delayed header drop */
		/*
		 * Insert segment which includes th into TCP reassembly queue
		 * with control block tp.  Set thflags to whether reassembly now
		 * includes a segment with FIN.  This handles the common case
		 * inline (segment is the next to be received on an established
		 * connection, and the queue is empty), avoiding linkage into
		 * and removal from the queue and repetition of various
		 * conversions.
		 * Set DELACK for segments received in order, but ack
		 * immediately when segments are out of order (so
		 * fast retransmit can work).
		 */
		if (th->th_seq == tp->rcv_nxt &&
		    LIST_EMPTY(&tp->t_segq) &&
		    TCPS_HAVEESTABLISHED(tp->t_state)) {
			if (DELAY_ACK(tp))
				tp->t_flags |= TF_DELACK;
			else
				tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt += tlen;
			thflags = th->th_flags & TH_FIN;
			TCPSTAT_INC(tcps_rcvpack);
			TCPSTAT_ADD(tcps_rcvbyte, tlen);
			ND6_HINT(tp);
			SOCKBUF_LOCK(&so->so_rcv);
			if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
				m_freem(m);
			else
				sbappendstream_locked(&so->so_rcv, m);
			/* NB: sorwakeup_locked() does an implicit unlock. */
			sorwakeup_locked(so);
		} else {
			/*
			 * XXX: Due to the header drop above "th" is
			 * theoretically invalid by now.  Fortunately
			 * m_adj() doesn't actually frees any mbufs
			 * when trimming from the head.
			 */
			thflags = tcp_reass(tp, th, &tlen, m);
			tp->t_flags |= TF_ACKNOW;
		}
		if (tlen > 0 && (tp->t_flags & TF_SACK_PERMIT))
			tcp_update_sack_list(tp, save_start, save_start + tlen);
#if 0
		/*
		 * Note the amount of data that peer has sent into
		 * our window, in order to estimate the sender's
		 * buffer size.
		 * XXX: Unused.
		 */
		if (SEQ_GT(tp->rcv_adv, tp->rcv_nxt))
			len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
		else
			len = so->so_rcv.sb_hiwat;
#endif
	} else {
		m_freem(m);
		thflags &= ~TH_FIN;
	}

	/*
	 * If FIN is received ACK the FIN and let the user know
	 * that the connection is closing.
	 */
	if (thflags & TH_FIN) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
			socantrcvmore(so);
			/*
			 * If connection is half-synchronized
			 * (ie NEEDSYN flag on) then delay ACK,
			 * so it may be piggybacked when SYN is sent.
			 * Otherwise, since we received a FIN then no
			 * more input can be expected, send ACK now.
			 */
			if (tp->t_flags & TF_NEEDSYN)
				tp->t_flags |= TF_DELACK;
			else
				tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt++;
		}
		switch (tp->t_state) {

		/*
		 * In SYN_RECEIVED and ESTABLISHED STATES
		 * enter the CLOSE_WAIT state.
		 */
		case TCPS_SYN_RECEIVED:
			tp->t_starttime = ticks;
			/* FALLTHROUGH */
		case TCPS_ESTABLISHED:
			tp->t_state = TCPS_CLOSE_WAIT;
			break;

		/*
		 * If still in FIN_WAIT_1 STATE FIN has not been acked so
		 * enter the CLOSING state.
		 */
		case TCPS_FIN_WAIT_1:
			tp->t_state = TCPS_CLOSING;
			break;

		/*
		 * In FIN_WAIT_2 state enter the TIME_WAIT state,
		 * starting the time-wait timer, turning off the other
		 * standard timers.
		 */
		case TCPS_FIN_WAIT_2:
			INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
			KASSERT(ti_locked == TI_WLOCKED, ("%s: dodata "
			    "TCP_FIN_WAIT_2 ti_locked: %d", __func__,
			    ti_locked));

			tcp_twstart(tp);
			INP_INFO_WUNLOCK(&V_tcbinfo);
			return;
		}
	}
	if (ti_locked == TI_WLOCKED)
		INP_INFO_WUNLOCK(&V_tcbinfo);
	ti_locked = TI_UNLOCKED;

#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW))
		(void) tcp_output(tp);

check_delack:
	KASSERT(ti_locked == TI_UNLOCKED, ("%s: check_delack ti_locked %d",
	    __func__, ti_locked));
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(tp->t_inpcb);

	if (tp->t_flags & TF_DELACK) {
		tp->t_flags &= ~TF_DELACK;
		tcp_timer_activate(tp, TT_DELACK, tcp_delacktime);
	}
	INP_WUNLOCK(tp->t_inpcb);
	return;

dropafterack:
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 *
	 * We can now skip the test for the RST flag since all
	 * paths to this code happen after packets containing
	 * RST have been dropped.
	 *
	 * In the SYN-RECEIVED state, don't send an ACK unless the
	 * segment we received passes the SYN-RECEIVED ACK test.
	 * If it fails send a RST.  This breaks the loop in the
	 * "LAND" DoS attack, and also prevents an ACK storm
	 * between two listening ports that have been sent forged
	 * SYN segments, each with the source address of the other.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && (thflags & TH_ACK) &&
	    (SEQ_GT(tp->snd_una, th->th_ack) ||
	     SEQ_GT(th->th_ack, tp->snd_max)) ) {
		rstreason = BANDLIM_RST_OPENPORT;
		goto dropwithreset;
	}
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_DROP, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
	if (ti_locked == TI_WLOCKED)
		INP_INFO_WUNLOCK(&V_tcbinfo);
	ti_locked = TI_UNLOCKED;

	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
	INP_WUNLOCK(tp->t_inpcb);
	m_freem(m);
	return;

dropwithreset:
	if (ti_locked == TI_WLOCKED)
		INP_INFO_WUNLOCK(&V_tcbinfo);
	ti_locked = TI_UNLOCKED;

	if (tp != NULL) {
		tcp_dropwithreset(m, th, tp, tlen, rstreason);
		INP_WUNLOCK(tp->t_inpcb);
	} else
		tcp_dropwithreset(m, th, NULL, tlen, rstreason);
	return;

drop:
	if (ti_locked == TI_WLOCKED) {
		INP_INFO_WUNLOCK(&V_tcbinfo);
		ti_locked = TI_UNLOCKED;
	}
#ifdef INVARIANTS
	else
		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
#endif

	/*
	 * Drop space held by incoming segment and return.
	 */
#ifdef TCPDEBUG
	if (tp == NULL || (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_DROP, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
	if (tp != NULL)
		INP_WUNLOCK(tp->t_inpcb);
	m_freem(m);
}

/*
 * Issue RST and make ACK acceptable to originator of segment.
 * The mbuf must still include the original packet header.
 * tp may be NULL.
 */
static void
tcp_dropwithreset(struct mbuf *m, struct tcphdr *th, struct tcpcb *tp,
    int tlen, int rstreason)
{
#ifdef INET
	struct ip *ip;
#endif
#ifdef INET6
	struct ip6_hdr *ip6;
#endif

	if (tp != NULL) {
		INP_WLOCK_ASSERT(tp->t_inpcb);
	}

	/* Don't bother if destination was broadcast/multicast. */
	if ((th->th_flags & TH_RST) || m->m_flags & (M_BCAST|M_MCAST))
		goto drop;
#ifdef INET6
	if (mtod(m, struct ip *)->ip_v == 6) {
		ip6 = mtod(m, struct ip6_hdr *);
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		    IN6_IS_ADDR_MULTICAST(&ip6->ip6_src))
			goto drop;
		/* IPv6 anycast check is done at tcp6_input() */
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		ip = mtod(m, struct ip *);
		if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
		    IN_MULTICAST(ntohl(ip->ip_src.s_addr)) ||
		    ip->ip_src.s_addr == htonl(INADDR_BROADCAST) ||
		    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))
			goto drop;
	}
#endif

	/* Perform bandwidth limiting. */
	if (badport_bandlim(rstreason) < 0)
		goto drop;

	/* tcp_respond consumes the mbuf chain. */
	if (th->th_flags & TH_ACK) {
		tcp_respond(tp, mtod(m, void *), th, m, (tcp_seq)0,
		    th->th_ack, TH_RST);
	} else {
		if (th->th_flags & TH_SYN)
			tlen++;
		tcp_respond(tp, mtod(m, void *), th, m, th->th_seq+tlen,
		    (tcp_seq)0, TH_RST|TH_ACK);
	}
	return;
drop:
	m_freem(m);
}

/*
 * Parse TCP options and place in tcpopt.
 */
static void
tcp_dooptions(struct tcpopt *to, u_char *cp, int cnt, int flags)
{
	int opt, optlen;

	to->to_flags = 0;
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < 2)
				break;
			optlen = cp[1];
			if (optlen < 2 || optlen > cnt)
				break;
		}
		switch (opt) {
		case TCPOPT_MAXSEG:
			if (optlen != TCPOLEN_MAXSEG)
				continue;
			if (!(flags & TO_SYN))
				continue;
			to->to_flags |= TOF_MSS;
			bcopy((char *)cp + 2,
			    (char *)&to->to_mss, sizeof(to->to_mss));
			to->to_mss = ntohs(to->to_mss);
			break;
		case TCPOPT_WINDOW:
			if (optlen != TCPOLEN_WINDOW)
				continue;
			if (!(flags & TO_SYN))
				continue;
			to->to_flags |= TOF_SCALE;
			to->to_wscale = min(cp[2], TCP_MAX_WINSHIFT);
			break;
		case TCPOPT_TIMESTAMP:
			if (optlen != TCPOLEN_TIMESTAMP)
				continue;
			to->to_flags |= TOF_TS;
			bcopy((char *)cp + 2,
			    (char *)&to->to_tsval, sizeof(to->to_tsval));
			to->to_tsval = ntohl(to->to_tsval);
			bcopy((char *)cp + 6,
			    (char *)&to->to_tsecr, sizeof(to->to_tsecr));
			to->to_tsecr = ntohl(to->to_tsecr);
			break;
#ifdef TCP_SIGNATURE
		/*
		 * XXX In order to reply to a host which has set the
		 * TCP_SIGNATURE option in its initial SYN, we have to
		 * record the fact that the option was observed here
		 * for the syncache code to perform the correct response.
		 */
		case TCPOPT_SIGNATURE:
			if (optlen != TCPOLEN_SIGNATURE)
				continue;
			to->to_flags |= TOF_SIGNATURE;
			to->to_signature = cp + 2;
			break;
#endif
		case TCPOPT_SACK_PERMITTED:
			if (optlen != TCPOLEN_SACK_PERMITTED)
				continue;
			if (!(flags & TO_SYN))
				continue;
			if (!V_tcp_do_sack)
				continue;
			to->to_flags |= TOF_SACKPERM;
			break;
		case TCPOPT_SACK:
			if (optlen <= 2 || (optlen - 2) % TCPOLEN_SACK != 0)
				continue;
			if (flags & TO_SYN)
				continue;
			to->to_flags |= TOF_SACK;
			to->to_nsacks = (optlen - 2) / TCPOLEN_SACK;
			to->to_sacks = cp + 2;
			TCPSTAT_INC(tcps_sack_rcv_blocks);
			break;
		default:
			continue;
		}
	}
}

/*
 * Pull out of band byte out of a segment so
 * it doesn't appear in the user's data queue.
 * It is still reflected in the segment length for
 * sequencing purposes.
 */
static void
tcp_pulloutofband(struct socket *so, struct tcphdr *th, struct mbuf *m,
    int off)
{
	int cnt = off + th->th_urp - 1;

	while (cnt >= 0) {
		if (m->m_len > cnt) {
			char *cp = mtod(m, caddr_t) + cnt;
			struct tcpcb *tp = sototcpcb(so);

			INP_WLOCK_ASSERT(tp->t_inpcb);

			tp->t_iobc = *cp;
			tp->t_oobflags |= TCPOOB_HAVEDATA;
			bcopy(cp+1, cp, (unsigned)(m->m_len - cnt - 1));
			m->m_len--;
			if (m->m_flags & M_PKTHDR)
				m->m_pkthdr.len--;
			return;
		}
		cnt -= m->m_len;
		m = m->m_next;
		if (m == NULL)
			break;
	}
	panic("tcp_pulloutofband");
}

/*
 * Collect new round-trip time estimate
 * and update averages and current timeout.
 */
static void
tcp_xmit_timer(struct tcpcb *tp, int rtt)
{
	int delta;

	INP_WLOCK_ASSERT(tp->t_inpcb);

	TCPSTAT_INC(tcps_rttupdated);
	tp->t_rttupdated++;
	if (tp->t_srtt != 0) {
		/*
		 * srtt is stored as fixed point with 5 bits after the
		 * binary point (i.e., scaled by 8).  The following magic
		 * is equivalent to the smoothing algorithm in rfc793 with
		 * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
		 * point).  Adjust rtt to origin 0.
		 */
		delta = ((rtt - 1) << TCP_DELTA_SHIFT)
			- (tp->t_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT));

		if ((tp->t_srtt += delta) <= 0)
			tp->t_srtt = 1;

		/*
		 * We accumulate a smoothed rtt variance (actually, a
		 * smoothed mean difference), then set the retransmit
		 * timer to smoothed rtt + 4 times the smoothed variance.
		 * rttvar is stored as fixed point with 4 bits after the
		 * binary point (scaled by 16).  The following is
		 * equivalent to rfc793 smoothing with an alpha of .75
		 * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
		 * rfc793's wired-in beta.
		 */
		if (delta < 0)
			delta = -delta;
		delta -= tp->t_rttvar >> (TCP_RTTVAR_SHIFT - TCP_DELTA_SHIFT);
		if ((tp->t_rttvar += delta) <= 0)
			tp->t_rttvar = 1;
		if (tp->t_rttbest > tp->t_srtt + tp->t_rttvar)
		    tp->t_rttbest = tp->t_srtt + tp->t_rttvar;
	} else {
		/*
		 * No rtt measurement yet - use the unsmoothed rtt.
		 * Set the variance to half the rtt (so our first
		 * retransmit happens at 3*rtt).
		 */
		tp->t_srtt = rtt << TCP_RTT_SHIFT;
		tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT - 1);
		tp->t_rttbest = tp->t_srtt + tp->t_rttvar;
	}
	tp->t_rtttime = 0;
	tp->t_rxtshift = 0;

	/*
	 * the retransmit should happen at rtt + 4 * rttvar.
	 * Because of the way we do the smoothing, srtt and rttvar
	 * will each average +1/2 tick of bias.  When we compute
	 * the retransmit timer, we want 1/2 tick of rounding and
	 * 1 extra tick because of +-1/2 tick uncertainty in the
	 * firing of the timer.  The bias will give us exactly the
	 * 1.5 tick we need.  But, because the bias is
	 * statistical, we have to test that we don't drop below
	 * the minimum feasible timer (which is 2 ticks).
	 */
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
		      max(tp->t_rttmin, rtt + 2), TCPTV_REXMTMAX);

	/*
	 * We received an ack for a packet that wasn't retransmitted;
	 * it is probably safe to discard any error indications we've
	 * received recently.  This isn't quite right, but close enough
	 * for now (a route might have failed after we sent a segment,
	 * and the return path might not be symmetrical).
	 */
	tp->t_softerror = 0;
}

/*
 * Determine a reasonable value for maxseg size.
 * If the route is known, check route for mtu.
 * If none, use an mss that can be handled on the outgoing
 * interface without forcing IP to fragment; if bigger than
 * an mbuf cluster (MCLBYTES), round down to nearest multiple of MCLBYTES
 * to utilize large mbufs.  If no route is found, route has no mtu,
 * or the destination isn't local, use a default, hopefully conservative
 * size (usually 512 or the default IP max size, but no more than the mtu
 * of the interface), as we can't discover anything about intervening
 * gateways or networks.  We also initialize the congestion/slow start
 * window to be a single segment if the destination isn't local.
 * While looking at the routing entry, we also initialize other path-dependent
 * parameters from pre-set or cached values in the routing entry.
 *
 * Also take into account the space needed for options that we
 * send regularly.  Make maxseg shorter by that amount to assure
 * that we can send maxseg amount of data even when the options
 * are present.  Store the upper limit of the length of options plus
 * data in maxopd.
 *
 * NOTE that this routine is only called when we process an incoming
 * segment, or an ICMP need fragmentation datagram. Outgoing SYN/ACK MSS
 * settings are handled in tcp_mssopt().
 */
void
tcp_mss_update(struct tcpcb *tp, int offer, int mtuoffer,
    struct hc_metrics_lite *metricptr, int *mtuflags)
{
	int mss = 0;
	u_long maxmtu = 0;
	struct inpcb *inp = tp->t_inpcb;
	struct hc_metrics_lite metrics;
	int origoffer;
#ifdef INET6
	int isipv6 = ((inp->inp_vflag & INP_IPV6) != 0) ? 1 : 0;
	size_t min_protoh = isipv6 ?
			    sizeof (struct ip6_hdr) + sizeof (struct tcphdr) :
			    sizeof (struct tcpiphdr);
#else
	const size_t min_protoh = sizeof(struct tcpiphdr);
#endif

	INP_WLOCK_ASSERT(tp->t_inpcb);

	if (mtuoffer != -1) {
		KASSERT(offer == -1, ("%s: conflict", __func__));
		offer = mtuoffer - min_protoh;
	}
	origoffer = offer;

	/* Initialize. */
#ifdef INET6
	if (isipv6) {
		maxmtu = tcp_maxmtu6(&inp->inp_inc, mtuflags);
		tp->t_maxopd = tp->t_maxseg = V_tcp_v6mssdflt;
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		maxmtu = tcp_maxmtu(&inp->inp_inc, mtuflags);
		tp->t_maxopd = tp->t_maxseg = V_tcp_mssdflt;
	}
#endif

	/*
	 * No route to sender, stay with default mss and return.
	 */
	if (maxmtu == 0) {
		/*
		 * In case we return early we need to initialize metrics
		 * to a defined state as tcp_hc_get() would do for us
		 * if there was no cache hit.
		 */
		if (metricptr != NULL)
			bzero(metricptr, sizeof(struct hc_metrics_lite));
		return;
	}

	/* What have we got? */
	switch (offer) {
		case 0:
			/*
			 * Offer == 0 means that there was no MSS on the SYN
			 * segment, in this case we use tcp_mssdflt as
			 * already assigned to t_maxopd above.
			 */
			offer = tp->t_maxopd;
			break;

		case -1:
			/*
			 * Offer == -1 means that we didn't receive SYN yet.
			 */
			/* FALLTHROUGH */

		default:
			/*
			 * Prevent DoS attack with too small MSS. Round up
			 * to at least minmss.
			 */
			offer = max(offer, V_tcp_minmss);
	}

	/*
	 * rmx information is now retrieved from tcp_hostcache.
	 */
	tcp_hc_get(&inp->inp_inc, &metrics);
	if (metricptr != NULL)
		bcopy(&metrics, metricptr, sizeof(struct hc_metrics_lite));

	/*
	 * If there's a discovered mtu int tcp hostcache, use it
	 * else, use the link mtu.
	 */
	if (metrics.rmx_mtu)
		mss = min(metrics.rmx_mtu, maxmtu) - min_protoh;
	else {
#ifdef INET6
		if (isipv6) {
			mss = maxmtu - min_protoh;
			if (!V_path_mtu_discovery &&
			    !in6_localaddr(&inp->in6p_faddr))
				mss = min(mss, V_tcp_v6mssdflt);
		}
#endif
#if defined(INET) && defined(INET6)
		else
#endif
#ifdef INET
		{
			mss = maxmtu - min_protoh;
			if (!V_path_mtu_discovery &&
			    !in_localaddr(inp->inp_faddr))
				mss = min(mss, V_tcp_mssdflt);
		}
#endif
		/*
		 * XXX - The above conditional (mss = maxmtu - min_protoh)
		 * probably violates the TCP spec.
		 * The problem is that, since we don't know the
		 * other end's MSS, we are supposed to use a conservative
		 * default.  But, if we do that, then MTU discovery will
		 * never actually take place, because the conservative
		 * default is much less than the MTUs typically seen
		 * on the Internet today.  For the moment, we'll sweep
		 * this under the carpet.
		 *
		 * The conservative default might not actually be a problem
		 * if the only case this occurs is when sending an initial
		 * SYN with options and data to a host we've never talked
		 * to before.  Then, they will reply with an MSS value which
		 * will get recorded and the new parameters should get
		 * recomputed.  For Further Study.
		 */
	}
	mss = min(mss, offer);

	/*
	 * Sanity check: make sure that maxopd will be large
	 * enough to allow some data on segments even if the
	 * all the option space is used (40bytes).  Otherwise
	 * funny things may happen in tcp_output.
	 */
	mss = max(mss, 64);

	/*
	 * maxopd stores the maximum length of data AND options
	 * in a segment; maxseg is the amount of data in a normal
	 * segment.  We need to store this value (maxopd) apart
	 * from maxseg, because now every segment carries options
	 * and thus we normally have somewhat less data in segments.
	 */
	tp->t_maxopd = mss;

	/*
	 * origoffer==-1 indicates that no segments were received yet.
	 * In this case we just guess.
	 */
	if ((tp->t_flags & (TF_REQ_TSTMP|TF_NOOPT)) == TF_REQ_TSTMP &&
	    (origoffer == -1 ||
	     (tp->t_flags & TF_RCVD_TSTMP) == TF_RCVD_TSTMP))
		mss -= TCPOLEN_TSTAMP_APPA;

#if	(MCLBYTES & (MCLBYTES - 1)) == 0
	if (mss > MCLBYTES)
		mss &= ~(MCLBYTES-1);
#else
	if (mss > MCLBYTES)
		mss = mss / MCLBYTES * MCLBYTES;
#endif
	tp->t_maxseg = mss;
}

void
tcp_mss(struct tcpcb *tp, int offer)
{
	int mss;
	u_long bufsize;
	struct inpcb *inp;
	struct socket *so;
	struct hc_metrics_lite metrics;
	int mtuflags = 0;

	KASSERT(tp != NULL, ("%s: tp == NULL", __func__));
	
	tcp_mss_update(tp, offer, -1, &metrics, &mtuflags);

	mss = tp->t_maxseg;
	inp = tp->t_inpcb;

	/*
	 * If there's a pipesize, change the socket buffer to that size,
	 * don't change if sb_hiwat is different than default (then it
	 * has been changed on purpose with setsockopt).
	 * Make the socket buffers an integral number of mss units;
	 * if the mss is larger than the socket buffer, decrease the mss.
	 */
	so = inp->inp_socket;
	SOCKBUF_LOCK(&so->so_snd);
	if ((so->so_snd.sb_hiwat == tcp_sendspace) && metrics.rmx_sendpipe)
		bufsize = metrics.rmx_sendpipe;
	else
		bufsize = so->so_snd.sb_hiwat;
	if (bufsize < mss)
		mss = bufsize;
	else {
		bufsize = roundup(bufsize, mss);
		if (bufsize > sb_max)
			bufsize = sb_max;
		if (bufsize > so->so_snd.sb_hiwat)
			(void)sbreserve_locked(&so->so_snd, bufsize, so, NULL);
	}
	SOCKBUF_UNLOCK(&so->so_snd);
	tp->t_maxseg = mss;

	SOCKBUF_LOCK(&so->so_rcv);
	if ((so->so_rcv.sb_hiwat == tcp_recvspace) && metrics.rmx_recvpipe)
		bufsize = metrics.rmx_recvpipe;
	else
		bufsize = so->so_rcv.sb_hiwat;
	if (bufsize > mss) {
		bufsize = roundup(bufsize, mss);
		if (bufsize > sb_max)
			bufsize = sb_max;
		if (bufsize > so->so_rcv.sb_hiwat)
			(void)sbreserve_locked(&so->so_rcv, bufsize, so, NULL);
	}
	SOCKBUF_UNLOCK(&so->so_rcv);

	/* Check the interface for TSO capabilities. */
	if (mtuflags & CSUM_TSO)
		tp->t_flags |= TF_TSO;
}

/*
 * Determine the MSS option to send on an outgoing SYN.
 */
int
tcp_mssopt(struct in_conninfo *inc)
{
	int mss = 0;
	u_long maxmtu = 0;
	u_long thcmtu = 0;
	size_t min_protoh;

	KASSERT(inc != NULL, ("tcp_mssopt with NULL in_conninfo pointer"));

#ifdef INET6
	if (inc->inc_flags & INC_ISIPV6) {
		mss = V_tcp_v6mssdflt;
		maxmtu = tcp_maxmtu6(inc, NULL);
		thcmtu = tcp_hc_getmtu(inc); /* IPv4 and IPv6 */
		min_protoh = sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		mss = V_tcp_mssdflt;
		maxmtu = tcp_maxmtu(inc, NULL);
		thcmtu = tcp_hc_getmtu(inc); /* IPv4 and IPv6 */
		min_protoh = sizeof(struct tcpiphdr);
	}
#endif
	if (maxmtu && thcmtu)
		mss = min(maxmtu, thcmtu) - min_protoh;
	else if (maxmtu || thcmtu)
		mss = max(maxmtu, thcmtu) - min_protoh;

	return (mss);
}


/*
 * On a partial ack arrives, force the retransmission of the
 * next unacknowledged segment.  Do not clear tp->t_dupacks.
 * By setting snd_nxt to ti_ack, this forces retransmission timer to
 * be started again.
 */
static void
tcp_newreno_partial_ack(struct tcpcb *tp, struct tcphdr *th)
{
	tcp_seq onxt = tp->snd_nxt;
	u_long  ocwnd = tp->snd_cwnd;

	INP_WLOCK_ASSERT(tp->t_inpcb);

	tcp_timer_activate(tp, TT_REXMT, 0);
	tp->t_rtttime = 0;
	tp->snd_nxt = th->th_ack;
	/*
	 * Set snd_cwnd to one segment beyond acknowledged offset.
	 * (tp->snd_una has not yet been updated when this function is called.)
	 */
	tp->snd_cwnd = tp->t_maxseg + BYTES_THIS_ACK(tp, th);
	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
	tp->snd_cwnd = ocwnd;
	if (SEQ_GT(onxt, tp->snd_nxt))
		tp->snd_nxt = onxt;
	/*
	 * Partial window deflation.  Relies on fact that tp->snd_una
	 * not updated yet.
	 */
	if (tp->snd_cwnd > BYTES_THIS_ACK(tp, th))
		tp->snd_cwnd -= BYTES_THIS_ACK(tp, th);
	else
		tp->snd_cwnd = 0;
	tp->snd_cwnd += tp->t_maxseg;
}
