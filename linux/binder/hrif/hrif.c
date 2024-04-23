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

#include "private/hrif.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "private/hrif_transact_code.h"
#include "hrbinder.h"

static struct binder_state *_bs = NULL;

static struct _svc {
    int id;
    const char *name;
    uint32_t target;
} _svcs[] = {
    {HRIF_TRANSACT_CODE_SYSTEM_BASE, "hrifd.system", 0},
    {HRIF_TRANSACT_CODE_NETWORK_BASE, "hrifd.network", 0},
    {HRIF_TRANSACT_CODE_WIRELESS_BASE, "hrifd.wireless", 0},
    {-1, NULL, 0}};

#if 0
static void _svc_death_cb(struct binder_state *bs, void *ptr) {
    struct _svc *s = (struct _svc *)ptr;
    printf("%s(%d): service '%s(%d)' died\n", __FUNCTION__, __LINE__, s->name, s->id);
    if (s->target > 0) {
        binder_release(bs, s->target);
        s->target = 0;
    }
}
#endif

static uint32_t _lookup(const char *name) {
    uint32_t handle;
    unsigned iodata[512 / 4];
    struct binder_io msg, reply;

    if (!name) return -1;

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, SVC_MGR_NAME);
    bio_put_string16_x(&msg, name);

    printf("%s(%d): look up :%s\n", __FUNCTION__, __LINE__, name);
    if (binder_call(_bs, &msg, &reply, BINDER_SERVICE_MANAGER, SVC_MGR_CHECK_SERVICE))
        return 0;

    handle = bio_get_ref(&reply);
    if (handle != 0)
        binder_acquire(_bs, handle);

    binder_done(_bs, &msg, &reply);

    return handle;
}

static int _init(void) {
    if (!_bs) {
        _bs = binder_open("/dev/binderfs/binder", 128 * 1024);
    }

    return _bs ? 0 : -1;
}
int hrif_init(void) {
    return _init();
}

// 0 - 3bits: result code
// 4 - 7bits: result payload size
// 8 - -    : result payload data
int hrif_transact(int method, void *data, uint32_t dsize, void *result, uint32_t *rsize) {
    // int hrif_transact(int method, struct binder_io *msg1, struct binder_io *reply1) {
    int id = -1;
    int status;
    unsigned iodata[512 / 4] = {0};
    struct binder_io msg, reply;

    memset((void *)&msg, 0, sizeof(msg));
    memset((void *)&reply, 0, sizeof(reply));

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header

    if (!_bs) {
        _init();
    }

    // using fast api
    id = (method & HRIF_TRANSACT_CODE_CATEGORY_MASK) >> 8;

    if (id > sizeof(_svcs) / sizeof(_svcs[0]) - 1 || (method & HRIF_TRANSACT_CODE_CATEGORY_MASK) != _svcs[id].id) {
        printf("invalid method:0x%X ... not support ...\n", method);
        return -1;
    }

    if (_svcs[id].target == 0) {
        _svcs[id].target = _lookup(_svcs[id].name);
        printf("lookup(%d):%s -> 0x%X\n", id, _svcs[id].name, _svcs[id].target);
#if 0
        struct binder_death death;
        death.func = (void *)_svc_death_cb;
        death.ptr = _svcs + id;
        binder_link_to_death(_bs, _svcs[id].target, &death);
#endif
    }

    if (id < 0 || _svcs[id].target == 0) {
        printf("can not found valid service: id:%d, target:%d\n", id, _svcs[id].target);
        return -1;
    }

    if (data && dsize > 0) {
        void *ptr = bio_alloc(&msg, dsize);
        if (!ptr) {
            if (msg.flags & BIO_F_OVERFLOW) {
                printf(" buffer overflow ...\n");
                return -1;
            }
            return -1;
        }
        memcpy(ptr, (void *)data, dsize);
    }
    if (!result || rsize == 0) {
        // current not support one way, because binder_call limited
    }

    if (binder_call(_bs, &msg, &reply, _svcs[id].target, method)) {
        // printf("%s(%d):can not call service: %d method:%d\n", __FUNCTION__, __LINE__, _svcs[id].target, method);
        binder_release(_bs, _svcs[id].target);
        _svcs[id].target = 0;
        return -1;
    }

    status = bio_get_uint32(&reply);

    if (result && rsize && reply.data_avail > 0) {
        uint32_t len = bio_get_uint32(&reply);
        // assert(len <= *rsize);
        if (*rsize < len) {
            binder_done(_bs, &msg, &reply);
            return -1;
        }
        *rsize = len;
        void *ptr = bio_get(&reply, len);
        if (!ptr) {
            binder_done(_bs, &msg, &reply);
            printf("%s(%d): can not get result ...\n", __FUNCTION__, __LINE__);
            return -1;
        }
        memcpy(result, ptr, len);
    }

    binder_done(_bs, &msg, &reply);

    return status;
}
