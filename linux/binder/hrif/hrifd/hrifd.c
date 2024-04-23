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

#include "hrifd.h"

#define ALOGE(x...) fprintf(stderr, "hrifd: " x)

static struct binder_state *_bs = NULL;

static int hrifd_srv_handler(struct binder_state *bs,
                             struct binder_transaction_data *txn,
                             struct binder_io *msg,
                             struct binder_io *reply) {
    // ALOGE("hrifd handler : target=%p code=%d pid=%d uid=%d\n",
    //      txn->target.ptr, txn->code, txn->sender_pid, txn->sender_euid);
    switch (txn->code) {
        case PING_TRANSACTION:
            bio_put_uint32(reply, 0);
            break;
        default: {
            hrifd_on_transact on_transact = (hrifd_on_transact)txn->target.ptr;
            if (on_transact) {
                return on_transact(txn->code, msg, reply);
            }
            break;
        }
    }

    return 0;
}

int hrifd_publish(const char *name, hrifd_on_transact on_transact) {
    int status;
    unsigned iodata[512 / 4];
    struct binder_io msg, reply;

    if (!name) return -1;

    if (!_bs) {
        ALOGE("binder maybe not initialized ...\n");
        return -1;
    }

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, SVC_MGR_NAME);
    bio_put_string16_x(&msg, name);
    bio_put_obj(&msg, (void *)on_transact);  // obj
    bio_put_uint32(&msg, 1);                 // allow isolate
    bio_put_uint32(&msg, 0 /*1 << 3*/);      // dumpsys priority

    if (binder_call(_bs, &msg, &reply, BINDER_SERVICE_MANAGER, SVC_MGR_ADD_SERVICE)) {
        ALOGE("add service failed ...\n");
        return -1;
    }

    status = bio_get_uint32(&reply);

    binder_done(_bs, &msg, &reply);

    return status;
}

// link to death

#if 0
status_t IPCThreadState::requestDeathNotification(int32_t handle, BpBinder* proxy)
{
    mOut.writeInt32(BC_REQUEST_DEATH_NOTIFICATION);
    mOut.writeInt32((int32_t)handle);
    mOut.writeInt32((int32_t)proxy);
    return NO_ERROR;
}

status_t IPCThreadState::clearDeathNotification(int32_t handle, BpBinder* proxy)
{
    mOut.writeInt32(BC_CLEAR_DEATH_NOTIFICATION);
    mOut.writeInt32((int32_t)handle);
    mOut.writeInt32((int32_t)proxy);
    return NO_ERROR;
}
#endif

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // adjust output line buffered mode
    setvbuf(stdout, NULL, _IOLBF, 0);

    _bs = binder_open("/dev/binderfs/binder", 128 * 1024);
    if (!_bs) {
        return -1;
    }

    // binder_set_maxthreads(_bs, 2);

    hrifd_network_init();

    binder_loop(_bs, hrifd_srv_handler);
    return 0;
}
