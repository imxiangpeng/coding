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

#ifndef _HRIFD_H_
#define _HRIFD_H_

#include <stdint.h>
#include "hrbinder.h"

// client get data without any param, load data into pointer
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
        int result = setter((type *)ptr);                                  \
        bio_put_uint32(reply, result);                                     \
        return 0;                                                          \
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
        int result = setter(*ptr /*id*/, (type *)(ptr + 1) /*data*/);       \
        bio_put_uint32(reply, result);                                      \
        return 0;                                                           \
    }

typedef int (*hrifd_on_transact)(uint32_t code, struct binder_io *msg, struct binder_io *reply);

int hrifd_publish(const char *name, hrifd_on_transact on_transact);

int hrifd_system_init();
int hrifd_network_init();
int hrifd_wireless_init();
int hrifd_easymesh_init();
#endif  //_HRIFD_H_
