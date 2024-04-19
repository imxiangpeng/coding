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

#include "hrbinder.h"

typedef int (*hrifd_on_transact)(struct binder_state *bs,
                               struct binder_transaction_data *txn,
                               struct binder_io *msg,
                               struct binder_io *reply);

int hrifd_publish(const char *name, hrifd_on_transact on_transact);

#endif  //_HRIFD_H_
