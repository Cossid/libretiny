#include "include.h"
#include "wlan_ui_pub.h"
#include "rw_pub.h"
#include "param_config.h"
#include "wpa_psk_cache.h"

extern void bk_wlan_sta_init_adv(network_InitTypeDef_adv_st *inNetworkInitParaAdv);

extern void bk_wlan_sta_adv_param_2_sta(network_InitTypeDef_adv_st *sta_adv,network_InitTypeDef_st* sta);

/* Decode a single hex digit; returns -1 if `c` is not a hex digit. */
static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Decode 64 hex characters into a 32-byte PSK; returns false if invalid. */
static bool hex_to_psk(const char *hex, uint8_t *psk)
{
    for (int i = 0; i < 32; i++) {
        int hi = hex_nibble(hex[2 * i]);
        int lo = hex_nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            return false;
        psk[i] = (hi << 4) | lo;
    }
    return true;
}

/*
 * Request the PSK for the station, copying the key to a null-terminated buffer
 * as `key_len` is authoritative. A 64-character hex key is a pre-computed PSK,
 * so it is decoded and passed directly instead of being derived as a passphrase.
 */
static void bk_wlan_request_psk(void)
{
    char key_buffer[PMK_LEN_MAX + 1];
    uint8_t psk[PMK_LEN];
    size_t key_len = g_sta_param_ptr->key_len;

    if (key_len > PMK_LEN_MAX)
        key_len = PMK_LEN_MAX;

    /* Make copy with a guaranteed null terminator. */
    memcpy(key_buffer, g_sta_param_ptr->key, key_len);
    key_buffer[key_len] = 0;

    if (key_len == PMK_LEN * 2 && hex_to_psk(key_buffer, psk)) {
        wpa_psk_request(g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length,
                        key_buffer, psk, PMK_LEN);
    } else {
        /* let wpa_psk_cal thread to calculate psk. */
        wpa_psk_request(g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length,
                        key_buffer, NULL, 0);
    }
}

OSStatus bk_wlan_start_sta_adv_fix(network_InitTypeDef_adv_st *inNetworkInitParaAdv)
{
    if (bk_wlan_is_monitor_mode()) {
        return 0;
    }

#if !CFG_WPA_CTRL_IFACE
#if CFG_ROLE_LAUNCH
    rl_status_set_private_state(RL_PRIV_STATUS_STA_ADV_RDY);
#endif
#if CFG_ROLE_LAUNCH
    rl_status_set_st_state(RL_ST_STATUS_RESTART_ST);
#endif

    bk_wlan_stop(BK_STATION);
#if CFG_ROLE_LAUNCH
    if (rl_pre_sta_set_status(RL_STATUS_STA_INITING))
        return -1;

    rl_status_reset_st_state(RL_ST_STATUS_RESTART_HOLD | RL_ST_STATUS_RESTART_ST);
    rl_status_set_private_state(RL_PRIV_STATUS_STA_ADV);
    rl_status_reset_private_state(RL_PRIV_STATUS_STA_ADV_RDY);
    LAUNCH_REQ lreq;

    lreq.req_type = LAUNCH_REQ_STA;
    bk_wlan_sta_adv_param_2_sta(inNetworkInitParaAdv, &lreq.descr);

    rl_sta_adv_register_cache_station(&lreq);
#endif

    bk_wlan_sta_init_adv(inNetworkInitParaAdv);

    bk_wlan_request_psk();

    supplicant_main_entry(inNetworkInitParaAdv->ap_info.ssid);

    net_wlan_add_netif(&g_sta_param_ptr->own_mac);


#else /* CFG_WPA_CTRL_IFACE */
    wlan_sta_config_t config;

#if (CFG_SOC_NAME == SOC_BK7231N)
    if (get_ate_mode_state()) {
        // cunliang20210407 set blk_standby_cfg with blk_txen_cfg like txevm, qunshan confirmed
        rwnx_cal_en_extra_txpa();
    }
#endif
    bk_wlan_sta_init_adv(inNetworkInitParaAdv);

    bk_wlan_request_psk();

    /* disconnect previous connected network */
    wlan_sta_disconnect();

    /* start wpa_supplicant */
    wlan_sta_enable();

    /* set network parameters: ssid, passphase */
    wlan_sta_set((uint8_t*)inNetworkInitParaAdv->ap_info.ssid, strlen(inNetworkInitParaAdv->ap_info.ssid),
                 (uint8_t*)inNetworkInitParaAdv->key);

    /* set fast connect bssid */
    memset(&config, 0, sizeof(config));
    /*
     * This API has no way to communicate whether BSSID was supplied or not.
     * So we will use an assumption that no valid endpoint will ever use the
     * `00:00:00:00:00:00`. It's basically a MAC address and no valid device
     * should ever use the value consisting of all zeroes.
     *
     * We have just zeroed out the `config` struct so we can use those zeroes
     * for comparison with our input argument.
     */
    if (memcmp(config.u.bssid, inNetworkInitParaAdv->ap_info.bssid, ETH_ALEN)) {
        memcpy(config.u.bssid, inNetworkInitParaAdv->ap_info.bssid, ETH_ALEN);
        config.field = WLAN_STA_FIELD_BSSID;
        wpa_ctrl_request(WPA_CTRL_CMD_STA_SET, &config);
    }

    /* set fast connect freq */
    memset(&config, 0, sizeof(config));
    config.u.channel = inNetworkInitParaAdv->ap_info.channel;
    config.field = WLAN_STA_FIELD_FREQ;
    wpa_ctrl_request(WPA_CTRL_CMD_STA_SET, &config);

    /* connect to AP */
    wlan_sta_connect(g_sta_param_ptr->fast_connect_set ? g_sta_param_ptr->fast_connect.chann : 0);

#endif
    ip_address_set(BK_STATION, inNetworkInitParaAdv->dhcp_mode,
                   inNetworkInitParaAdv->local_ip_addr,
                   inNetworkInitParaAdv->net_mask,
                   inNetworkInitParaAdv->gateway_ip_addr,
                   inNetworkInitParaAdv->dns_server_ip_addr);

    return 0;
}

// Allow changing the SDK's RSSI threshold for disabling HT capability
// This defaults to -50 in BDK, which most devices don't even reach
#ifndef LT_BK_NOHT_RSSI_THRESHOLD
#define LT_BK_NOHT_RSSI_THRESHOLD -100
#endif

// Declared in beken378/ip/lmac/src/rwnx/rwnx_config.h
__attribute__((weak)) int rwnx_get_noht_rssi_thresold(void) {
    return LT_BK_NOHT_RSSI_THRESHOLD;
}
