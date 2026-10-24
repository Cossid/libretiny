/* Copyright (c) Kuba Szczodrzyński 2023-02-27. */

#include <libretiny.h>
#include <sdk_private.h>

void lt_get_device_mac(uint8_t *mac) {
	cfg_load_mac(mac);
}

void lt_reboot() {
#if CFG_BDK_VERSION >= 30076 && CFG_WPA_CTRL_IFACE
	// Leave the AP cleanly, as it rejects a quick reconnect while it still
	// holds the old PMF session.
	LinkStatusTypeDef link;
	if (bk_wlan_get_link_status(&link) == kNoErr) {
		wlan_sta_disconnect();
		rtos_delay_milliseconds(50);
	}
#endif
	bk_reboot();
}

bool lt_reboot_download_mode() {
	bk_reboot();
	return true;
}

lt_reboot_reason_t lt_get_reboot_reason() {
	switch (bk_misc_get_start_type()) {
		case RESET_SOURCE_POWERON:
			return REBOOT_REASON_POWER;
		case RESET_SOURCE_REBOOT:
			return REBOOT_REASON_SOFTWARE;
		case RESET_SOURCE_WATCHDOG:
			return REBOOT_REASON_WATCHDOG;
		case RESET_SOURCE_CRASH_XAT0:
		case RESET_SOURCE_CRASH_UNDEFINED:
		case RESET_SOURCE_CRASH_PREFETCH_ABORT:
		case RESET_SOURCE_CRASH_DATA_ABORT:
		case RESET_SOURCE_CRASH_UNUSED:
		case RESET_SOURCE_CRASH_PER_XAT0:
			return REBOOT_REASON_CRASH;
		case RESET_SOURCE_DEEPPS_USB:
			return REBOOT_REASON_SLEEP_USB;
		case RESET_SOURCE_DEEPPS_GPIO:
			return REBOOT_REASON_SLEEP_GPIO;
		case RESET_SOURCE_DEEPPS_RTC:
			return REBOOT_REASON_SLEEP_RTC;
		default:
			return REBOOT_REASON_UNKNOWN;
	}
}
