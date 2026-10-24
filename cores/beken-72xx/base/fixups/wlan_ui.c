#include "include.h"
#include "wlan_ui_pub.h"
#include "rw_pub.h"
#include "param_config.h"
#include "wpa_psk_cache.h"

extern void bk_wlan_sta_init_adv(network_InitTypeDef_adv_st *inNetworkInitParaAdv);

extern void bk_wlan_sta_adv_param_2_sta(network_InitTypeDef_adv_st *sta_adv,network_InitTypeDef_st* sta);

/*
 * Copy the key to a null-terminated buffer, as `key_len` is authoritative and a
 * 64-character key (a hex PSK) fills the whole `key` array with no terminator.
 */
static void bk_wlan_copy_key(char key_buffer[PMK_LEN_MAX + 1])
{
    size_t key_len = g_sta_param_ptr->key_len;

    if (key_len > PMK_LEN_MAX)
        key_len = PMK_LEN_MAX;

    memcpy(key_buffer, g_sta_param_ptr->key, key_len);
    key_buffer[key_len] = 0;
}

/* Decode a 64-character hex PSK into 32 bytes; false if not a hex PSK. */
static bool bk_wlan_key_to_psk(const char *key_buffer, uint8_t psk[PMK_LEN])
{
    if (strlen(key_buffer) != PMK_LEN * 2)
        return false;
    for (int i = 0; i < PMK_LEN; i++) {
        if (!isxdigit((unsigned char)key_buffer[2 * i]) ||
            !isxdigit((unsigned char)key_buffer[2 * i + 1]))
            return false;
        char byte[3] = {key_buffer[2 * i], key_buffer[2 * i + 1], 0};
        psk[i] = (uint8_t)strtoul(byte, NULL, 16);
    }
    return true;
}

/*
 * Allow or forbid the supplicant from selecting SAE (WPA3). This is done by
 * toggling the driver's SAE capability flag, which is set when the supplicant is
 * created, so it must be called after the supplicant is enabled.
 */
static void bk_wlan_set_sae_allowed(bool allowed)
{
    extern struct wpa_supplicant *wpa_suppliant_ctrl_get_wpas(void);
    static bool sae_hidden = false;
    struct wpa_supplicant *wpa_s = wpa_suppliant_ctrl_get_wpas();

    if (!wpa_s)
        return;
    if (!allowed && (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SAE)) {
        wpa_s->drv_flags &= ~(u64)WPA_DRIVER_FLAGS_SAE;
        sae_hidden = true;
    } else if (allowed && sae_hidden) {
        wpa_s->drv_flags |= WPA_DRIVER_FLAGS_SAE;
        sae_hidden = false;
    }
}

/* let wpa_psk_cal thread to calculate the PSK from the passphrase */
static void bk_wlan_request_psk(const char *key_buffer)
{
    wpa_psk_request(g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length,
                    (char *)key_buffer, NULL, 0);
}

/*
 * Apply a known PSK to the configured network. This is done directly instead of
 * through `wpa_psk_request()`, which caches the last PSK and does nothing when
 * asked for the same one again, leaving a recreated network without a PSK. The
 * network must already be configured, as the PSK is matched against it.
 */
static void bk_wlan_apply_psk(const char *passphrase, const uint8_t psk[PMK_LEN])
{
    wlan_gen_psk_param_t param;

    memset(&param, 0, sizeof(param));
    memcpy(param.ssid, g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length);
    param.ssid_len = g_sta_param_ptr->ssid.length;
    strlcpy(param.passphrase, passphrase, sizeof(param.passphrase));
    memcpy(param.psk, psk, PMK_LEN);
    wlan_sta_gen_psk(&param);
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

    {
        char key_buffer[PMK_LEN_MAX + 1];
        bk_wlan_copy_key(key_buffer);
        bk_wlan_request_psk(key_buffer);
    }

    supplicant_main_entry(inNetworkInitParaAdv->ap_info.ssid);

    net_wlan_add_netif(&g_sta_param_ptr->own_mac);


#else /* CFG_WPA_CTRL_IFACE */
    wlan_sta_config_t config;
    char key_buffer[PMK_LEN_MAX + 1];
    uint8_t psk[PMK_LEN];
    bool is_psk;

#if (CFG_SOC_NAME == SOC_BK7231N)
    if (get_ate_mode_state()) {
        // cunliang20210407 set blk_standby_cfg with blk_txen_cfg like txevm, qunshan confirmed
        rwnx_cal_en_extra_txpa();
    }
#endif
    bk_wlan_sta_init_adv(inNetworkInitParaAdv);

    bk_wlan_copy_key(key_buffer);

    /*
     * A hex PSK is handed over as a PSK instead of being used as the network's
     * passphrase. The supplicant still needs some passphrase to match the PSK
     * against, so a valid-length (63 character) placeholder is used for it.
     */
    is_psk = bk_wlan_key_to_psk(key_buffer, psk);
    if (is_psk)
        key_buffer[PMK_LEN_MAX - 1] = 0;

    /* disconnect previous connected network */
    wlan_sta_disconnect();

    /* start wpa_supplicant */
    wlan_sta_enable();

    /* set network parameters: ssid, passphase */
    wlan_sta_set((uint8_t*)inNetworkInitParaAdv->ap_info.ssid, strlen(inNetworkInitParaAdv->ap_info.ssid),
                 (uint8_t*)key_buffer);

    /*
     * The SDK re-enables SAE in the network's key management every time it
     * matches a scan result, and SAE is preferred over WPA2-PSK. SAE derives its
     * key from the passphrase, which a PSK cannot provide, so on a WPA2/WPA3
     * network it would be chosen and fail authentication. The supplicant only
     * selects SAE when the driver reports support for it, so hide that.
     */
    bk_wlan_set_sae_allowed(!is_psk);

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

    /*
     * The network is fully configured now, so the PSK can be applied to it.
     * Applying a PSK makes the supplicant start connecting right away, so this
     * is done after the BSSID and channel are set to avoid connecting twice with
     * a partial configuration.
     */
    if (is_psk)
        bk_wlan_apply_psk(key_buffer, psk);
    else
        bk_wlan_request_psk(key_buffer);

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
