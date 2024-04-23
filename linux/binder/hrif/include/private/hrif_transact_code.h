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

#ifndef _HRIF_TRANSACT_CODE_H_
#define _HRIF_TRANSACT_CODE_H_

// category    method
// xxxx xxxx   yyyy yyyy

#define HRIF_TRANSACT_CODE_CATEGORY_MASK 0xFFFFFF00
enum {
    HRIF_TRANSACT_CODE_SYSTEM_BASE = 0 << 8,
    HRIF_TRANSACT_CODE_NETWORK_BASE = 1 << 8,
    HRIF_TRANSACT_CODE_WIRELESS_BASE = 2 << 8
};

enum {
    HRIF_TRANSACT_CODE_SYSTEM_INIT = HRIF_TRANSACT_CODE_SYSTEM_BASE,

};

enum {
    HRIF_TRANSACT_CODE_NETWORK_INIT = HRIF_TRANSACT_CODE_NETWORK_BASE,
    HRIF_TRANSACT_CODE_NETWORK_WAN_SIZE,
    HRIF_TRANSACT_CODE_NETWORK_WAN_ARRAY,

};

#endif  //_HRIF_TRANSACT_CODE_H_
