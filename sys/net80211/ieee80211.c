/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * IEEE 802.11 generic handler
 */
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/kernel.h>
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>

#include <net/bpf.h>

const char *ieee80211_phymode_name[IEEE80211_MODE_MAX] = {
	[IEEE80211_MODE_AUTO]	  = "auto",
	[IEEE80211_MODE_11A]	  = "11a",
	[IEEE80211_MODE_11B]	  = "11b",
	[IEEE80211_MODE_11G]	  = "11g",
	[IEEE80211_MODE_FH]	  = "FH",
	[IEEE80211_MODE_TURBO_A]  = "turboA",
	[IEEE80211_MODE_TURBO_G]  = "turboG",
	[IEEE80211_MODE_STURBO_A] = "sturboA",
	[IEEE80211_MODE_11NA]	  = "11na",
	[IEEE80211_MODE_11NG]	  = "11ng",
};
/* map ieee80211_opmode to the corresponding capability bit */
const int ieee80211_opcap[IEEE80211_OPMODE_MAX] = {
	[IEEE80211_M_IBSS]	= IEEE80211_C_IBSS,
	[IEEE80211_M_WDS]	= IEEE80211_C_WDS,
	[IEEE80211_M_STA]	= IEEE80211_C_STA,
	[IEEE80211_M_AHDEMO]	= IEEE80211_C_AHDEMO,
	[IEEE80211_M_HOSTAP]	= IEEE80211_C_HOSTAP,
	[IEEE80211_M_MONITOR]	= IEEE80211_C_MONITOR,
};

static const uint8_t ieee80211broadcastaddr[IEEE80211_ADDR_LEN] =
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static	void ieee80211_syncflag_locked(struct ieee80211com *ic, int flag);
static	void ieee80211_syncflag_ext_locked(struct ieee80211com *ic, int flag);
static	int ieee80211_media_setup(struct ieee80211com *ic,
		struct ifmedia *media, int caps, int addsta,
		ifm_change_cb_t media_change, ifm_stat_cb_t media_stat);
static	void ieee80211com_media_status(struct ifnet *, struct ifmediareq *);
static	int ieee80211com_media_change(struct ifnet *);
static	int media_status(enum ieee80211_opmode,
		const struct ieee80211_channel *);

MALLOC_DEFINE(M_80211_VAP, "80211vap", "802.11 vap state");

/*
 * Default supported rates for 802.11 operation (in IEEE .5Mb units).
 */
#define	B(r)	((r) | IEEE80211_RATE_BASIC)
static const struct ieee80211_rateset ieee80211_rateset_11a =
	{ 8, { B(12), 18, B(24), 36, B(48), 72, 96, 108 } };
static const struct ieee80211_rateset ieee80211_rateset_half =
	{ 8, { B(6), 9, B(12), 18, B(24), 36, 48, 54 } };
static const struct ieee80211_rateset ieee80211_rateset_quarter =
	{ 8, { B(3), 4, B(6), 9, B(12), 18, 24, 27 } };
static const struct ieee80211_rateset ieee80211_rateset_11b =
	{ 4, { B(2), B(4), B(11), B(22) } };
/* NB: OFDM rates are handled specially based on mode */
static const struct ieee80211_rateset ieee80211_rateset_11g =
	{ 12, { B(2), B(4), B(11), B(22), 12, 18, 24, 36, 48, 72, 96, 108 } };
#undef B

/*
 * Fill in 802.11 available channel set, mark
 * all available channels as active, and pick
 * a default channel if not already specified.
 */
