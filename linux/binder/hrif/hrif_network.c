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

#include "hrif_network.h"

#include "hrbinder.h"

#include "private/hrif_transact_code.h"
#include "sys/types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>

static void *_realloc(void *ptr, uint32_t size) {
    return realloc(ptr, size);
}

int hrif_network_init() {
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_INIT, NULL, 0, NULL, NULL);
}

int hrif_network_wan_size(unsigned int *num) {
    *num = hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_SIZE, NULL, 0, NULL, NULL);
    return 0;
}

// using share memory for large data
int hrif_network_wan_array(hrif_wan_t *wans, uint32_t *size) {
    struct {
        key_t key;
        size_t max_size;
    } data;
    if (!wans || !size) return -1;

    uint32_t len = *size * sizeof(hrif_wan_t);

    // using wans pointer as id
    int sid = shmget((key_t)wans, len, IPC_CREAT | 0666);
    if (sid < 0) return -1;

    void *ptr = shmat(sid, 0, 0);
    if (ptr == (void *)-1) {
        printf("can not get shared memory ..........\n");
        shmctl(sid, IPC_RMID, NULL);
        return -1;
    }
    data.key = (key_t)wans;
    data.max_size = *size;

    *size = hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_ARRAY, (void *)&data, sizeof(data), NULL, NULL);

    memcpy((void *)wans, ptr, *size * sizeof(hrif_wan_t));

    shmdt(ptr);

    shmctl(sid, IPC_RMID, NULL);

    return 0;
}
int hrif_network_get_wan(int index, hrif_wan_t *wan) {
    uint32_t len = sizeof(hrif_wan_t);
    if (!wan) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_GET, (void *)&index, sizeof(index), (void *)wan, &len);
}

int hrif_network_wan_get_proto(int index, hrif_protocol_e *proto) {
    if (!proto) return -1;
    *proto = hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_GET_PROTO, (void *)&index, sizeof(index), NULL, NULL);
    return 0;
}

int hrif_network_wan_set_proto(int index, hrif_protocol_e proto) {
    int data[2] = {index, proto};
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_SET_PROTO, (void *)&data, sizeof(data), NULL, NULL);
}

int hrif_network_wan_get_ipv4(int id, hrif_ipv4_t *ipv4) {
    uint32_t len = sizeof(hrif_ipv4_t);
    if (!ipv4) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_GET_IPV4, (void *)&id, sizeof(id), (void *)ipv4, &len);
}

int hrif_network_wan_set_ipv4(int index, hrif_ipv4_t *ipv4) {
    if (!ipv4) return -1;
    struct {
        int id;
        hrif_ipv4_t ipv4;
    } data = {index, *ipv4};
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_SET_IPV4, (void *)&data, sizeof(data), NULL, NULL);
}

int hrif_network_wan_get_ipv6(int index, hrif_ipv6_t *ipv6) {
    (void)index;
    (void)ipv6;
    return 0;
}

int hrif_network_wan_set_ipv6(int index, hrif_ipv6_t *ipv6) {
    if (!ipv6) return -1;
    struct {
        int id;
        hrif_ipv6_t ipv6;
    } data = {index, *ipv6};
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_SET_IPV6, (void *)&data, sizeof(data), NULL, NULL);
}

int hrif_network_wan_get_pppoe(int index, hrif_pppoe_t *pppoe) {
    uint32_t len = sizeof(hrif_pppoe_t);
    if (!pppoe) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_GET_PPPOE, (void *)&index, sizeof(index), (void *)pppoe, &len);
}
int hrif_network_wan_set_pppoe(int index, hrif_pppoe_t *pppoe) {
    struct {
        int id;
        hrif_pppoe_t pppoe;
    } data = {index, *pppoe};
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_SET_PPPOE, (void *)&data, sizeof(data), NULL, NULL);
}

int hrif_network_wan_commit(int index) {
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_COMMIT, (void *)&index, sizeof(index), NULL, NULL);
}

int hrif_network_wan_discard(int index) {
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_DISCARD, (void *)&index, sizeof(index), NULL, NULL);
}

int hrif_network_wan_get_status(int index, hrif_wan_status_t *status) {
    uint32_t len = sizeof(hrif_wan_status_t);
    if (!status) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_STATUS, (void *)&index, sizeof(index), (void *)status, &len);
}
int hrif_network_wan_netstats(int index, hrif_netstats_t *st) {
    uint32_t len = sizeof(hrif_netstats_t);
    if (!st) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_WAN_NETSTATS, (void *)&index, sizeof(index), (void *)st, &len);
}

hrif_workmode_e hrif_network_get_workmode() {
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_GET_WORKMODE, NULL, 0, NULL, NULL);
}

