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

// mxp, 20240423 implement binder service
// binder buffer is limited at 1k,
// you should use shared memory when data is large!

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hrif_wireless.h"

#include "hrifd.h"

#include "private/hrif_transact_code.h"

#include "hr_log.h"
#include "unistd.h"

#define SVC_NAME "hrifd.wireless"

static int _hrif_wireless_init(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    hrif_wireless_init();
    bio_put_uint32(reply, 0);
    return 0;
}

HRIF_STRUCT_GET(hrif_wireless_capability_t, hrif_wireless_capability)
HRIF_STRUCT_GET_WITH_ID(hrif_wireless_radio_status_t, hrif_wireless_radio_status)
HRIF_STRUCT_GET_WITH_ID(hrif_wireless_radio_t, hrif_wireless_radio_get)
HRIF_STRUCT_SET_WITH_ID(hrif_wireless_radio_t, hrif_wireless_radio_set)
HRIF_STRUCT_GET_WITH_ID(hrif_wireless_interface_t, hrif_wireless_interface_get)
HRIF_STRUCT_SET_WITH_ID(hrif_wireless_interface_t, hrif_wireless_interface_set)
HRIF_STRUCT_GET(hrif_wireless_bandsteering_t, hrif_wireless_bandsteering_get)
HRIF_STRUCT_SET(hrif_wireless_bandsteering_t, hrif_wireless_bandsteering_set)

static int _hrif_wireless_scan_list_array(struct binder_io *msg, struct binder_io *reply) {
    uint32_t payload_length = 0;
    void *ptr = NULL;
    hrif_wireless_scan_node_t *l = NULL;
    uint32_t size = 0;

    if (!msg || !reply) return -1;
    hrif_wireless_band_e band = bio_get_uint32(msg);

    int result = hrif_wireless_scan_list_array(&l, &size, band);
    if (result != 0 || !l) {
        bio_put_uint32(reply, result);
        return -1;
    }

    payload_length = sizeof(hrif_wireless_scan_node_t) * size;
    // using our dynamic memory when it's large than reply default size
    if (payload_length + 4 + 4 > reply->data_avail) {
        // bio not use maxoffset, you should add it if you use it
        ptr = realloc(l, payload_length + 4 + 4);  // result + result size + payload
        if (!ptr) {
            free(l);
            bio_put_uint32(reply, -1);
            return -1;
        }

        // skip result + payload length
        memmove((void *)((char *)ptr + 4 + 4), ptr, payload_length);
        // ptr will be freed auto in BR_TRANSACTION
        bio_init_with_prealloced(reply, ptr, payload_length + 4 + 4, 0);
    }

    // must call to alloc memory
    int *data = (int *)bio_alloc(reply, payload_length + 4 + 4);
    if (!data) {
        if (!ptr) {
            free(l);  // not BIO_F_MALLOCED, release it
        }
        printf("not enough ..........\n");
        return -1;
    }

    *data = 0;
    *(data + 1) = payload_length;
    // not using dynamic memory, we should copy it manual
    if (!ptr) {
        memcpy((void *)(data + 2), l, payload_length);
        // reply is not malloced, we should release l here
        free(l);  // not BIO_F_MALLOCED, release it
    }

    return 0;
}

HRIF_STRUCT_SET(hrif_wireless_scan_node_t, hrif_wireless_scan_connect)
HRIF_STRUCT_GET(uint32_t, hrif_wireless_scan_status)

HRIF_STRUCT_GET_WITH_ID(uint32_t, hrif_wireless_wps_get)

static int _hrif_wireless_wps_set(struct binder_io *msg, struct binder_io *reply) {
    hrif_wireless_band_e band = bio_get_uint32(msg);
    int result = hrif_wireless_wps_set(band);
    bio_put_uint32(reply, result);
    return 0;
}

HRIF_STRUCT_GET_WITH_ID(uint32_t, hrif_wireless_wps_switch_get)

static int _hrif_wireless_wps_switch_set(struct binder_io *msg, struct binder_io *reply) {
    hrif_wireless_band_e band = bio_get_uint32(msg);
    int status = bio_get_uint32(msg);
    int result = hrif_wireless_wps_switch_set(band, status);
    bio_put_uint32(reply, result);
    return 0;
}

static int _hrif_wireless_timer_switch_get(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    int status = hrif_wireless_timer_switch_get();
    bio_put_uint32(reply, status);
    return 0;
}

static int _hrif_wireless_timer_switch_set(struct binder_io *msg, struct binder_io *reply) {
    int status = bio_get_uint32(msg);
    int result = hrif_wireless_timer_switch_set(status);
    bio_put_uint32(reply, result);
    return 0;
}