static void
ieee80211_chan_init(struct ieee80211com *ic)
{
#define	DEFAULTRATES(m, def) do { \
	if (isset(ic->ic_modecaps, m) && ic->ic_sup_rates[m].rs_nrates == 0) \
		ic->ic_sup_rates[m] = def; \
} while (0)
	struct ieee80211_channel *c;
	int i;

	KASSERT(0 < ic->ic_nchans && ic->ic_nchans < IEEE80211_CHAN_MAX,
		("invalid number of channels specified: %u", ic->ic_nchans));
	memset(ic->ic_chan_avail, 0, sizeof(ic->ic_chan_avail));
	memset(ic->ic_modecaps, 0, sizeof(ic->ic_modecaps));
	setbit(ic->ic_modecaps, IEEE80211_MODE_AUTO);
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		KASSERT(c->ic_flags != 0, ("channel with no flags"));
		KASSERT(c->ic_ieee < IEEE80211_CHAN_MAX,
			("channel with bogus ieee number %u", c->ic_ieee));
		setbit(ic->ic_chan_avail, c->ic_ieee);
		/*
		 * Identify mode capabilities.
		 */
		if (IEEE80211_IS_CHAN_A(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_11A);
		if (IEEE80211_IS_CHAN_B(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_11B);
		if (IEEE80211_IS_CHAN_ANYG(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_11G);
		if (IEEE80211_IS_CHAN_FHSS(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_FH);
		if (IEEE80211_IS_CHAN_108A(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_TURBO_A);
		if (IEEE80211_IS_CHAN_108G(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_TURBO_G);
		if (IEEE80211_IS_CHAN_ST(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_STURBO_A);
		if (IEEE80211_IS_CHAN_HTA(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_11NA);
		if (IEEE80211_IS_CHAN_HTG(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_11NG);
	}
	/* initialize candidate channels to all available */
	memcpy(ic->ic_chan_active, ic->ic_chan_avail,
		sizeof(ic->ic_chan_avail));

	/* sort channel table to allow lookup optimizations */
	ieee80211_sort_channels(ic->ic_channels, ic->ic_nchans);

	/* invalidate any previous state */
	ic->ic_bsschan = IEEE80211_CHAN_ANYC;
	ic->ic_prevchan = NULL;
	ic->ic_csa_newchan = NULL;
	/* arbitrarily pick the first channel */
	ic->ic_curchan = &ic->ic_channels[0];

	/* fillin well-known rate sets if driver has not specified */
	DEFAULTRATES(IEEE80211_MODE_11B,	 ieee80211_rateset_11b);
	DEFAULTRATES(IEEE80211_MODE_11G,	 ieee80211_rateset_11g);
	DEFAULTRATES(IEEE80211_MODE_11A,	 ieee80211_rateset_11a);
	DEFAULTRATES(IEEE80211_MODE_TURBO_A,	 ieee80211_rateset_11a);
	DEFAULTRATES(IEEE80211_MODE_TURBO_G,	 ieee80211_rateset_11g);

	/*
	 * Set auto mode to reset active channel state and any desired channel.
	 */
	(void) ieee80211_setmode(ic, IEEE80211_MODE_AUTO);
#undef DEFAULTRATES
}

static void
null_update_mcast(struct ifnet *ifp)
{
	if_printf(ifp, "need multicast update callback\n");
}

static void
null_update_promisc(struct ifnet *ifp)
{
	if_printf(ifp, "need promiscuous mode update callback\n");
}

static int
null_output(struct ifnet *ifp, struct mbuf *m,
	struct sockaddr *dst, struct rtentry *rt0)
{
	if_printf(ifp, "discard raw packet\n");
	m_freem(m);
	return EIO;
}

static void
null_input(struct ifnet *ifp, struct mbuf *m)
{
	if_printf(ifp, "if_input should not be called\n");
	m_freem(m);
}

/*
 * Attach/setup the common net80211 state.  Called by
 * the driver on attach to prior to creating any vap's.
 */
void
ieee80211_ifattach(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct sockaddr_dl *sdl;
	struct ifaddr *ifa;

	KASSERT(ifp->if_type == IFT_IEEE80211, ("if_type %d", ifp->if_type));

	IEEE80211_LOCK_INIT(ic, "ieee80211com");
	TAILQ_INIT(&ic->ic_vaps);
	/*
	 * Fill in 802.11 available channel set, mark all
	 * available channels as active, and pick a default
	 * channel if not already specified.
	 */
	ieee80211_media_init(ic);

	ic->ic_update_mcast = null_update_mcast;
	ic->ic_update_promisc = null_update_promisc;

	ic->ic_bintval = IEEE80211_BINTVAL_DEFAULT;
	ic->ic_lintval = ic->ic_bintval;
	ic->ic_txpowlimit = IEEE80211_TXPOWER_MAX;

	ieee80211_crypto_attach(ic);
	ieee80211_node_attach(ic);
	ieee80211_power_attach(ic);
	ieee80211_proto_attach(ic);
	ieee80211_ht_attach(ic);
	ieee80211_scan_attach(ic);
	ieee80211_regdomain_attach(ic);

	ieee80211_sysctl_attach(ic);

	ifp->if_addrlen = IEEE80211_ADDR_LEN;
	ifp->if_hdrlen = 0;
	if_attach(ifp);
	ifp->if_mtu = IEEE80211_MTU_MAX;
	ifp->if_broadcastaddr = ieee80211broadcastaddr;
	ifp->if_output = null_output;
	ifp->if_input = null_input;	/* just in case */
	ifp->if_resolvemulti = NULL;	/* NB: callers check */

	ifa = ifaddr_byindex(ifp->if_index);
	KASSERT(ifa != NULL, ("%s: no lladdr!\n", __func__));
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;		/* XXX IFT_IEEE80211? */
	sdl->sdl_alen = IEEE80211_ADDR_LEN;
	IEEE80211_ADDR_COPY(LLADDR(sdl), ic->ic_myaddr);
}

/*
 * Detach net80211 state on device detach.  Tear down
 * all vap's and reclaim all common state prior to the
 * device state going away.  Note we may call back into
 * driver; it must be prepared for this.
 */
void
ieee80211_ifdetach(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211vap *vap;

	/* XXX ieee80211_stop_all? */
	while ((vap = TAILQ_FIRST(&ic->ic_vaps)) != NULL)
		ieee80211_vap_destroy(vap);

	ieee80211_sysctl_detach(ic);
	ieee80211_regdomain_detach(ic);
	ieee80211_scan_detach(ic);
	ieee80211_ht_detach(ic);
	/* NB: must be called before ieee80211_node_detach */
	ieee80211_proto_detach(ic);
	ieee80211_crypto_detach(ic);
	ieee80211_power_detach(ic);
	ieee80211_node_detach(ic);
	ifmedia_removeall(&ic->ic_media);

	IEEE80211_LOCK_DESTROY(ic);
	if_detach(ifp);
}

/*
 * Default reset method for use with the ioctl support.  This
 * method is invoked after any state change in the 802.11
 * layer that should be propagated to the hardware but not
 * require re-initialization of the 802.11 state machine (e.g
 * rescanning for an ap).  We always return ENETRESET which
 * should cause the driver to re-initialize the device. Drivers
 * can override this method to implement more optimized support.
 */
static int
default_reset(struct ieee80211vap *vap, u_long cmd)
{
	return ENETRESET;
}

/*
 * Prepare a vap for use.  Drivers use this call to
 * setup net80211 state in new vap's prior attaching
 * them with ieee80211_vap_attach (below).
 */
int
ieee80211_vap_setup(struct ieee80211com *ic, struct ieee80211vap *vap,
	const char name[IFNAMSIZ], int unit, int opmode, int flags,
	const uint8_t bssid[IEEE80211_ADDR_LEN],
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct ifnet *ifp;

	ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		if_printf(ic->ic_ifp, "%s: unable to allocate ifnet\n",
		    __func__);
		return ENOMEM;
	}
	if_initname(ifp, name, unit);
	ifp->if_softc = vap;			/* back pointer */
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;
	ifp->if_start = ieee80211_start;
	ifp->if_ioctl = ieee80211_ioctl;
	ifp->if_watchdog = NULL;		/* NB: no watchdog routine */
	ifp->if_init = ieee80211_init;
	/* NB: input+output filled in by ether_ifattach */
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	vap->iv_ifp = ifp;
	vap->iv_ic = ic;
	vap->iv_flags = ic->ic_flags;		/* propagate common flags */
	vap->iv_flags_ext = ic->ic_flags_ext;
	vap->iv_flags_ven = ic->ic_flags_ven;
	vap->iv_caps = ic->ic_caps &~ IEEE80211_C_OPMODE;
	vap->iv_htcaps = ic->ic_htcaps;
	vap->iv_opmode = opmode;
	vap->iv_caps |= ieee80211_opcap[opmode];
	switch (opmode) {
	case IEEE80211_M_STA:
		/* auto-enable s/w beacon miss support */
		if (flags & IEEE80211_CLONE_NOBEACONS)
			vap->iv_flags_ext |= IEEE80211_FEXT_SWBMISS;
		break;
	case IEEE80211_M_WDS:
		/*
		 * WDS links must specify the bssid of the far end.
		 * For legacy operation this is a static relationship.
		 * For non-legacy operation the station must associate
		 * and be authorized to pass traffic.  Plumbing the
		 * vap to the proper node happens when the vap
		 * transitions to RUN state.
		 */
		IEEE80211_ADDR_COPY(vap->iv_des_bssid, bssid);
		vap->iv_flags |= IEEE80211_F_DESBSSID;
		if (flags & IEEE80211_CLONE_WDSLEGACY)
			vap->iv_flags_ext |= IEEE80211_FEXT_WDSLEGACY;
		break;
	}
	/*
	 * Enable various functionality by default if we're
	 * capable; the driver can override us if it knows better.
	 */
	if (vap->iv_caps & IEEE80211_C_WME)
		vap->iv_flags |= IEEE80211_F_WME;
	if (vap->iv_caps & IEEE80211_C_BURST)
		vap->iv_flags |= IEEE80211_F_BURST;
	if (vap->iv_caps & IEEE80211_C_FF)
		vap->iv_flags |= IEEE80211_F_FF;
	if (vap->iv_caps & IEEE80211_C_TURBOP)
		vap->iv_flags |= IEEE80211_F_TURBOP;
	/* NB: bg scanning only makes sense for station mode right now */
	if (vap->iv_opmode == IEEE80211_M_STA &&
	    (vap->iv_caps & IEEE80211_C_BGSCAN))
		vap->iv_flags |= IEEE80211_F_BGSCAN;
	vap->iv_flags |= IEEE80211_F_DOTH;	/* XXX no cap, just ena */
	/* NB: DFS support only makes sense for ap mode right now */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP &&
	    (vap->iv_caps & IEEE80211_C_DFS))
		vap->iv_flags_ext |= IEEE80211_FEXT_DFS;

	vap->iv_des_chan = IEEE80211_CHAN_ANYC;		/* any channel is ok */
	vap->iv_bmissthreshold = IEEE80211_HWBMISS_DEFAULT;
	vap->iv_dtim_period = IEEE80211_DTIM_DEFAULT;
	/*
	 * Install a default reset method for the ioctl support;
	 * the driver can override this.
	 */
	vap->iv_reset = default_reset;

	IEEE80211_ADDR_COPY(vap->iv_myaddr, macaddr);

	ieee80211_sysctl_vattach(vap);
	ieee80211_crypto_vattach(vap);
	ieee80211_node_vattach(vap);
	ieee80211_power_vattach(vap);
	ieee80211_proto_vattach(vap);
	ieee80211_ht_vattach(vap);
	ieee80211_scan_vattach(vap);
	ieee80211_regdomain_vattach(vap);

	return 0;
}

/*
 * Activate a vap.  State should have been prepared with a
 * call to ieee80211_vap_setup and by the driver.  On return
 * from this call the vap is ready for use.
 */
int
ieee80211_vap_attach(struct ieee80211vap *vap,
	ifm_change_cb_t media_change, ifm_stat_cb_t media_stat)
{
	struct ifnet *ifp = vap->iv_ifp;
	struct ieee80211com *ic = vap->iv_ic;
	struct ifmediareq imr;
	int maxrate;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
	    "%s: %s parent %s flags 0x%x flags_ext 0x%x\n",
	    __func__, ieee80211_opmode_name[vap->iv_opmode],
	    ic->ic_ifp->if_xname, vap->iv_flags, vap->iv_flags_ext);

	/*
	 * Do late attach work that cannot happen until after
	 * the driver has had a chance to override defaults.
	 */
	ieee80211_node_latevattach(vap);
	ieee80211_power_latevattach(vap);

	maxrate = ieee80211_media_setup(ic, &vap->iv_media, vap->iv_caps,
	    vap->iv_opmode == IEEE80211_M_STA, media_change, media_stat);
	ieee80211_media_status(ifp, &imr);
	/* NB: strip explicit mode; we're actually in autoselect */
	ifmedia_set(&vap->iv_media, imr.ifm_active &~ IFM_MMASK);
	if (maxrate)
		ifp->if_baudrate = IF_Mbps(maxrate);

	ether_ifattach(ifp, vap->iv_myaddr);
	/* hook output method setup by ether_ifattach */
	vap->iv_output = ifp->if_output;
	ifp->if_output = ieee80211_output;
	/* NB: if_mtu set by ether_ifattach to ETHERMTU */
	bpfattach2(ifp, DLT_IEEE802_11, ifp->if_hdrlen, &vap->iv_rawbpf);

	IEEE80211_LOCK(ic);
	TAILQ_INSERT_TAIL(&ic->ic_vaps, vap, iv_next);
	ieee80211_syncflag_locked(ic, IEEE80211_F_WME);
	ieee80211_syncflag_locked(ic, IEEE80211_F_TURBOP);
	ieee80211_syncflag_locked(ic, IEEE80211_F_PCF);
	ieee80211_syncflag_locked(ic, IEEE80211_F_BURST);
	ieee80211_syncflag_ext_locked(ic, IEEE80211_FEXT_HT);
	ieee80211_syncflag_ext_locked(ic, IEEE80211_FEXT_USEHT40);
	ieee80211_syncifflag_locked(ic, IFF_PROMISC);
	ieee80211_syncifflag_locked(ic, IFF_ALLMULTI);
	IEEE80211_UNLOCK(ic);

	return 1;
}

/* 
 * Tear down vap state and reclaim the ifnet.
 * The driver is assumed to have prepared for
 * this; e.g. by turning off interrupts for the
 * underlying device.
 */
void
ieee80211_vap_detach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ifnet *ifp = vap->iv_ifp;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE, "%s: %s parent %s\n",
	    __func__, ieee80211_opmode_name[vap->iv_opmode],
	    ic->ic_ifp->if_xname);

	IEEE80211_LOCK(ic);
	/* block traffic from above */
	ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	/*
	 * Evil hack.  Clear the backpointer from the ifnet to the
	 * vap so any requests from above will return an error or
	 * be ignored.  In particular this short-circuits requests
	 * by the bridge to turn off promiscuous mode as a result
	 * of calling ether_ifdetach.
	 */
	ifp->if_softc = NULL;
	/*
	 * Stop the vap before detaching the ifnet.  Ideally we'd
	 * do this in the other order so the ifnet is inaccessible
	 * while we cleanup internal state but that is hard.
	 */
	ieee80211_stop_locked(vap);

	/* XXX accumulate iv_stats in ic_stats? */
	TAILQ_REMOVE(&ic->ic_vaps, vap, iv_next);
	ieee80211_syncflag_locked(ic, IEEE80211_F_WME);
	ieee80211_syncflag_locked(ic, IEEE80211_F_TURBOP);
	ieee80211_syncflag_locked(ic, IEEE80211_F_PCF);
	ieee80211_syncflag_locked(ic, IEEE80211_F_BURST);
	ieee80211_syncflag_ext_locked(ic, IEEE80211_FEXT_HT);
	ieee80211_syncflag_ext_locked(ic, IEEE80211_FEXT_USEHT40);
	ieee80211_syncifflag_locked(ic, IFF_PROMISC);
	ieee80211_syncifflag_locked(ic, IFF_ALLMULTI);
	IEEE80211_UNLOCK(ic);

	/* XXX can't hold com lock */
	/* NB: bpfattach is called by ether_ifdetach and claims all taps */
	ether_ifdetach(ifp);

	ifmedia_removeall(&vap->iv_media);

	ieee80211_regdomain_vdetach(vap);
	ieee80211_scan_vdetach(vap);
	ieee80211_ht_vdetach(vap);
	/* NB: must be before ieee80211_node_vdetach */
	ieee80211_proto_vdetach(vap);
	ieee80211_crypto_vdetach(vap);
	ieee80211_power_vdetach(vap);
	ieee80211_node_vdetach(vap);
	ieee80211_sysctl_vdetach(vap);
}

/*
 * Synchronize flag bit state in the parent ifnet structure
 * according to the state of all vap ifnet's.  This is used,
 * for example, to handle IFF_PROMISC and IFF_ALLMULTI.
 */
void
ieee80211_syncifflag_locked(struct ieee80211com *ic, int flag)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211vap *vap;
	int bit, oflags;

	IEEE80211_LOCK_ASSERT(ic);

	bit = 0;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
		if (vap->iv_ifp->if_flags & flag) {
			/*
			 * XXX the bridge sets PROMISC but we don't want to
			 * enable it on the device, discard here so all the
			 * drivers don't need to special-case it
			 */
			if (flag == IFF_PROMISC &&
			    vap->iv_opmode == IEEE80211_M_HOSTAP)
				continue;
			bit = 1;
			break;
		}
	oflags = ifp->if_flags;
	if (bit)
		ifp->if_flags |= flag;
	else
		ifp->if_flags &= ~flag;
	if ((ifp->if_flags ^ oflags) & flag) {
		/* XXX should we return 1/0 and let caller do this? */
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			if (flag == IFF_PROMISC)
				ic->ic_update_promisc(ifp);
			else if (flag == IFF_ALLMULTI)
				ic->ic_update_mcast(ifp);
		}
	}
}