int hrif_network_set_workmode(hrif_workmode_e workmode) {
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_SET_WORKMODE, (void *)&workmode, sizeof(workmode), NULL, NULL);
}

int hrif_network_lan_get(hrif_lan_t *lan) {
    uint32_t len = sizeof(hrif_lan_t);
    if (!lan) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_LAN_GET, NULL, 0, (void *)lan, &len);
}

int hrif_network_lan_set(hrif_lan_t *lan) {
    if (!lan) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_LAN_GET, (void *)lan, sizeof(hrif_lan_t), NULL, NULL);
}

int hrif_network_dhcp_get(hrif_dhcp_t *dhcp) {
    uint32_t len = sizeof(hrif_dhcp_t);
    if (!dhcp) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_DHCP_GET, NULL, 0, (void *)dhcp, &len);
}

int hrif_network_dhcp_set(hrif_dhcp_t *dhcp) {
    if (!dhcp) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_DHCP_SET, (void *)dhcp, sizeof(hrif_dhcp_t), NULL, NULL);
}

int hrif_network_lanhost_size(uint32_t *size) {
    if (!size) return -1;
    *size = hrif_transact(HRIF_TRANSACT_CODE_NETWORK_LANHOST_SIZE, NULL, 0, NULL, NULL);
    return 0;
}
int hrif_network_lanhost_array(hrif_lanhost_t *lanhosts, uint32_t *size) {
    struct {
        key_t key;
        size_t max_size;
    } data;
    if (!lanhosts || !size) return -1;

    uint32_t len = *size * sizeof(hrif_lanhost_t);

    // using lanhosts pointer as id
    int sid = shmget((key_t)lanhosts, len, IPC_CREAT | 0666);
    if (sid < 0) return -1;

    void *ptr = shmat(sid, 0, 0);
    if (ptr == (void *)-1) {
        printf("can not get shared memory ..........\n");
        shmctl(sid, IPC_RMID, NULL);
        return -1;
    }
    data.key = (key_t)lanhosts;
    data.max_size = *size;

    *size = hrif_transact(HRIF_TRANSACT_CODE_NETWORK_LANHOST_ARRAY, (void *)&data, sizeof(data), NULL, NULL);

    memcpy((void *)lanhosts, ptr, *size * sizeof(hrif_lanhost_t));

    shmdt(ptr);

    shmctl(sid, IPC_RMID, NULL);
    return 0;
}

int hrif_network_lanhost_online_size(uint32_t *size) {
    if (!size) return -1;
    *size = hrif_transact(HRIF_TRANSACT_CODE_NETWORK_LANHOST_ONLINE_SIZE, NULL, 0, NULL, NULL);
    return 0;
}

int hrif_network_lanhost_limitspeed_set(hrif_limitspeed_t *limit) {
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_LANHOST_LIMITSPEED_SET, (void *)limit, sizeof(hrif_limitspeed_t), NULL, NULL);
}

// using prealloc shared memory
// please release memory *limit when you passed *limit is not null
// *limit will be auto allocate when *limit == NULL
int hrif_network_limit_get(hrif_limit_e mode, hrif_limit_t **limit, uint32_t *size) {
    int result = 0;
    uint32_t len = 0;
    void *ptr = NULL;

    if (!limit || !size) return -1;

    result = hrif_transact2(HRIF_TRANSACT_CODE_NETWORK_LIMIT_GET, (void *)&mode, sizeof(mode), &ptr, &len, _realloc);
    if (result != 0) {
        return result;
    }

    *size = len / sizeof(hrif_limit_t);
    *limit = ptr;
    return result;
}
// we may pass null & 0 for clear limit table
int hrif_network_limit_set(hrif_limit_e mode, hrif_limit_t *limit, uint32_t size) {
    int result = 0;
    int sid = -1;
    void *ptr = NULL;
    struct {
        key_t key;
        hrif_limit_e mode;
        size_t max_size;
    } data;
    // if (!limit) return -1;

    uint32_t len = sizeof(hrif_limit_t) * size;

    if (len > 0) {
        // using method function address as id
        sid = shmget((key_t)limit, len, IPC_CREAT | 0666);
        if (sid < 0) return -1;

        ptr = shmat(sid, 0, 0);
        if (ptr == (void *)-1) {
            printf("can not get shared memory ..........\n");
            shmctl(sid, IPC_RMID, NULL);
            return -1;
        }
    }

    data.key = (key_t)limit;
    data.mode = mode;
    data.max_size = size;

    if (ptr) {
        memcpy((void *)ptr, limit, size * sizeof(hrif_limit_t));
    }

    result = hrif_transact(HRIF_TRANSACT_CODE_NETWORK_LIMIT_SET, (void *)&data, sizeof(data), NULL, NULL);
    if (result <= 0) {
        shmdt(ptr);
        shmctl(sid, IPC_RMID, NULL);
        return result;
    }

    if (ptr) {
        shmdt(ptr);
        shmctl(sid, IPC_RMID, NULL);
    }

    return result;
}
#if 0
int hrif_network_limit_del(hrif_limit_t *limit) {
    (void)limit;
    return 0;
}
#endif