HRIF_STRUCT_SET(hrif_wireless_timer_t, hrif_wireless_timer_add)
HRIF_STRUCT_SET(hrif_wireless_timer_t, hrif_wireless_timer_mod)

static int _hrif_wireless_timer_del(struct binder_io *msg, struct binder_io *reply) {
    uint32_t len = bio_get_uint32(msg);

    // must call to alloc memory
    char *data = (char *)bio_alloc(msg, len);
    if (!data) {
        printf("not enough ..........\n");
        return -1;
    }

    // hrif_wireless_timer_del is bad interface!
    // it call strtok which may modify data!
    // data pointer is readonly!
    // it will crash if you pass data directly
    char *writable = strdup(data);
    int result = hrif_wireless_timer_del(writable);
    free(writable);
    bio_put_uint32(reply, result);
    return 0;
}

static int _hrif_wireless_timer_array(struct binder_io *msg, struct binder_io *reply) {
    uint32_t payload_length = 0;
    void *ptr = NULL;
    hrif_wireless_timer_t *t = NULL;
    uint32_t size = 0;

    if (!msg || !reply) return -1;

    int result = hrif_wireless_timer_array(&t, &size);
    if (result != 0 || !t) {
        return result;
    }

    payload_length = sizeof(hrif_wireless_timer_t) * size;
    // using our dynamic memory when it's large than reply default size
    if (payload_length + 4 + 4 > reply->data_avail) {
        // bio not use maxoffset, you should add it if you use it
        ptr = realloc(t, payload_length + 4 + 4);  // result + result size + payload
        if (!ptr) {
            free(t);
            return -1;
        }

        // skip result + payload length
        memmove((void *)((char *)ptr + 4 + 4), ptr, payload_length);
        // ptr will be freed auto in BR_TRANSACTION
        bio_init_with_prealloced(reply, ptr, payload_length + 4 + 4, 0);
    }

    // must call to alloc memory
    int *data = (int *)bio_alloc(reply, payload_length + 4 + 4);
    if (!data) {
        if (!ptr) {
            free(t);  // not BIO_F_MALLOCED, release it
        }
        printf("not enough ..........\n");
        return -1;
    }

    *data = 0;
    *(data + 1) = payload_length;
    // not using dynamic memory, we should copy it manual
    if (!ptr) {
        memcpy((void *)(data + 2), t, payload_length);
        // reply is not malloced, we should release l here
        free(t);  // not BIO_F_MALLOCED, release it
    }

    return 0;
}

static int _hrif_wireless_deassociate(struct binder_io *msg, struct binder_io *reply) {
    if (!msg || !reply) return -1;

    // directly access raw data
    const char *ptr = (const char *)msg->data;
    if (!ptr) return -1;

    return hrif_wireless_deassociate(ptr);
}

static int _hrif_wireless_channel_scan(struct binder_io *msg, struct binder_io *reply) {
    hrif_wireless_band_e band = bio_get_uint32(msg);
    int result = hrif_wireless_channelscan(band);
    bio_put_uint32(reply, result);
    return 0;
}

static int _hrif_wireless_channel_score_get(struct binder_io *msg, struct binder_io *reply) {
    hrif_wireless_band_e band = bio_get_uint32(msg);
    hrif_wireless_channelscore_t *l = NULL;
    int num = 0;

    uint32_t payload_length = 0;
    void *ptr = NULL;

    int result = hrif_wireless_channelscore_get(band, &l, &num);
    if (result != 0 || !l) {
        return result;
    }
    payload_length = sizeof(hrif_wireless_channelscore_t) * num;
    // using our dynamic memory when it's large than reply default size
    if (payload_length + 4 + 4 > reply->data_avail) {
        // bio not use maxoffset, you should add it if you use it
        ptr = realloc(l, payload_length + 4 + 4);  // result + result size + payload
        if (!ptr) {
            free(l);
            return -1;
        }

        // skip result + payload length
        memmove((void *)((char *)ptr + 4 + 4), ptr, payload_length);
        // ptr will be freed auto in BR_TRANSACTION
        bio_init_with_prealloced(reply, ptr, payload_length + 4 + 4, 0);
    }

    // must call to alloc memory
    int *data = (int *)bio_alloc(reply, payload_length + 4 + 4);
    if (!data) {
        if (!ptr) {
            free(l);  // not BIO_F_MALLOCED, release it
        }
        printf("not enough ..........\n");
        return -1;
    }

    *data = 0;
    *(data + 1) = payload_length;
    // not using dynamic memory, we should copy it manual
    if (!ptr) {
        memcpy((void *)(data + 2), l, payload_length);
        // reply is not malloced, we should release l here
        free(l);  // not BIO_F_MALLOCED, release it
    }

    return 0;
}