/*
 * Synchronize flag bit state in the com structure
 * according to the state of all vap's.  This is used,
 * for example, to handle state changes via ioctls.
 */
static void
ieee80211_syncflag_locked(struct ieee80211com *ic, int flag)
{
	struct ieee80211vap *vap;
	int bit;

	IEEE80211_LOCK_ASSERT(ic);

	bit = 0;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
		if (vap->iv_flags & flag) {
			bit = 1;
			break;
		}
	if (bit)
		ic->ic_flags |= flag;
	else
		ic->ic_flags &= ~flag;
}

void
ieee80211_syncflag(struct ieee80211vap *vap, int flag)
{
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK(ic);
	if (flag < 0) {
		flag = -flag;
		vap->iv_flags &= ~flag;
	} else
		vap->iv_flags |= flag;
	ieee80211_syncflag_locked(ic, flag);
	IEEE80211_UNLOCK(ic);
}

/*
 * Synchronize flag bit state in the com structure
 * according to the state of all vap's.  This is used,
 * for example, to handle state changes via ioctls.
 */
static void
ieee80211_syncflag_ext_locked(struct ieee80211com *ic, int flag)
{
	struct ieee80211vap *vap;
	int bit;

	IEEE80211_LOCK_ASSERT(ic);

	bit = 0;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
		if (vap->iv_flags_ext & flag) {
			bit = 1;
			break;
		}
	if (bit)
		ic->ic_flags_ext |= flag;
	else
		ic->ic_flags_ext &= ~flag;
}

