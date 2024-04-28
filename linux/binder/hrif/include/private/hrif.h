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

#include <stdint.h>

typedef void* (*hrif_transact_realloc)(void *, uint32_t);
// it's not good method, which leading memory copy ...
int hrif_transact(int method, void *data, uint32_t dsize, void *result, uint32_t *rsize);
// when result / rsize is not large enough, we will try realloc memory var realloc_fn
int hrif_transact2(int method, void *data, uint32_t dsize, void **result, uint32_t *rsize, hrif_transact_realloc realloc_fn);
#endif  //_HRIF_H_
