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

#include "hrif_system.h"

#include "private/hrif.h"

#include "hrbinder.h"

#include "private/hrif_transact_code.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int hrif_system_init() {
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_INIT, NULL, 0, NULL, NULL);
}

int hrif_system_board(hrif_board_t *board) {
    if (!board) return -1;
    uint32_t len = sizeof(hrif_board_t);
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_BOARD, NULL, 0, (void *)board, &len);
}

int hrif_system_storage(hrif_storage_t *storage) {
    if (!storage) return -1;
    uint32_t len = sizeof(hrif_storage_t);
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_STORAGE, NULL, 0, (void *)storage, &len);
}
int hrif_system_memory(hrif_memory_t *mem) {
    if (!mem) return -1;
    uint32_t len = sizeof(hrif_memory_t);
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_MEMORY, NULL, 0, (void *)mem, &len);
}

int hrif_system_uptime(hrif_uptime_t *uptime) {
    if (!uptime) return -1;
    uint32_t len = sizeof(hrif_uptime_t);
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_UPTIME, NULL, 0, (void *)uptime, &len);
}

int hrif_system_time_get(hrif_time_t *t) {
    if (!t) return -1;
    uint32_t len = sizeof(hrif_time_t);
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_TIME_GET, NULL, 0, (void *)t, &len);
}
int hrif_system_time_set(hrif_time_t *t) {
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_TIME_SET, (void *)t, sizeof(hrif_time_t), NULL, NULL);
}
int hrif_system_timezone_get() {
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_TIMEZONE_GET, NULL, 0, NULL, NULL);
}
int hrif_system_timezone_set(int timezone) {
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_TIME_SET, (void *)&timezone, sizeof(timezone), NULL, NULL);
}

int hrif_system_ntp_get(hrif_ntp_t *t) {
    uint32_t len = sizeof(hrif_ntp_t);
    if (!t) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_NTP_GET, NULL, 0, (void *)t, &len);
}
int hrif_system_ntp_set(hrif_ntp_t *t) {
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_NTP_SET, (void *)t, sizeof(hrif_ntp_t), NULL, NULL);
}

int hrif_system_cpu_jiffies(hrif_cpu_jiffies_t *jifs) {
    uint32_t len = sizeof(hrif_cpu_jiffies_t);
    if (!jifs) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_CPU_JIFFIES, NULL, 0, (void *)jifs, &len);
}

int hrif_system_reboot() {
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_REBOOT, NULL, 0, NULL, NULL);
}

int hrif_system_reset() {
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_RESET, NULL, 0, NULL, NULL);
}

int hrif_system_led_get(int *enable) {
    if (!enable) return -1;
    *enable = hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_LED_GET, NULL, 0, NULL, NULL);
    return 0;
}

int hrif_system_led_set(int enable) {
    return hrif_transact(HRIF_TRANSACT_CODE_SYSTEM_NTP_SET, (void *)&enable, sizeof(enable), NULL, NULL);
}