void
ieee80211_syncflag_ext(struct ieee80211vap *vap, int flag)
{
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK(ic);
	if (flag < 0) {
		flag = -flag;
		vap->iv_flags_ext &= ~flag;
	} else
		vap->iv_flags_ext |= flag;
	ieee80211_syncflag_ext_locked(ic, flag);
	IEEE80211_UNLOCK(ic);
}

static __inline int
mapgsm(u_int freq, u_int flags)
{
	freq *= 10;
	if (flags & IEEE80211_CHAN_QUARTER)
		freq += 5;
	else if (flags & IEEE80211_CHAN_HALF)
		freq += 10;
	else
		freq += 20;
	/* NB: there is no 907/20 wide but leave room */
	return (freq - 906*10) / 5;
}

static __inline int
mappsb(u_int freq, u_int flags)
{
	return 37 + ((freq * 10) + ((freq % 5) == 2 ? 5 : 0) - 49400) / 5;
}

/*
 * Convert MHz frequency to IEEE channel number.
 */
int
ieee80211_mhz2ieee(u_int freq, u_int flags)
{
#define	IS_FREQ_IN_PSB(_freq) ((_freq) > 4940 && (_freq) < 4990)
	if (flags & IEEE80211_CHAN_GSM)
		return mapgsm(freq, flags);
	if (flags & IEEE80211_CHAN_2GHZ) {	/* 2GHz band */
		if (freq == 2484)
			return 14;
		if (freq < 2484)
			return ((int) freq - 2407) / 5;
		else
			return 15 + ((freq - 2512) / 20);
	} else if (flags & IEEE80211_CHAN_5GHZ) {	/* 5Ghz band */
		if (freq <= 5000) {
			/* XXX check regdomain? */
			if (IS_FREQ_IN_PSB(freq))
				return mappsb(freq, flags);
			return (freq - 4000) / 5;
		} else
			return (freq - 5000) / 5;
	} else {				/* either, guess */
		if (freq == 2484)
			return 14;
		if (freq < 2484) {
			if (907 <= freq && freq <= 922)
				return mapgsm(freq, flags);
			return ((int) freq - 2407) / 5;
		}
		if (freq < 5000) {
			if (IS_FREQ_IN_PSB(freq))
				return mappsb(freq, flags);
			else if (freq > 4900)
				return (freq - 4000) / 5;
			else
				return 15 + ((freq - 2512) / 20);
		}
		return (freq - 5000) / 5;
	}
#undef IS_FREQ_IN_PSB
}

/*
 * Convert channel to IEEE channel number.
 */
int
ieee80211_chan2ieee(struct ieee80211com *ic, const struct ieee80211_channel *c)
{
	if (c == NULL) {
		if_printf(ic->ic_ifp, "invalid channel (NULL)\n");
		return 0;		/* XXX */
	}
	return (c == IEEE80211_CHAN_ANYC ?  IEEE80211_CHAN_ANY : c->ic_ieee);
}

/*
 * Convert IEEE channel number to MHz frequency.
 */
u_int
ieee80211_ieee2mhz(u_int chan, u_int flags)
{
	if (flags & IEEE80211_CHAN_GSM)
		return 907 + 5 * (chan / 10);
	if (flags & IEEE80211_CHAN_2GHZ) {	/* 2GHz band */
		if (chan == 14)
			return 2484;
		if (chan < 14)
			return 2407 + chan*5;
		else
			return 2512 + ((chan-15)*20);
	} else if (flags & IEEE80211_CHAN_5GHZ) {/* 5Ghz band */
		if (flags & (IEEE80211_CHAN_HALF|IEEE80211_CHAN_QUARTER)) {
			chan -= 37;
			return 4940 + chan*5 + (chan % 5 ? 2 : 0);
		}
		return 5000 + (chan*5);
	} else {				/* either, guess */
		/* XXX can't distinguish PSB+GSM channels */
		if (chan == 14)
			return 2484;
		if (chan < 14)			/* 0-13 */
			return 2407 + chan*5;
		if (chan < 27)			/* 15-26 */
			return 2512 + ((chan-15)*20);
		return 5000 + (chan*5);
	}
}

/*
 * Locate a channel given a frequency+flags.  We cache
 * the previous lookup to optimize switching between two
 * channels--as happens with dynamic turbo.
 */
