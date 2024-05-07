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
#include "sys/types.h"
#include "zconf.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *_realloc(void *ptr, uint32_t size) {
    return realloc(ptr, size);
}

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
    void *ptr = NULL;
    uint32_t length = 0;

    if (!list || !size) return -1;

    int result = hrif_transact2(HRIF_TRANSACT_CODE_WIRELESS_SCAN_LIST_ARRAY, (void *)&band, sizeof(band), &ptr, &length, _realloc);
    if (result == 0) {
        *size = length / sizeof(hrif_wireless_scan_node_t);
        *list = ptr;
    }

    return result;
}

int hrif_wireless_scan_connect(hrif_wireless_scan_node_t *node) {
    if (!node) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_SCAN_CONNECT, node, sizeof(hrif_wireless_scan_node_t), NULL, NULL);
}

int hrif_wireless_scan_status(uint32_t *status) {
    uint32_t length = sizeof(uint32_t);
    if (!status) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_SCAN_STATUS, NULL, 0, (void *)status, &length);
}

int hrif_wireless_wps_set(hrif_wireless_band_e band) {
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_WPS_SET, (void *)&band, sizeof(band), NULL, NULL);
}

int hrif_wireless_wps_get(hrif_wireless_band_e band, uint32_t *status) {
    uint32_t length = sizeof(uint32_t);
    if (!status) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_WPS_GET, (void *)&band, sizeof(band), (void *)status, &length);
}

int hrif_wireless_wps_switch_set(hrif_wireless_band_e band, uint32_t status) {
    struct {
        int band;
        uint32_t status;
    } d = {band, status};
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_WPS_SWITCH_SET, (void *)&d, sizeof(d), NULL, NULL);
}

int hrif_wireless_wps_switch_get(hrif_wireless_band_e band, uint32_t *status) {
    uint32_t length = sizeof(uint32_t);
    if (!status) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_WPS_SWITCH_GET, (void *)&band, sizeof(band), (void *)status, &length);
}

int hrif_wireless_timer_switch_set(int on) {
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_TIMER_SWITCH_SET, (void *)&on, sizeof(on), NULL, NULL);
}

int hrif_wireless_timer_switch_get() {
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_TIMER_SWITCH_GET, NULL, 0, NULL, NULL);
}

int hrif_wireless_timer_add(hrif_wireless_timer_t *data) {
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_TIMER_ADD, (void *)data, sizeof(*data), NULL, NULL);
}

int hrif_wireless_timer_del(char *idx) {
    if (!idx) return -1;
    int len = 4 /*len*/ + strlen(idx) + 1 /*end*/;
    uint32_t *ptr = (uint32_t *)calloc(1, len);
    if (!ptr) return -1;
    *ptr = strlen(idx) + 1;
    memcpy((void *)(ptr + 1), idx, strlen(idx));
    int result = hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_TIMER_DEL, (void *)ptr, len, NULL, NULL);
    free(ptr);
    return result;
}

int hrif_wireless_timer_mod(hrif_wireless_timer_t *data) {
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_TIMER_ADD, (void *)data, sizeof(*data), NULL, NULL);
}

int hrif_wireless_timer_array(hrif_wireless_timer_t **list, uint32_t *size) {
    void *ptr = NULL;
    uint32_t length = 0;

    if (!list || !size) return -1;

    int result = hrif_transact2(HRIF_TRANSACT_CODE_WIRELESS_TIMER_ARRAY, NULL, 0, &ptr, &length, _realloc);
    if (result == 0) {
        *size = length / sizeof(hrif_wireless_timer_t);
        *list = ptr;
    }

    return result;
}

int hrif_wireless_deassociate(const char *mac) {
    if (!mac) return -1;

    // pass raw string
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_DEASSOCIATE, (void *)mac, strlen(mac) + 1, NULL, NULL);
}

int hrif_wireless_channelscan(hrif_wireless_band_e band) {
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_CHANNEL_SCAN, (void *)&band, sizeof(band), NULL, NULL);
}

int hrif_wireless_vsie_get(hrif_wireless_vsie_t *vsie) {
    uint32_t length = sizeof(hrif_wireless_vsie_t);
    if (!vsie) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_VSIE_GET, NULL, 0, (void *)vsie, &length);
}

int hrif_wireless_vsie_beacontx_set(hrif_wireless_vsie_beacontx_action_e action, hrif_wireless_vsie_beacontx_t *vsie_beacontx) {
    if (!vsie_beacontx) return -1;
    struct {
        int action;
        hrif_wireless_vsie_beacontx_t data;
    } d = {action, *vsie_beacontx};
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_VSIE_BEACONTX_SET, (void *)&d, sizeof(d), NULL, NULL);
}

int hrif_wireless_vsie_beacontx_list(hrif_wireless_vsie_beacontx_t **vsie_beacontx, uint32_t *num) {
    void *ptr = NULL;
    uint32_t length = 0;

    if (!vsie_beacontx || !num) return -1;

    int result = hrif_transact2(HRIF_TRANSACT_CODE_WIRELESS_VSIE_BEACONTX_LST, NULL, 0, &ptr, &length, _realloc);
    if (result == 0) {
        *num = length / sizeof(hrif_wireless_timer_t);
        *vsie_beacontx = ptr;
    }

    return result;
}

int hrif_wireless_vsie_proberx_set(hrif_wireless_vsie_proberx_action_e action, hrif_wireless_vsie_proberx_t *vsie_proberx) {
    if (!vsie_proberx) return -1;
    struct {
        int action;
        hrif_wireless_vsie_proberx_t data;
    } d = {action, *vsie_proberx};
    return hrif_transact(HRIF_TRANSACT_CODE_WIRELESS_VSIE_PROBERX_SET, (void *)&d, sizeof(d), NULL, NULL);
}

int hrif_wireless_vsie_proberx_list(hrif_wireless_vsie_proberx_t **vsie_proberx, uint32_t *num) {
    void *ptr = NULL;
    uint32_t length = 0;

    if (!vsie_proberx || !num) return -1;
    int result = hrif_transact2(HRIF_TRANSACT_CODE_WIRELESS_VSIE_PROBERX_LST, NULL, 0, &ptr, &length, _realloc);
    if (result == 0) {
        *num = length / sizeof(hrif_wireless_timer_t);
        *vsie_proberx = ptr;
    }

    return result;
}

int hrif_wireless_channelscore_get(hrif_wireless_band_e band, hrif_wireless_channelscore_t **score, int *num) {
    void *ptr = NULL;
    uint32_t length = 0;
    if (!score|| !num) return -1;
    int result = hrif_transact2(HRIF_TRANSACT_CODE_WIRELESS_CHANNEL_SCORE_GET, (void*)&band, sizeof(band), &ptr, &length, _realloc);
    if (result == 0) {
        *num = length / sizeof(hrif_wireless_channelscore_t);
        *score = ptr;
    }

    return result;
}