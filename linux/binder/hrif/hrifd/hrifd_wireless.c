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
#include <string.h>

#include "hrif_wireless.h"

#include "hrifd.h"

#include "private/hrif_transact_code.h"

#include "hr_log.h"

#define SVC_NAME "hrifd.wireless"

// client get data without any param
// data will be returned with follow format:
// 4: result
// 4: payload size
// -: payload data
// care that this method memory is limited
#define HRIF_STRUCT_GET(type, getter)                                                         \
    static int _##getter(struct binder_io *msg, struct binder_io *reply) {                    \
        (void)msg;                                                                            \
        if (!reply) return -1;                                                                \
        int *ptr = (int *)bio_alloc(reply, 4 + 4 + sizeof(type)); /*result + size + payload*/ \
        if (!ptr) {                                                                           \
            if (reply->flags & BIO_F_OVERFLOW) {                                              \
                HR_LOGE("buffer overflow ...\n");                                             \
            }                                                                                 \
            return -1;                                                                        \
        }                                                                                     \
        int result = getter((type *)(ptr + 2)); /*3. payload have been loade*/                \
        *ptr = result;                          /*1. write result*/                           \
        *(ptr + 1) = sizeof(type);              /*2. write payload size*/                     \
        return 0;                                                                             \
    }

// when client pass struct object, we use this macro to pass it to implement interface
// it get object from msg and call setter directly
#define HRIF_STRUCT_SET(type, setter)                                      \
    static int _##setter(struct binder_io *msg, struct binder_io *reply) { \
        if (!msg || !reply) return -1;                                     \
        int *ptr = (int *)bio_alloc(msg, sizeof(type));                    \
        if (!ptr) {                                                        \
            if (reply->flags & BIO_F_OVERFLOW) {                           \
                HR_LOGE("buffer overflow ...\n");                          \
            }                                                              \
            return -1;                                                     \
        }                                                                  \
        return setter((type *)ptr);                                        \
    }

#define HRIF_STRUCT_GET_WITH_ID(type, getter)                                                  \
    static int _##getter(struct binder_io *msg, struct binder_io *reply) {                     \
        if (!msg || !reply) return -1;                                                         \
        uint32_t id = bio_get_uint32(msg);                                                     \
        int *ptr = (int *)bio_alloc(reply, 4 + 4 + sizeof(type)); /* result + size + payload*/ \
        if (!ptr) {                                                                            \
            if (reply->flags & BIO_F_OVERFLOW) {                                               \
                HR_LOGE("buffer overflow ...\n");                                              \
            }                                                                                  \
            return -1;                                                                         \
        }                                                                                      \
        int result = getter(id, (type *)(ptr + 2)); /*3. payload have been loaded*/            \
        *ptr = result;                              /*1. write result*/                        \
        *(ptr + 1) = sizeof(type);                  /*2. write payload size*/                  \
        return 0;                                                                              \
    }

#define HRIF_STRUCT_SET_WITH_ID(type, setter)                               \
    static int _##setter(struct binder_io *msg, struct binder_io *reply) {  \
        if (!msg || !reply) return -1;                                      \
        int *ptr = (int *)bio_alloc(msg, 4 + sizeof(type)); /* id & ipv4 */ \
        if (!ptr) {                                                         \
            if (reply->flags & BIO_F_OVERFLOW) {                            \
                HR_LOGE("buffer overflow ...\n");                           \
            }                                                               \
            return -1;                                                      \
        }                                                                   \
        return setter(*ptr /*id*/, (type *)(ptr + 1) /*data*/);             \
    }

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