struct ieee80211_channel *
ieee80211_find_channel(struct ieee80211com *ic, int freq, int flags)
{
	struct ieee80211_channel *c;
	int i;

	flags &= IEEE80211_CHAN_ALLTURBO;
	c = ic->ic_prevchan;
	if (c != NULL && c->ic_freq == freq &&
	    (c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags)
		return c;
	/* brute force search */
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (c->ic_freq == freq &&
		    (c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags)
			return c;
	}
	return NULL;
}

/*
 * Locate a channel given a channel number+flags.  We cache
 * the previous lookup to optimize switching between two
 * channels--as happens with dynamic turbo.
 */
struct ieee80211_channel *
ieee80211_find_channel_byieee(struct ieee80211com *ic, int ieee, int flags)
{
	struct ieee80211_channel *c;
	int i;

	flags &= IEEE80211_CHAN_ALLTURBO;
	c = ic->ic_prevchan;
	if (c != NULL && c->ic_ieee == ieee &&
	    (c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags)
		return c;
	/* brute force search */
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (c->ic_ieee == ieee &&
		    (c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags)
			return c;
	}
	return NULL;
}

static void
addmedia(struct ifmedia *media, int caps, int addsta, int mode, int mword)
{
#define	ADD(_ic, _s, _o) \
	ifmedia_add(media, \
		IFM_MAKEWORD(IFM_IEEE80211, (_s), (_o), 0), 0, NULL)
	static const u_int mopts[IEEE80211_MODE_MAX] = { 
		IFM_AUTO,
		IFM_IEEE80211_11A,
		IFM_IEEE80211_11B,
		IFM_IEEE80211_11G,
		IFM_IEEE80211_FH,
		IFM_IEEE80211_11A | IFM_IEEE80211_TURBO,
		IFM_IEEE80211_11G | IFM_IEEE80211_TURBO,
		IFM_IEEE80211_11A | IFM_IEEE80211_TURBO,
		IFM_IEEE80211_11NA,
		IFM_IEEE80211_11NG,
	};
	u_int mopt;

	mopt = mopts[mode];
	if (addsta)
		ADD(ic, mword, mopt);	/* STA mode has no cap */
	if (caps & IEEE80211_C_IBSS)
		ADD(media, mword, mopt | IFM_IEEE80211_ADHOC);
	if (caps & IEEE80211_C_HOSTAP)
		ADD(media, mword, mopt | IFM_IEEE80211_HOSTAP);
	if (caps & IEEE80211_C_AHDEMO)
		ADD(media, mword, mopt | IFM_IEEE80211_ADHOC | IFM_FLAG0);
	if (caps & IEEE80211_C_MONITOR)
		ADD(media, mword, mopt | IFM_IEEE80211_MONITOR);
	if (caps & IEEE80211_C_WDS)
		ADD(media, mword, mopt | IFM_IEEE80211_WDS);
#undef ADD
}

/*
 * Setup the media data structures according to the channel and
 * rate tables.
 */
static int
ieee80211_media_setup(struct ieee80211com *ic,
	struct ifmedia *media, int caps, int addsta,
	ifm_change_cb_t media_change, ifm_stat_cb_t media_stat)
{
	int i, j, mode, rate, maxrate, mword, r;
	const struct ieee80211_rateset *rs;
	struct ieee80211_rateset allrates;

	/*
	 * Fill in media characteristics.
	 */
	ifmedia_init(media, 0, media_change, media_stat);
	maxrate = 0;
	/*
	 * Add media for legacy operating modes.
	 */
	memset(&allrates, 0, sizeof(allrates));
	for (mode = IEEE80211_MODE_AUTO; mode < IEEE80211_MODE_11NA; mode++) {
		if (isclr(ic->ic_modecaps, mode))
			continue;
		addmedia(media, caps, addsta, mode, IFM_AUTO);
		if (mode == IEEE80211_MODE_AUTO)
			continue;
		rs = &ic->ic_sup_rates[mode];
		for (i = 0; i < rs->rs_nrates; i++) {
			rate = rs->rs_rates[i];
			mword = ieee80211_rate2media(ic, rate, mode);
			if (mword == 0)
				continue;
			addmedia(media, caps, addsta, mode, mword);
			/*
			 * Add legacy rate to the collection of all rates.
			 */
			r = rate & IEEE80211_RATE_VAL;
			for (j = 0; j < allrates.rs_nrates; j++)
				if (allrates.rs_rates[j] == r)
					break;
			if (j == allrates.rs_nrates) {
				/* unique, add to the set */
				allrates.rs_rates[j] = r;
				allrates.rs_nrates++;
			}
			rate = (rate & IEEE80211_RATE_VAL) / 2;
			if (rate > maxrate)
				maxrate = rate;
		}
	}
	for (i = 0; i < allrates.rs_nrates; i++) {
		mword = ieee80211_rate2media(ic, allrates.rs_rates[i],
				IEEE80211_MODE_AUTO);
		if (mword == 0)
			continue;
		/* NB: remove media options from mword */
		addmedia(media, caps, addsta,
		    IEEE80211_MODE_AUTO, IFM_SUBTYPE(mword));
	}
	/*
	 * Add HT/11n media.  Note that we do not have enough
	 * bits in the media subtype to express the MCS so we
	 * use a "placeholder" media subtype and any fixed MCS
	 * must be specified with a different mechanism.
	 */
	for (; mode < IEEE80211_MODE_MAX; mode++) {
		if (isclr(ic->ic_modecaps, mode))
			continue;
		addmedia(media, caps, addsta, mode, IFM_AUTO);
		addmedia(media, caps, addsta, mode, IFM_IEEE80211_MCS);
	}
	if (isset(ic->ic_modecaps, IEEE80211_MODE_11NA) ||
	    isset(ic->ic_modecaps, IEEE80211_MODE_11NG)) {
		addmedia(media, caps, addsta,
		    IEEE80211_MODE_AUTO, IFM_IEEE80211_MCS);
		/* XXX could walk htrates */
		/* XXX known array size */
		if (ieee80211_htrates[15].ht40_rate_400ns > maxrate)
			maxrate = ieee80211_htrates[15].ht40_rate_400ns;
	}
	return maxrate;
}

void
ieee80211_media_init(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	int maxrate;

	/* NB: this works because the structure is initialized to zero */
	if (!LIST_EMPTY(&ic->ic_media.ifm_list)) {
		/*
		 * We are re-initializing the channel list; clear
		 * the existing media state as the media routines
		 * don't suppress duplicates.
		 */
		ifmedia_removeall(&ic->ic_media);
	}
	ieee80211_chan_init(ic);

	/*
	 * Recalculate media settings in case new channel list changes
	 * the set of available modes.
	 */
	maxrate = ieee80211_media_setup(ic, &ic->ic_media, ic->ic_caps, 1,
		ieee80211com_media_change, ieee80211com_media_status);
	/* NB: strip explicit mode; we're actually in autoselect */
	ifmedia_set(&ic->ic_media,
		media_status(ic->ic_opmode, ic->ic_curchan) &~ IFM_MMASK);
	if (maxrate)
		ifp->if_baudrate = IF_Mbps(maxrate);

	/* XXX need to propagate new media settings to vap's */
}

const struct ieee80211_rateset *
ieee80211_get_suprates(struct ieee80211com *ic, const struct ieee80211_channel *c)
{
	if (IEEE80211_IS_CHAN_HALF(c))
		return &ieee80211_rateset_half;
	if (IEEE80211_IS_CHAN_QUARTER(c))
		return &ieee80211_rateset_quarter;
	if (IEEE80211_IS_CHAN_HTA(c))
		return &ic->ic_sup_rates[IEEE80211_MODE_11A];
	if (IEEE80211_IS_CHAN_HTG(c)) {
		/* XXX does this work for basic rates? */
		return &ic->ic_sup_rates[IEEE80211_MODE_11G];
	}
	return &ic->ic_sup_rates[ieee80211_chan2mode(c)];
}

void
ieee80211_announce(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	int i, mode, rate, mword;
	const struct ieee80211_rateset *rs;

	/* NB: skip AUTO since it has no rates */
	for (mode = IEEE80211_MODE_AUTO+1; mode < IEEE80211_MODE_11NA; mode++) {
		if (isclr(ic->ic_modecaps, mode))
			continue;
		if_printf(ifp, "%s rates: ", ieee80211_phymode_name[mode]);
		rs = &ic->ic_sup_rates[mode];
		for (i = 0; i < rs->rs_nrates; i++) {
			mword = ieee80211_rate2media(ic, rs->rs_rates[i], mode);
			if (mword == 0)
				continue;
			rate = ieee80211_media2rate(mword);
			printf("%s%d%sMbps", (i != 0 ? " " : ""),
			    rate / 2, ((rate & 0x1) != 0 ? ".5" : ""));
		}
		printf("\n");
	}
	ieee80211_ht_announce(ic);
}

void
ieee80211_announce_channels(struct ieee80211com *ic)
{
	const struct ieee80211_channel *c;
	char type;
	int i, cw;

	printf("Chan  Freq  CW  RegPwr  MinPwr  MaxPwr\n");
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (IEEE80211_IS_CHAN_ST(c))
			type = 'S';
		else if (IEEE80211_IS_CHAN_108A(c))
			type = 'T';
		else if (IEEE80211_IS_CHAN_108G(c))
			type = 'G';
		else if (IEEE80211_IS_CHAN_HT(c))
			type = 'n';
		else if (IEEE80211_IS_CHAN_A(c))
			type = 'a';
		else if (IEEE80211_IS_CHAN_ANYG(c))
			type = 'g';
		else if (IEEE80211_IS_CHAN_B(c))
			type = 'b';
		else
			type = 'f';
		if (IEEE80211_IS_CHAN_HT40(c) || IEEE80211_IS_CHAN_TURBO(c))
			cw = 40;
		else if (IEEE80211_IS_CHAN_HALF(c))
			cw = 10;
		else if (IEEE80211_IS_CHAN_QUARTER(c))
			cw = 5;
		else
			cw = 20;
		printf("%4d  %4d%c %2d%c %6d  %4d.%d  %4d.%d\n"
			, c->ic_ieee, c->ic_freq, type
			, cw
			, IEEE80211_IS_CHAN_HT40U(c) ? '+' :
			  IEEE80211_IS_CHAN_HT40D(c) ? '-' : ' '
			, c->ic_maxregpower
			, c->ic_minpower / 2, c->ic_minpower & 1 ? 5 : 0
			, c->ic_maxpower / 2, c->ic_maxpower & 1 ? 5 : 0
		);
	}
}

static int
media2mode(const struct ieee80211com *ic,
	const struct ifmedia_entry *ime, enum ieee80211_phymode *mode)
{
	switch (IFM_MODE(ime->ifm_media)) {
	case IFM_IEEE80211_11A:
		*mode = IEEE80211_MODE_11A;
		break;
	case IFM_IEEE80211_11B:
		*mode = IEEE80211_MODE_11B;
		break;
	case IFM_IEEE80211_11G:
		*mode = IEEE80211_MODE_11G;
		break;
	case IFM_IEEE80211_FH:
		*mode = IEEE80211_MODE_FH;
		break;
	case IFM_IEEE80211_11NA:
		*mode = IEEE80211_MODE_11NA;
		break;
	case IFM_IEEE80211_11NG:
		*mode = IEEE80211_MODE_11NG;
		break;
	case IFM_AUTO:
		*mode = IEEE80211_MODE_AUTO;
		break;
	default:
		return 0;
	}
	/*
	 * Turbo mode is an ``option''.
	 * XXX does not apply to AUTO
	 */
	if (ime->ifm_media & IFM_IEEE80211_TURBO) {
		if (*mode == IEEE80211_MODE_11A) {
			if (ic->ic_flags & IEEE80211_F_TURBOP)
				*mode = IEEE80211_MODE_TURBO_A;
			else
				*mode = IEEE80211_MODE_STURBO_A;
		} else if (*mode == IEEE80211_MODE_11G)
			*mode = IEEE80211_MODE_TURBO_G;
		else
			return 0;
	}
	/* XXX HT40 +/- */
	return 1;
}

/*
 * Handle a media change request on the underlying
 * interface; we accept mode changes only.
 */
int
ieee80211com_media_change(struct ifnet *ifp)
{
	struct ieee80211com *ic = ifp->if_l2com;
	struct ifmedia_entry *ime = ic->ic_media.ifm_cur;
	enum ieee80211_phymode newphymode;
	int error = 0;

	/*
	 * First, identify the phy mode.
	 */
	if (!media2mode(ic, ime, &newphymode))
		return EINVAL;
	/* NB: mode must be supported, no need to check */

	/*
	 * Handle phy mode change.
	 */
	IEEE80211_LOCK(ic);
	if (ic->ic_curmode != newphymode) {		/* change phy mode */
		struct ieee80211vap *vap;

		(void) ieee80211_setmode(ic, newphymode);
		/*
		 * Propagate new state to each vap.
		 */
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		}
	}
	IEEE80211_UNLOCK(ic);
	return error;
}

