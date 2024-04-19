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

#include "hrif_network.h"

#include "hrifd_network.h"
#include "private/hrifd_network.h"

#include "hr_log.h"

#define SVC_NAME "hrifd.network"

static int _on_transact(struct binder_state *bs,
                        struct binder_transaction_data *txn,
                        struct binder_io *msg,
                        struct binder_io *reply) {

    HR_LOGD("%s(%d): come in code:%d\n", __FUNCTION__, __LINE__, txn->code);

    switch (txn->code) {
        case HRIFD_NETWORK_INIT:
            break;
        case HRIFD_NETWORK_WAN_SIZE:
            break;
        default:
            break;
    }
    return 0;
}

int hrifd_network_init() {
    return hrifd_publish(SVC_NAME, _on_transact);
}
