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

#ifndef _HRIF_H_
#define _HRIF_H_

#include "libubus.h"

enum {
    HRIF_METHOD_UNSPEC = 0,
    HRIF_METHOD_WAN_SIZE,
};
int hrif_init(void);

int hrif_request(struct blob_attr *msg, ubus_data_handler_t cb, void *priv);

#endif  //_HRIF_H_