static int
findrate(const struct ieee80211com *ic, enum ieee80211_phymode m, int r)
{
	int i, nrates;

	for (i = 0, nrates = ic->ic_sup_rates[m].rs_nrates; i < nrates; i++)
		if ((ic->ic_sup_rates[m].rs_rates[i] & IEEE80211_RATE_VAL) == r)
			return i;
	return -1;
}

/*
 * Handle a media change request on the vap interface.
 */
int
ieee80211_media_change(struct ifnet *ifp)
{
	struct ieee80211vap *vap = ifp->if_softc;
	struct ifmedia_entry *ime = vap->iv_media.ifm_cur;
	struct ieee80211com *ic = vap->iv_ic;
	int newrate;

	/* XXX this won't work unless ic_curmode is != IEEE80211_MODE_AUTO */
	if (ic->ic_curmode == IEEE80211_MODE_AUTO)
		return EINVAL;
	if (IFM_SUBTYPE(ime->ifm_media) != IFM_AUTO) {
		/*
		 * NB: this can only be used to specify a legacy rate.
		 */
		newrate = ieee80211_media2rate(ime->ifm_media);
		if (newrate == 0)
			return EINVAL;
		if (findrate(ic, ic->ic_curmode, newrate) == -1)
			return EINVAL;
	} else {
		newrate = IEEE80211_FIXED_RATE_NONE;
	}
	if (newrate != vap->iv_txparms[ic->ic_curmode].ucastrate) {
		vap->iv_txparms[ic->ic_curmode].ucastrate = newrate;
		return ENETRESET;
	}
	return 0;
}

