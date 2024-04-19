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

#ifndef _HRIFD_NETWORK_H_
#define _HRIFD_NETWORK_H_

#include "hrifd.h"

enum {
    HRIFD_NETWORK_BASE = 0,
    HRIFD_NETWORK_INIT,
    HRIFD_NETWORK_WAN_SIZE
    HRIFD_WIRELESS_BASE = 0,
};

int hrifd_network_init();

#endif  //_HRIFD_NETWORK_H_