HRIF_STRUCT_GET(hrif_wireless_vsie_t, hrif_wireless_vsie_get)
HRIF_STRUCT_SET_WITH_ID(hrif_wireless_vsie_beacontx_t, hrif_wireless_vsie_beacontx_set)

static int _hrif_wireless_vsie_beacontx_list(struct binder_io *msg, struct binder_io *reply) {
    uint32_t payload_length = 0;
    void *ptr = NULL;
    hrif_wireless_vsie_beacontx_t *l = NULL;
    uint32_t size = 0;

    if (!msg || !reply) return -1;

    int result = hrif_wireless_vsie_beacontx_list(&l, &size);
    if (result != 0 || !l) {
        return result;
    }

    payload_length = sizeof(hrif_wireless_vsie_beacontx_t) * size;
    // using our dynamic memory when it's large than reply default size
    if (payload_length + 4 + 4 > reply->data_avail) {
        // bio not use maxoffset, you should add it if you use it
        ptr = realloc(l, payload_length + 4 + 4);  // result + result size + payload
        if (!ptr) {
            free(l);
            return -1;
        }

        // skip result + payload length
        memmove((void *)((char *)ptr + 4 + 4), ptr, payload_length);
        // ptr will be freed auto in BR_TRANSACTION
        bio_init_with_prealloced(reply, ptr, payload_length + 4 + 4, 0);
    }

    // must call to alloc memory
    int *data = (int *)bio_alloc(reply, payload_length + 4 + 4);
    if (!data) {
        if (!ptr) {
            free(l);  // not BIO_F_MALLOCED, release it
        }
        printf("not enough ..........\n");
        return -1;
    }

    *data = 0;
    *(data + 1) = payload_length;
    // not using dynamic memory, we should copy it manual
    if (!ptr) {
        memcpy((void *)(data + 2), l, payload_length);
        // reply is not malloced, we should release l here
        free(l);  // not BIO_F_MALLOCED, release it
    }

    return 0;
}

HRIF_STRUCT_SET_WITH_ID(hrif_wireless_vsie_proberx_t, hrif_wireless_vsie_proberx_set)

static int _hrif_wireless_vsie_proberx_list(struct binder_io *msg, struct binder_io *reply) {
    uint32_t payload_length = 0;
    void *ptr = NULL;
    hrif_wireless_vsie_proberx_t *l = NULL;
    uint32_t size = 0;

    if (!msg || !reply) return -1;

    int result = hrif_wireless_vsie_proberx_list(&l, &size);
    if (result != 0 || !l) {
        return result;
    }

    payload_length = sizeof(hrif_wireless_vsie_proberx_t) * size;
    // using our dynamic memory when it's large than reply default size
    if (payload_length + 4 + 4 > reply->data_avail) {
        // bio not use maxoffset, you should add it if you use it
        ptr = realloc(l, payload_length + 4 + 4);  // result + result size + payload
        if (!ptr) {
            free(l);
            return -1;
        }

        // skip result + payload length
        memmove((void *)((char *)ptr + 4 + 4), ptr, payload_length);
        // ptr will be freed auto in BR_TRANSACTION
        bio_init_with_prealloced(reply, ptr, payload_length + 4 + 4, 0);
    }

    // must call to alloc memory
    int *data = (int *)bio_alloc(reply, payload_length + 4 + 4);
    if (!data) {
        if (!ptr) {
            free(l);  // not BIO_F_MALLOCED, release it
        }
        printf("not enough ..........\n");
        return -1;
    }

    *data = 0;
    *(data + 1) = payload_length;
    // not using dynamic memory, we should copy it manual
    if (!ptr) {
        memcpy((void *)(data + 2), l, payload_length);
        // reply is not malloced, we should release l here
        free(l);  // not BIO_F_MALLOCED, release it
    }

    return 0;
}

