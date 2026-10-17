/* Copyright (c) Kuba Szczodrzyński 2026-10-17. */

// Workarounds for the Beken SDK handling of a second (STA) vif while the soft AP
// is running. The MAC library leaves parts of the STA vif entry uninitialised, so
// in AP+STA mode the 4-way handshake and DHCP never reach the AP.
//
// Only needed (and only linked, see builder/family/beken-72xx.py) on BK7231T/Q
// (SOC_BK7231) and BK7231N with BDK <= 3.0.33. The problem was not observed on
// BK7238 with BDK 3.0.78. Other chips and BDK versions are untested.

#include "include.h"

#if defined(CFG_SOC_NAME) && defined(SOC_BK7231) && defined(SOC_BK7231N) && defined(CFG_BDK_VERSION)
#if (CFG_SOC_NAME == SOC_BK7231 || CFG_SOC_NAME == SOC_BK7231N) && CFG_BDK_VERSION <= 30033

#include <string.h>

#include "param_config.h"
#include "lwip/netif.h"
#include "rw_pub.h"
#include "uart_pub.h"
#include "vif_mgmt.h"

extern int __real_rw_msg_send_add_if(
	const unsigned char *mac,
	enum nl80211_iftype iftype,
	bool p2p,
	struct mm_add_if_cfm *cfm
);

// The second vif is registered with an all-zero MAC, even though the supplicant
// requested the correct one. rwm_mgmt_vif_mac2idx() then fails in
// l2_packet_init()/net_wlan_add_netif() and EAPOL frames are never delivered.
// Write the requested MAC into the vif entry right after it is created, before
// the supplicant looks it up.
int __wrap_rw_msg_send_add_if(
	const unsigned char *mac,
	enum nl80211_iftype iftype,
	bool p2p,
	struct mm_add_if_cfm *cfm
) {
	int ret = __real_rw_msg_send_add_if(mac, iftype, p2p, cfm);
	if (ret == 0 && cfm->status == CO_OK && cfm->inst_nbr < NX_VIRT_DEV_MAX) {
		unsigned char *vif_mac = (unsigned char *)vif_mgmt_get_addr(cfm->inst_nbr);
		if (!(vif_mac[0] | vif_mac[1] | vif_mac[2] | vif_mac[3] | vif_mac[4] | vif_mac[5]))
			memcpy(vif_mac, mac, 6);
	}
	return ret;
}

extern unsigned int __real_rwm_transfer(unsigned char vif_idx, unsigned char *buf, unsigned int len, int sync, void *arg);

// The STA vif's peer-AP station index (u.sta.ap_id) is never updated and stays 0,
// which is the soft AP's first client. rwm_mgmt_tx_get_staidx() then returns that
// station for every frame sent through the STA vif, so the 4-way handshake reply
// is queued against the wrong peer and never reaches the AP. Resolve the peer
// from the EAPOL destination (the AP's BSSID) before sending.
unsigned int __wrap_rwm_transfer(unsigned char vif_idx, unsigned char *buf, unsigned int len, int sync, void *arg) {
	if (len > 14 && buf[12] == 0x88 && buf[13] == 0x8e) {
		VIF_INF_PTR vif = rwm_mgmt_vif_idx2ptr(vif_idx);
		if (vif && vif->type == VIF_STA) {
			unsigned char sta_idx = rwm_mgmt_sta_mac2idx(buf);
			if (sta_idx != 0xff)
				vif->u.sta.ap_id = sta_idx;
		}
	}
	return __real_rwm_transfer(vif_idx, buf, len, sync, arg);
}

extern unsigned char __real_rwm_mgmt_get_netif2vif(struct netif *netif);

// The lwIP glue resolves netif -> vif through vif_entry->index, which is 0 for the
// second vif (and gets reset by the library after being written). The STA netif
// would then transmit out of the soft AP vif and DHCP never gets out.
// netif->state is a pointer into vif_info_tab, so derive the vif number from the
// table slot instead.
unsigned char __wrap_rwm_mgmt_get_netif2vif(struct netif *netif) {
	if (netif && netif->state) {
		for (unsigned char i = 0; i < NX_VIRT_DEV_MAX; i++) {
			if ((void *)rwm_mgmt_vif_idx2ptr(i) == netif->state)
				return i;
		}
	}
	return __real_rwm_mgmt_get_netif2vif(netif);
}

extern unsigned char __real_rwm_mgmt_vif_mac2idx(void *mac);

// The library clears the STA vif's MAC again when the connection is made, which
// breaks every lookup by MAC (e.g. bk_wlan_get_link_status() and thus
// WiFi.SSID()). Resolve our own STA MAC to the STA vif when the lookup fails.
unsigned char __wrap_rwm_mgmt_vif_mac2idx(void *mac) {
	unsigned char idx = __real_rwm_mgmt_vif_mac2idx(mac);
	if (idx == 0xff && mac) {
		unsigned char sta_mac[6];
		wifi_get_mac_address((char *)sta_mac, CONFIG_ROLE_STA);
		if (!memcmp(mac, sta_mac, 6)) {
			for (unsigned char i = 0; i < NX_VIRT_DEV_MAX; i++) {
				VIF_INF_PTR vif = rwm_mgmt_vif_idx2ptr(i);
				if (vif && vif->type == VIF_STA)
					return i;
			}
		}
	}
	return idx;
}

#endif /* (BK7231 || BK7231N) && BDK <= 3.0.33 */
#endif /* defined(...) */