/*
 * Common code to calculate the media status word
 * from the operating mode and channel state.
 */
static int
media_status(enum ieee80211_opmode opmode, const struct ieee80211_channel *chan)
{
	int status;

	status = IFM_IEEE80211;
	switch (opmode) {
	case IEEE80211_M_STA:
		break;
	case IEEE80211_M_IBSS:
		status |= IFM_IEEE80211_ADHOC;
		break;
	case IEEE80211_M_HOSTAP:
		status |= IFM_IEEE80211_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		status |= IFM_IEEE80211_MONITOR;
		break;
	case IEEE80211_M_AHDEMO:
		status |= IFM_IEEE80211_ADHOC | IFM_FLAG0;
		break;
	case IEEE80211_M_WDS:
		status |= IFM_IEEE80211_WDS;
		break;
	}
	if (IEEE80211_IS_CHAN_HTA(chan)) {
		status |= IFM_IEEE80211_11NA;
	} else if (IEEE80211_IS_CHAN_HTG(chan)) {
		status |= IFM_IEEE80211_11NG;
	} else if (IEEE80211_IS_CHAN_A(chan)) {
		status |= IFM_IEEE80211_11A;
	} else if (IEEE80211_IS_CHAN_B(chan)) {
		status |= IFM_IEEE80211_11B;
	} else if (IEEE80211_IS_CHAN_ANYG(chan)) {
		status |= IFM_IEEE80211_11G;
	} else if (IEEE80211_IS_CHAN_FHSS(chan)) {
		status |= IFM_IEEE80211_FH;
	}
	/* XXX else complain? */

	if (IEEE80211_IS_CHAN_TURBO(chan))
		status |= IFM_IEEE80211_TURBO;
#if 0
	if (IEEE80211_IS_CHAN_HT20(chan))
		status |= IFM_IEEE80211_HT20;
	if (IEEE80211_IS_CHAN_HT40(chan))
		status |= IFM_IEEE80211_HT40;
#endif
	return status;
}

static void
ieee80211com_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap;

	imr->ifm_status = IFM_AVALID;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
		if (vap->iv_ifp->if_flags & IFF_UP) {
			imr->ifm_status |= IFM_ACTIVE;
			break;
		}
	imr->ifm_active = media_status(ic->ic_opmode, ic->ic_curchan);
	if (imr->ifm_status & IFM_ACTIVE)
		imr->ifm_current = imr->ifm_active;
}

void
ieee80211_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct ieee80211vap *vap = ifp->if_softc;
	struct ieee80211com *ic = vap->iv_ic;
	enum ieee80211_phymode mode;

	imr->ifm_status = IFM_AVALID;
	/*
	 * NB: use the current channel's mode to lock down a xmit
	 * rate only when running; otherwise we may have a mismatch
	 * in which case the rate will not be convertible.
	 */
	if (vap->iv_state == IEEE80211_S_RUN) {
		imr->ifm_status |= IFM_ACTIVE;
		mode = ieee80211_chan2mode(ic->ic_curchan);
	} else
		mode = IEEE80211_MODE_AUTO;
	imr->ifm_active = media_status(vap->iv_opmode, ic->ic_curchan);
	/*
	 * Calculate a current rate if possible.
	 */
	if (vap->iv_txparms[mode].ucastrate != IEEE80211_FIXED_RATE_NONE) {
		/*
		 * A fixed rate is set, report that.
		 */
		imr->ifm_active |= ieee80211_rate2media(ic,
			vap->iv_txparms[mode].ucastrate, mode);
	} else if (vap->iv_opmode == IEEE80211_M_STA) {
		/*
		 * In station mode report the current transmit rate.
		 */
		imr->ifm_active |= ieee80211_rate2media(ic,
			vap->iv_bss->ni_txrate, mode);
	} else
		imr->ifm_active |= IFM_AUTO;
	if (imr->ifm_status & IFM_ACTIVE)
		imr->ifm_current = imr->ifm_active;
}

/*
 * Set the current phy mode and recalculate the active channel
 * set based on the available channels for this mode.  Also
 * select a new default/current channel if the current one is
 * inappropriate for this mode.
 */
int
ieee80211_setmode(struct ieee80211com *ic, enum ieee80211_phymode mode)
{
	/*
	 * Adjust basic rates in 11b/11g supported rate set.
	 * Note that if operating on a hal/quarter rate channel
	 * this is a noop as those rates sets are different
	 * and used instead.
	 */
	if (mode == IEEE80211_MODE_11G || mode == IEEE80211_MODE_11B)
		ieee80211_setbasicrates(&ic->ic_sup_rates[mode], mode);

	ic->ic_curmode = mode;
	ieee80211_reset_erp(ic);	/* reset ERP state */

	return 0;
}

/*
 * Return the phy mode for with the specified channel.
 */
enum ieee80211_phymode
ieee80211_chan2mode(const struct ieee80211_channel *chan)
{

	if (IEEE80211_IS_CHAN_HTA(chan))
		return IEEE80211_MODE_11NA;
	else if (IEEE80211_IS_CHAN_HTG(chan))
		return IEEE80211_MODE_11NG;
	else if (IEEE80211_IS_CHAN_108G(chan))
		return IEEE80211_MODE_TURBO_G;
	else if (IEEE80211_IS_CHAN_ST(chan))
		return IEEE80211_MODE_STURBO_A;
	else if (IEEE80211_IS_CHAN_TURBO(chan))
		return IEEE80211_MODE_TURBO_A;
	else if (IEEE80211_IS_CHAN_A(chan))
		return IEEE80211_MODE_11A;
	else if (IEEE80211_IS_CHAN_ANYG(chan))
		return IEEE80211_MODE_11G;
	else if (IEEE80211_IS_CHAN_B(chan))
		return IEEE80211_MODE_11B;
	else if (IEEE80211_IS_CHAN_FHSS(chan))
		return IEEE80211_MODE_FH;

	/* NB: should not get here */
	printf("%s: cannot map channel to mode; freq %u flags 0x%x\n",
		__func__, chan->ic_freq, chan->ic_flags);
	return IEEE80211_MODE_11B;
}