// clang-format off
static struct {
    unsigned int id;
    int (*transact)(struct binder_io *msg, struct binder_io *reply);
} _methods[] = {
    [HRIF_TRANSACT_CODE_WIRELESS_INIT & HRIF_TRANSACT_CODE_ID_MASK]             = {HRIF_TRANSACT_CODE_WIRELESS_INIT, _hrif_wireless_init},
    [HRIF_TRANSACT_CODE_WIRELESS_CAPABILITY & HRIF_TRANSACT_CODE_ID_MASK]       = {HRIF_TRANSACT_CODE_WIRELESS_CAPABILITY, _hrif_wireless_capability},
    [HRIF_TRANSACT_CODE_WIRELESS_RADIO_STATUS & HRIF_TRANSACT_CODE_ID_MASK]     = {HRIF_TRANSACT_CODE_WIRELESS_RADIO_STATUS, _hrif_wireless_radio_status},
    [HRIF_TRANSACT_CODE_WIRELESS_RADIO_GET & HRIF_TRANSACT_CODE_ID_MASK]        = {HRIF_TRANSACT_CODE_WIRELESS_RADIO_GET, _hrif_wireless_radio_get},
    [HRIF_TRANSACT_CODE_WIRELESS_RADIO_SET & HRIF_TRANSACT_CODE_ID_MASK]        = {HRIF_TRANSACT_CODE_WIRELESS_RADIO_SET, _hrif_wireless_radio_set},
    [HRIF_TRANSACT_CODE_WIRELESS_INTERFACE_GET & HRIF_TRANSACT_CODE_ID_MASK]    = {HRIF_TRANSACT_CODE_WIRELESS_INTERFACE_GET, _hrif_wireless_interface_get},
    [HRIF_TRANSACT_CODE_WIRELESS_INTERFACE_SET & HRIF_TRANSACT_CODE_ID_MASK]    = {HRIF_TRANSACT_CODE_WIRELESS_INTERFACE_SET, _hrif_wireless_interface_set},
    [HRIF_TRANSACT_CODE_WIRELESS_BANDSTEERING_GET & HRIF_TRANSACT_CODE_ID_MASK] = {HRIF_TRANSACT_CODE_WIRELESS_BANDSTEERING_GET, _hrif_wireless_bandsteering_get},
    [HRIF_TRANSACT_CODE_WIRELESS_BANDSTEERING_SET & HRIF_TRANSACT_CODE_ID_MASK] = {HRIF_TRANSACT_CODE_WIRELESS_BANDSTEERING_SET, _hrif_wireless_bandsteering_set},
    [HRIF_TRANSACT_CODE_WIRELESS_SCAN_LIST_ARRAY & HRIF_TRANSACT_CODE_ID_MASK]  = {HRIF_TRANSACT_CODE_WIRELESS_SCAN_LIST_ARRAY, _hrif_wireless_scan_list_array},
    [HRIF_TRANSACT_CODE_WIRELESS_SCAN_CONNECT & HRIF_TRANSACT_CODE_ID_MASK]     = {HRIF_TRANSACT_CODE_WIRELESS_SCAN_CONNECT, _hrif_wireless_scan_connect},
    [HRIF_TRANSACT_CODE_WIRELESS_SCAN_STATUS & HRIF_TRANSACT_CODE_ID_MASK]      = {HRIF_TRANSACT_CODE_WIRELESS_SCAN_STATUS, _hrif_wireless_scan_status},
    [HRIF_TRANSACT_CODE_WIRELESS_WPS_GET & HRIF_TRANSACT_CODE_ID_MASK]          = {HRIF_TRANSACT_CODE_WIRELESS_WPS_GET, _hrif_wireless_wps_get},
    [HRIF_TRANSACT_CODE_WIRELESS_WPS_SET & HRIF_TRANSACT_CODE_ID_MASK]          = {HRIF_TRANSACT_CODE_WIRELESS_WPS_SET, _hrif_wireless_wps_set},
    [HRIF_TRANSACT_CODE_WIRELESS_WPS_SWITCH_GET & HRIF_TRANSACT_CODE_ID_MASK]   = {HRIF_TRANSACT_CODE_WIRELESS_WPS_SWITCH_GET, _hrif_wireless_wps_switch_get},
    [HRIF_TRANSACT_CODE_WIRELESS_WPS_SWITCH_SET & HRIF_TRANSACT_CODE_ID_MASK]   = {HRIF_TRANSACT_CODE_WIRELESS_WPS_SWITCH_SET, _hrif_wireless_wps_switch_set},
    [HRIF_TRANSACT_CODE_WIRELESS_TIMER_SWITCH_GET & HRIF_TRANSACT_CODE_ID_MASK] = {HRIF_TRANSACT_CODE_WIRELESS_TIMER_SWITCH_GET, _hrif_wireless_timer_switch_get},
    [HRIF_TRANSACT_CODE_WIRELESS_TIMER_SWITCH_SET & HRIF_TRANSACT_CODE_ID_MASK] = {HRIF_TRANSACT_CODE_WIRELESS_TIMER_SWITCH_SET, _hrif_wireless_timer_switch_set},
    [HRIF_TRANSACT_CODE_WIRELESS_TIMER_ADD & HRIF_TRANSACT_CODE_ID_MASK]        = {HRIF_TRANSACT_CODE_WIRELESS_TIMER_ADD, _hrif_wireless_timer_add},
    [HRIF_TRANSACT_CODE_WIRELESS_TIMER_DEL & HRIF_TRANSACT_CODE_ID_MASK]        = {HRIF_TRANSACT_CODE_WIRELESS_TIMER_DEL, _hrif_wireless_timer_del},
    [HRIF_TRANSACT_CODE_WIRELESS_TIMER_MOD & HRIF_TRANSACT_CODE_ID_MASK]        = {HRIF_TRANSACT_CODE_WIRELESS_TIMER_MOD, _hrif_wireless_timer_mod},
    [HRIF_TRANSACT_CODE_WIRELESS_TIMER_ARRAY & HRIF_TRANSACT_CODE_ID_MASK]      = {HRIF_TRANSACT_CODE_WIRELESS_TIMER_ARRAY, _hrif_wireless_timer_array},
    [HRIF_TRANSACT_CODE_WIRELESS_DEASSOCIATE & HRIF_TRANSACT_CODE_ID_MASK]      = {HRIF_TRANSACT_CODE_WIRELESS_DEASSOCIATE, _hrif_wireless_deassociate},
    [HRIF_TRANSACT_CODE_WIRELESS_CHANNEL_SCAN & HRIF_TRANSACT_CODE_ID_MASK]     = {HRIF_TRANSACT_CODE_WIRELESS_CHANNEL_SCAN, _hrif_wireless_channel_scan},
    [HRIF_TRANSACT_CODE_WIRELESS_CHANNEL_SCORE_GET & HRIF_TRANSACT_CODE_ID_MASK]= {HRIF_TRANSACT_CODE_WIRELESS_CHANNEL_SCORE_GET, _hrif_wireless_channel_score_get},
    [HRIF_TRANSACT_CODE_WIRELESS_VSIE_GET & HRIF_TRANSACT_CODE_ID_MASK]         = {HRIF_TRANSACT_CODE_WIRELESS_VSIE_GET, _hrif_wireless_vsie_get},
    [HRIF_TRANSACT_CODE_WIRELESS_VSIE_BEACONTX_SET & HRIF_TRANSACT_CODE_ID_MASK]= {HRIF_TRANSACT_CODE_WIRELESS_VSIE_BEACONTX_SET, _hrif_wireless_vsie_beacontx_set},
    [HRIF_TRANSACT_CODE_WIRELESS_VSIE_BEACONTX_LST & HRIF_TRANSACT_CODE_ID_MASK]= {HRIF_TRANSACT_CODE_WIRELESS_VSIE_BEACONTX_LST, _hrif_wireless_vsie_beacontx_list},
    [HRIF_TRANSACT_CODE_WIRELESS_VSIE_PROBERX_SET & HRIF_TRANSACT_CODE_ID_MASK] = {HRIF_TRANSACT_CODE_WIRELESS_VSIE_PROBERX_SET, _hrif_wireless_vsie_proberx_set},
    [HRIF_TRANSACT_CODE_WIRELESS_VSIE_PROBERX_LST & HRIF_TRANSACT_CODE_ID_MASK] = {HRIF_TRANSACT_CODE_WIRELESS_VSIE_PROBERX_LST, _hrif_wireless_vsie_proberx_list},
};
// clang-format on
//
static int _on_transact(uint32_t code, struct binder_io *msg, struct binder_io *reply) {
    if ((code & HRIF_TRANSACT_CODE_CATEGORY_MASK) != HRIF_TRANSACT_CODE_WIRELESS_BASE) return -1;

    uint32_t id = code & ~HRIF_TRANSACT_CODE_CATEGORY_MASK;

    if (id > sizeof(_methods) / sizeof(_methods[0]) - 1) {
        HR_LOGE("id invalid ......\n");
        return -1;
    }

    if (!_methods[id].transact) {
        HR_LOGE("not support current transact code:%d\n", code);
        return -1;
    }

    if (_methods[id].id != code) {
        HR_LOGE("not support current transact code:0x%X(0x%X) your code maybe error!\n", code, _methods[id].id);
        return -1;
    }

    return _methods[id].transact(msg, reply);
}

int hrifd_wireless_init() {
    return hrifd_publish(SVC_NAME, _on_transact);
}