int hrif_network_port_forwarding_add(hrif_port_forwarding_t *port_forwarding) {
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_ADD, (void *)port_forwarding, sizeof(hrif_port_forwarding_t), NULL, NULL);
}
int hrif_network_port_forwarding_mod(hrif_port_forwarding_t *port_forwarding) {
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_MOD, (void *)port_forwarding, sizeof(hrif_port_forwarding_t), NULL, NULL);
}

int hrif_network_port_forwarding_del(char *port_forwarding_indexs) {
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_DEL, (void *)port_forwarding_indexs, strlen(port_forwarding_indexs) + 1, NULL, NULL);
}

int hrif_network_port_forwarding_array(hrif_port_forwarding_t **list, uint32_t *size) {
    void *ptr = NULL;
    u_int32_t length = 0;
    if (!list || !size) return -1;
    int result = hrif_transact2(HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_ARRAY, (void *)NULL, 0, &ptr, &length, _realloc);
    if (result == 0) {
        *size = length / sizeof(hrif_port_forwarding_t);
        *list = ptr;
    }

    return result;
}

int hrif_network_dmz_get(hrif_dmz_t *dmz) {
    uint32_t len = sizeof(hrif_dmz_t);
    if (!dmz) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_DMZ_GET, NULL, 0, (void *)dmz, &len);
}

int hrif_network_dmz_set(hrif_dmz_t *dmz) {
    if (!dmz) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_DMZ_SET, (void *)dmz, sizeof(hrif_dmz_t), NULL, NULL);
}

int hrif_network_iptv_get(hrif_iptv_t *iptv) {
    uint32_t len = sizeof(hrif_iptv_t);
    if (!iptv) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_IPTV_GET, NULL, 0, (void *)iptv, &len);
}

int hrif_network_iptv_set(hrif_iptv_t *iptv) {
    if (!iptv) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_IPTV_SET, (void *)iptv, sizeof(hrif_iptv_t), NULL, NULL);
}

int hrif_network_dos_get(hrif_dos_t *data) {
    uint32_t len = sizeof(hrif_dos_t);
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_DOS_GET, NULL, 0, (void *)data, &len);
}
int hrif_network_dos_set(hrif_dos_t *data) {
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_DOS_SET, (void *)data, sizeof(hrif_dos_t), NULL, NULL);
}

int hrif_network_address_filter_get(hrif_address_filter_t *data) {
    uint32_t len = sizeof(hrif_address_filter_t);
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_ADDRESS_FILTER_GET, NULL, 0, (void *)data, &len);
}

int hrif_network_address_filter_set(hrif_address_filter_t *data) {
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_ADDRESS_FILTER_SET, (void *)data, sizeof(hrif_address_filter_t), NULL, NULL);
}

int hrif_network_port_filter_get(hrif_port_filter_t *data) {
    uint32_t len = sizeof(hrif_port_filter_t);
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_PORT_FILTER_GET, NULL, 0, (void *)data, &len);
    return 0;
}

int hrif_network_port_filter_set(hrif_port_filter_t *data) {
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_PORT_FILTER_SET, (void *)data, sizeof(hrif_port_filter_t), NULL, NULL);
    return 0;
}

int hrif_network_url_filter_get(hrif_url_filter_t *data) {
    uint32_t len = sizeof(hrif_url_filter_t);
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_URL_FILTER_GET, NULL, 0, (void *)data, &len);
    return 0;
}
int hrif_network_url_filter_set(hrif_url_filter_t *data) {
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_URL_FILTER_SET, (void *)data, sizeof(hrif_url_filter_t), NULL, NULL);
}

int hrif_network_filter_mode_get(hrif_filter_mode_t *data) {
    uint32_t len = sizeof(hrif_filter_mode_t);
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_FILTERMODE_GET, NULL, 0, (void *)data, &len);
}
int hrif_network_filter_mode_set(hrif_filter_mode_t *data) {
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_FILTERMODE_SET, (void *)data, sizeof(hrif_filter_mode_t), NULL, NULL);
    return 0;
}

int hrif_network_qos_get(hrif_qos_t *data) {
    uint32_t len = sizeof(hrif_qos_t);
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_QOS_GET, NULL, 0, (void *)data, &len);
}

int hrif_network_qos_set(hrif_qos_t *data) {
    if (!data) return -1;
    return hrif_transact(HRIF_TRANSACT_CODE_NETWORK_QOS_SET, (void *)data, sizeof(hrif_qos_t), NULL, NULL);
    return 0;
}