struct ratemedia {
	u_int	match;	/* rate + mode */
	u_int	media;	/* if_media rate */
};

static int
findmedia(const struct ratemedia rates[], int n, u_int match)
{
	int i;

	for (i = 0; i < n; i++)
		if (rates[i].match == match)
			return rates[i].media;
	return IFM_AUTO;
}

/*
 * Convert IEEE80211 rate value to ifmedia subtype.
 * Rate is either a legacy rate in units of 0.5Mbps
 * or an MCS index.
 */
int
ieee80211_rate2media(struct ieee80211com *ic, int rate, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const struct ratemedia rates[] = {
		{   2 | IFM_IEEE80211_FH, IFM_IEEE80211_FH1 },
		{   4 | IFM_IEEE80211_FH, IFM_IEEE80211_FH2 },
		{   2 | IFM_IEEE80211_11B, IFM_IEEE80211_DS1 },
		{   4 | IFM_IEEE80211_11B, IFM_IEEE80211_DS2 },
		{  11 | IFM_IEEE80211_11B, IFM_IEEE80211_DS5 },
		{  22 | IFM_IEEE80211_11B, IFM_IEEE80211_DS11 },
		{  44 | IFM_IEEE80211_11B, IFM_IEEE80211_DS22 },
		{  12 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM6 },
		{  18 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM9 },
		{  24 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM12 },
		{  36 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM18 },
		{  48 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM24 },
		{  72 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM36 },
		{  96 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM48 },
		{ 108 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM54 },
		{   2 | IFM_IEEE80211_11G, IFM_IEEE80211_DS1 },
		{   4 | IFM_IEEE80211_11G, IFM_IEEE80211_DS2 },
		{  11 | IFM_IEEE80211_11G, IFM_IEEE80211_DS5 },
		{  22 | IFM_IEEE80211_11G, IFM_IEEE80211_DS11 },
		{  12 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM6 },
		{  18 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM9 },
		{  24 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM12 },
		{  36 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM18 },
		{  48 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM24 },
		{  72 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM36 },
		{  96 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM48 },
		{ 108 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM54 },
		{   6 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM3 },
		{   9 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM4 },
		{  54 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM27 },
		/* NB: OFDM72 doesn't realy exist so we don't handle it */
	};
	static const struct ratemedia htrates[] = {
		{   0, IFM_IEEE80211_MCS },
		{   1, IFM_IEEE80211_MCS },
		{   2, IFM_IEEE80211_MCS },
		{   3, IFM_IEEE80211_MCS },
		{   4, IFM_IEEE80211_MCS },
		{   5, IFM_IEEE80211_MCS },
		{   6, IFM_IEEE80211_MCS },
		{   7, IFM_IEEE80211_MCS },
		{   8, IFM_IEEE80211_MCS },
		{   9, IFM_IEEE80211_MCS },
		{  10, IFM_IEEE80211_MCS },
		{  11, IFM_IEEE80211_MCS },
		{  12, IFM_IEEE80211_MCS },
		{  13, IFM_IEEE80211_MCS },
		{  14, IFM_IEEE80211_MCS },
		{  15, IFM_IEEE80211_MCS },
	};
	int m;

	/*
	 * Check 11n rates first for match as an MCS.
	 */
	if (mode == IEEE80211_MODE_11NA) {
		if (rate & IEEE80211_RATE_MCS) {
			rate &= ~IEEE80211_RATE_MCS;
			m = findmedia(htrates, N(htrates), rate);
			if (m != IFM_AUTO)
				return m | IFM_IEEE80211_11NA;
		}
	} else if (mode == IEEE80211_MODE_11NG) {
		/* NB: 12 is ambiguous, it will be treated as an MCS */
		if (rate & IEEE80211_RATE_MCS) {
			rate &= ~IEEE80211_RATE_MCS;
			m = findmedia(htrates, N(htrates), rate);
			if (m != IFM_AUTO)
				return m | IFM_IEEE80211_11NG;
		}
	}
	rate &= IEEE80211_RATE_VAL;
	switch (mode) {
	case IEEE80211_MODE_11A:
	case IEEE80211_MODE_11NA:
	case IEEE80211_MODE_TURBO_A:
	case IEEE80211_MODE_STURBO_A:
		return findmedia(rates, N(rates), rate | IFM_IEEE80211_11A);
	case IEEE80211_MODE_11B:
		return findmedia(rates, N(rates), rate | IFM_IEEE80211_11B);
	case IEEE80211_MODE_FH:
		return findmedia(rates, N(rates), rate | IFM_IEEE80211_FH);
	case IEEE80211_MODE_AUTO:
		/* NB: ic may be NULL for some drivers */
		if (ic && ic->ic_phytype == IEEE80211_T_FH)
			return findmedia(rates, N(rates),
			    rate | IFM_IEEE80211_FH);
		/* NB: hack, 11g matches both 11b+11a rates */
		/* fall thru... */
	case IEEE80211_MODE_11G:
	case IEEE80211_MODE_11NG:
	case IEEE80211_MODE_TURBO_G:
		return findmedia(rates, N(rates), rate | IFM_IEEE80211_11G);
	}
	return IFM_AUTO;
#undef N
}

int
ieee80211_media2rate(int mword)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const int ieeerates[] = {
		-1,		/* IFM_AUTO */
		0,		/* IFM_MANUAL */
		0,		/* IFM_NONE */
		2,		/* IFM_IEEE80211_FH1 */
		4,		/* IFM_IEEE80211_FH2 */
		2,		/* IFM_IEEE80211_DS1 */
		4,		/* IFM_IEEE80211_DS2 */
		11,		/* IFM_IEEE80211_DS5 */
		22,		/* IFM_IEEE80211_DS11 */
		44,		/* IFM_IEEE80211_DS22 */
		12,		/* IFM_IEEE80211_OFDM6 */
		18,		/* IFM_IEEE80211_OFDM9 */
		24,		/* IFM_IEEE80211_OFDM12 */
		36,		/* IFM_IEEE80211_OFDM18 */
		48,		/* IFM_IEEE80211_OFDM24 */
		72,		/* IFM_IEEE80211_OFDM36 */
		96,		/* IFM_IEEE80211_OFDM48 */
		108,		/* IFM_IEEE80211_OFDM54 */
		144,		/* IFM_IEEE80211_OFDM72 */
		0,		/* IFM_IEEE80211_DS354k */
		0,		/* IFM_IEEE80211_DS512k */
		6,		/* IFM_IEEE80211_OFDM3 */
		9,		/* IFM_IEEE80211_OFDM4 */
		54,		/* IFM_IEEE80211_OFDM27 */
		-1,		/* IFM_IEEE80211_MCS */
	};
	return IFM_SUBTYPE(mword) < N(ieeerates) ?
		ieeerates[IFM_SUBTYPE(mword)] : 0;
#undef N
}
