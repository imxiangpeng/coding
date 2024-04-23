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

#include <stdio.h>
#include <string.h>

#include "hrif_network.h"

#include "hrifd.h"

#include "private/hrif_transact_code.h"

#include "hr_log.h"

#define SVC_NAME "hrifd.network"

static int _hrif_network_init(uint32_t code,
                              struct binder_io *msg,
                              struct binder_io *reply) {
    (void)msg;

    hrif_network_init();
    bio_put_uint32(reply, 0);
    return 0;
}

static int _hrif_network_wan_size(uint32_t code,
                                  struct binder_io *msg,
                                  struct binder_io *reply) {
    int num = 0;
    hrif_network_wan_size(&num);
    bio_put_uint32(reply, num);
    return 0;
}

static int _hrif_network_wan_array(uint32_t code,
                                   struct binder_io *msg,
                                   struct binder_io *reply) {
    (void)msg;
    unsigned int size = 0;
    hrif_wan_t *wans = NULL;

    int ret = hrif_network_wan_size(&size);
    if (ret != 0 || size == 0) {
        HR_LOGD("can not get wan size ...\n");
        return -1;
    }

    wans = (hrif_wan_t *)calloc(size, sizeof(hrif_wan_t));
    if (!wans) {
        return -1;
    }

    if (0 != hrif_network_wan_array(wans, &size)) {
        free(wans);
        return -1;
    }

    // 1. write result
    bio_put_uint32(reply, 0);  // result
    // 2. write payload size
    bio_put_uint32(reply, size * sizeof(hrif_wan_t));
    // 3. write payload data
    void *ptr = bio_alloc(reply, size * sizeof(hrif_wan_t));
    if (!ptr) {
        if (reply->flags & BIO_F_OVERFLOW) {
            HR_LOGE(" buffer overflow ...\n");
        }
        free(wans);
        return -1;
    }
    memcpy(ptr, (void *)wans, size * sizeof(hrif_wan_t));

    free(wans);

    return 0;
}

static struct {
    unsigned int id;
    hrifd_on_transact transact;
} _methods[] = {
    {HRIF_TRANSACT_CODE_NETWORK_INIT, _hrif_network_init},
    {HRIF_TRANSACT_CODE_NETWORK_WAN_SIZE, _hrif_network_wan_size},
    {HRIF_TRANSACT_CODE_NETWORK_WAN_ARRAY, _hrif_network_wan_array},
};

static int _on_transact(uint32_t code,
                        struct binder_io *msg,
                        struct binder_io *reply) {

    if (code & HRIF_TRANSACT_CODE_CATEGORY_MASK != HRIF_TRANSACT_CODE_CATEGORY_MASK) return -1;

    int id = code & ~HRIF_TRANSACT_CODE_CATEGORY_MASK;

    if (id > sizeof(_methods) / sizeof(_methods[0]) - 1) {
        HR_LOGE("id invalid ......\n");
        return -1;
    }

    if (!_methods[id].transact) {
        HR_LOGE("not support current transact code:%d\n", code);
        return -1;
    }

    if (_methods[id].id != code) {
        HR_LOGE("not support current transact code:%d your code maybe error!\n", code);
        return -1;
    }

    return _methods[id].transact(code, msg, reply);
}

int hrifd_network_init() {
    return hrifd_publish(SVC_NAME, _on_transact);
}
