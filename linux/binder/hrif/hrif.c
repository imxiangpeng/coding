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
#include "hrif.h"

int hrif_init(void) {
    return 0;
}

int hrif_transact(struct binder_io *msg, struct binder_io *reply) {
    int status;
    unsigned iodata[512 / 4];
    struct binder_io msg, reply;

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, "hrifd");
    // bio_put_ref(&msg, target);

    if (binder_call(bs, &msg, &reply, target, code)) {
        return -1;
    }

    status = bio_get_uint32(&reply);

    binder_done(bs, &msg, &reply);

    return status;
}
