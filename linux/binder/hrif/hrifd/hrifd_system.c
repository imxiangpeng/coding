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

#include "hrif_system.h"

#include "hrifd.h"

#include "private/hrif_transact_code.h"

#include "hr_log.h"

#define SVC_NAME "hrifd.system"

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

static int _hrif_system_init(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;

    hrif_system_init();
    bio_put_uint32(reply, 0);
    return 0;
}

HRIF_STRUCT_GET(hrif_board_t, hrif_system_board)
HRIF_STRUCT_GET(hrif_storage_t, hrif_system_storage)
HRIF_STRUCT_GET(hrif_memory_t, hrif_system_memory)
HRIF_STRUCT_GET(hrif_uptime_t, hrif_system_uptime)
HRIF_STRUCT_GET(hrif_time_t, hrif_system_time_get)
HRIF_STRUCT_SET(hrif_time_t, hrif_system_time_set)

static int _hrif_system_timezone_get(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    int tz = hrif_system_timezone_get();
    // 1. write result, direct  pass tz using result code
    bio_put_uint32(reply, tz);

    return 0;
}

static int _hrif_system_timezone_set(struct binder_io *msg, struct binder_io *reply) {
    int tz = bio_get_uint32(msg);
    int result = hrif_system_timezone_set(tz);
    // 1. write result, direct  pass tz using result code
    bio_put_uint32(reply, result);
    return 0;
}

HRIF_STRUCT_GET(hrif_ntp_t, hrif_system_ntp_get)
HRIF_STRUCT_SET(hrif_ntp_t, hrif_system_ntp_set)

HRIF_STRUCT_GET(hrif_cpu_jiffies_t, hrif_system_cpu_jiffies)

static int _hrif_system_reboot(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    (void)reply;
    return hrif_system_reboot();
}

static int _hrif_system_reset(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    (void)reply;
    return hrif_system_reset();
}

static int _hrif_system_led_get(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    int en = 0;
    hrif_system_led_get(&en);
    // 1. write result, direct  pass en using result code
    bio_put_uint32(reply, en);
    return 0;
}

static int _hrif_system_led_set(struct binder_io *msg, struct binder_io *reply) {
    int en = bio_get_uint32(msg);
    int result = hrif_system_led_set(en);
    bio_put_uint32(reply, result);
    return 0;
}

// clang-format off
static struct {
    unsigned int id;
    int (*transact)(struct binder_io *msg, struct binder_io *reply);
} _methods[] = {
    // force special index, avoiding someone write error! which may leading all method failed
    [HRIF_TRANSACT_CODE_SYSTEM_INIT & HRIF_TRANSACT_CODE_ID_MASK]                     = {HRIF_TRANSACT_CODE_SYSTEM_INIT, _hrif_system_init},
    [HRIF_TRANSACT_CODE_SYSTEM_BOARD & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_SYSTEM_BOARD, _hrif_system_board},
    [HRIF_TRANSACT_CODE_SYSTEM_STORAGE & HRIF_TRANSACT_CODE_ID_MASK]                  = {HRIF_TRANSACT_CODE_SYSTEM_STORAGE, _hrif_system_storage},
    [HRIF_TRANSACT_CODE_SYSTEM_MEMORY & HRIF_TRANSACT_CODE_ID_MASK]                   = {HRIF_TRANSACT_CODE_SYSTEM_MEMORY, _hrif_system_memory},
    [HRIF_TRANSACT_CODE_SYSTEM_UPTIME & HRIF_TRANSACT_CODE_ID_MASK]                   = {HRIF_TRANSACT_CODE_SYSTEM_UPTIME, _hrif_system_uptime},
    [HRIF_TRANSACT_CODE_SYSTEM_TIME_GET & HRIF_TRANSACT_CODE_ID_MASK]                 = {HRIF_TRANSACT_CODE_SYSTEM_TIME_GET, _hrif_system_time_get},
    [HRIF_TRANSACT_CODE_SYSTEM_TIME_SET & HRIF_TRANSACT_CODE_ID_MASK]                 = {HRIF_TRANSACT_CODE_SYSTEM_TIME_SET, _hrif_system_time_set},
    [HRIF_TRANSACT_CODE_SYSTEM_TIMEZONE_GET & HRIF_TRANSACT_CODE_ID_MASK]             = {HRIF_TRANSACT_CODE_SYSTEM_TIMEZONE_GET, _hrif_system_timezone_get},
    [HRIF_TRANSACT_CODE_SYSTEM_TIMEZONE_SET & HRIF_TRANSACT_CODE_ID_MASK]             = {HRIF_TRANSACT_CODE_SYSTEM_TIMEZONE_SET, _hrif_system_timezone_set},
    [HRIF_TRANSACT_CODE_SYSTEM_NTP_GET & HRIF_TRANSACT_CODE_ID_MASK]                  = {HRIF_TRANSACT_CODE_SYSTEM_NTP_GET, _hrif_system_ntp_get},
    [HRIF_TRANSACT_CODE_SYSTEM_NTP_SET & HRIF_TRANSACT_CODE_ID_MASK]                  = {HRIF_TRANSACT_CODE_SYSTEM_NTP_SET, _hrif_system_ntp_set},
    [HRIF_TRANSACT_CODE_SYSTEM_CPU_JIFFIES & HRIF_TRANSACT_CODE_ID_MASK]              = {HRIF_TRANSACT_CODE_SYSTEM_CPU_JIFFIES, _hrif_system_cpu_jiffies},
    [HRIF_TRANSACT_CODE_SYSTEM_REBOOT & HRIF_TRANSACT_CODE_ID_MASK]                   = {HRIF_TRANSACT_CODE_SYSTEM_REBOOT, _hrif_system_reboot},
    [HRIF_TRANSACT_CODE_SYSTEM_RESET & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_SYSTEM_RESET, _hrif_system_reset},
    [HRIF_TRANSACT_CODE_SYSTEM_LED_GET & HRIF_TRANSACT_CODE_ID_MASK]                  = {HRIF_TRANSACT_CODE_SYSTEM_LED_GET, _hrif_system_led_get},
    [HRIF_TRANSACT_CODE_SYSTEM_LED_SET & HRIF_TRANSACT_CODE_ID_MASK]                  = {HRIF_TRANSACT_CODE_SYSTEM_LED_SET, _hrif_system_led_set},
};
// clang-format on

static int _on_transact(uint32_t code, struct binder_io *msg, struct binder_io *reply) {
    if ((code & HRIF_TRANSACT_CODE_CATEGORY_MASK) != HRIF_TRANSACT_CODE_SYSTEM_BASE) return -1;

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

int hrifd_system_init() {
    return hrifd_publish(SVC_NAME, _on_transact);
}
