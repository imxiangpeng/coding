/*
 * Copyright (C) 2024 Inspur Group Co., Ltd. Unpublished
 *
 * Inspur Group Co., Ltd.
 * Proprietary & Confidential
 *
 * This source code and the algorithms implemented therein constitute
 * confidential information and may comprise trade secrets of Inspur
 * or its associates, and any use thereof is subject to the terms and
 * conditions of the Non-Disclosure Agreement pursuant to which this
 * source code was originally received.
 */

#include "hrif_wireless.h"


#include "private/hrif.h"

#include "hrbinder.h"

#include "private/hrif_transact_code.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int hrif_wireless_init() {
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_INIT, NULL, 0, NULL, NULL);
}

// IgdDevCapabilityTab.ucWlanNum/Wlan24Num
int hrif_wireless_capability(hrif_wireless_capability_t *cap) {
    if (!cap) return -1;
    uint32_t len = sizeof(hrif_wireless_capability_t);
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_CAPABILITY, NULL, 0, (void *)cap, &len);
}

int hrif_wireless_radio_status(int id, hrif_wireless_radio_status_t *data) {
    uint32_t len = sizeof(hrif_wireless_radio_status_t);
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_RADIO_STATUS, (void *)&id, sizeof(id), (void *)data, &len);
}

int hrif_wireless_radio_get(int id, hrif_wireless_radio_t *data) {
    uint32_t len = sizeof(hrif_wireless_radio_t);
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_RADIO_GET, (void *)&id, sizeof(id), (void *)data, &len);
}
int hrif_wireless_radio_set(int id, hrif_wireless_radio_t *data) {
    if (!data) return -1;
    struct {
        int id;
        hrif_wireless_radio_t radio;
    } d = {id, *data};
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_RADIO_SET, (void *)&d, sizeof(d), NULL, NULL);
}
int hrif_wireless_interface_get(int id, hrif_wireless_interface_t *iface) {
    uint32_t len = sizeof(hrif_wireless_interface_t);
    if (!iface) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_INTERFACE_GET, (void *)&id, sizeof(id), (void *)iface, &len);
}

int hrif_wireless_interface_set(int id, hrif_wireless_interface_t *iface) {
    if (!iface) return -1;
    struct {
        int id;
        hrif_wireless_interface_t iface;
    } data = {id, *iface};
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_INTERFACE_SET, (void *)&data, sizeof(data), NULL, NULL);
}

int hrif_wireless_bandsteering_get(hrif_wireless_bandsteering_t *bandsteering) {
    uint32_t len = sizeof(hrif_wireless_interface_t);
    if (!bandsteering) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_BANDSTEERING_GET, NULL, 0, (void *)bandsteering, &len);
}

// adjust 5g ssid name append -5G when bandsteer disabled
int hrif_wireless_bandsteering_set(hrif_wireless_bandsteering_t *bandsteering) {
    if (!bandsteering) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_BANDSTEERING_SET, (void *)bandsteering, sizeof(*bandsteering), NULL, NULL);
}

int hrif_wireless_scan_list_array(hrif_wireless_scan_node_t **list, uint32_t *size, hrif_wireless_band_e band) {
    (void)list;
    (void)size;
    (void)band;
    return 0;
}

int hrif_wireless_scan_connect(hrif_wireless_scan_node_t *node) {
    (void)node;
    return 0;
}

int hrif_wireless_scan_status(uint32_t *status) {
    (void)status;
    return 0;
}

int hrif_wireless_wps_set(hrif_wireless_band_e band) {
    (void)band;
    return 0;
}

int hrif_wireless_wps_get(uint32_t *status, hrif_wireless_band_e band) {
    (void)status;
    (void)band;
    return 0;
}

int hrif_wireless_timer_switch_set() {

    return 0;
}

int hrif_wireless_timer_add(hrif_wireless_timer_t *data) {
    (void)data;
    return 0;
}

int hrif_wireless_timer_del(char *idx) {
    (void)idx;
    return 0;
}

int hrif_wireless_timer_mod(hrif_wireless_timer_t *data) {
    (void)data;

    return 0;
}

int hrif_wireless_timer_array(hrif_wireless_timer_t **list, uint32_t *size) {
    (void)list;
    (void)size;
    return 0;
}
