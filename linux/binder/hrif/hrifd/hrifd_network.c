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

// mxp, 20240423 implement binder service
// binder buffer is limited at 1k,
// you should use shared memory when data is large!

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>

#include "hrif_network.h"

#include "hrifd.h"

#include "private/hrif_transact_code.h"

#include "hr_log.h"
#include "sys/types.h"

#define SVC_NAME "hrifd.network"

// client get data without any param
// data will be returned with follow format:
// 4: result
// 4: payload size
// -: payload data
// care that this method memory is limited
#define HRIF_STRUCT_GET(type, getter)                                                         \
    static int _##getter(struct binder_io *msg, struct binder_io *reply) {                    \
        (void)msg;                                                                            \
        if (!reply) return -1;                                                                \
        int *ptr = (int *)bio_alloc(reply, 4 + 4 + sizeof(type)); /*result + size + payload*/ \
        if (!ptr) {                                                                           \
            if (reply->flags & BIO_F_OVERFLOW) {                                              \
                HR_LOGE("buffer overflow ...\n");                                             \
            }                                                                                 \
            return -1;                                                                        \
        }                                                                                     \
        int result = getter((type *)(ptr + 2)); /*3. payload have been loade*/                \
        *ptr = result;                          /*1. write result*/                           \
        *(ptr + 1) = sizeof(type);              /*2. write payload size*/                     \
        return 0;                                                                             \
    }

// when client pass struct object, we use this macro to pass it to implement interface
// it get object from msg and call setter directly
#define HRIF_STRUCT_SET(type, setter)                                      \
    static int _##setter(struct binder_io *msg, struct binder_io *reply) { \
        if (!msg || !reply) return -1;                                     \
        int *ptr = (int *)bio_alloc(msg, sizeof(type));                    \
        if (!ptr) {                                                        \
            if (reply->flags & BIO_F_OVERFLOW) {                           \
                HR_LOGE("buffer overflow ...\n");                          \
            }                                                              \
            return -1;                                                     \
        }                                                                  \
        return setter((type *)ptr);                                        \
    }

#define HRIF_STRUCT_GET_WITH_ID(type, getter)                                                  \
    static int _##getter(struct binder_io *msg, struct binder_io *reply) {                     \
        if (!msg || !reply) return -1;                                                         \
        uint32_t id = bio_get_uint32(msg);                                                     \
        int *ptr = (int *)bio_alloc(reply, 4 + 4 + sizeof(type)); /* result + size + payload*/ \
        if (!ptr) {                                                                            \
            if (reply->flags & BIO_F_OVERFLOW) {                                               \
                HR_LOGE("buffer overflow ...\n");                                              \
            }                                                                                  \
            return -1;                                                                         \
        }                                                                                      \
        int result = getter(id, (type *)(ptr + 2)); /*3. payload have been loaded*/            \
        *ptr = result;                              /*1. write result*/                        \
        *(ptr + 1) = sizeof(type);                  /*2. write payload size*/                  \
        return 0;                                                                              \
    }

#define HRIF_STRUCT_SET_WITH_ID(type, setter)                               \
    static int _##setter(struct binder_io *msg, struct binder_io *reply) {  \
        if (!msg || !reply) return -1;                                      \
        int *ptr = (int *)bio_alloc(msg, 4 + sizeof(type)); /* id & ipv4 */ \
        if (!ptr) {                                                         \
            if (reply->flags & BIO_F_OVERFLOW) {                            \
                HR_LOGE("buffer overflow ...\n");                           \
            }                                                               \
            return -1;                                                      \
        }                                                                   \
        return setter(*ptr /*id*/, (type *)(ptr + 1) /*data*/);             \
    }

static int _hrif_network_init(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;

    hrif_network_init();
    bio_put_uint32(reply, 0);
    return 0;
}

static int _hrif_network_wan_size(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    int num = 0;
    hrif_network_wan_size(&num);
    bio_put_uint32(reply, num);
    return 0;
}

static int _hrif_network_wan_array(struct binder_io *msg, struct binder_io *reply) {
    struct _data {
        key_t key;
        size_t max_size;
    };
    int sid = -1;
    hrif_wan_t *wan = NULL;
    uint32_t size = 0;

    struct _data *data = (struct _data *)bio_alloc(msg, sizeof(struct _data));  // result + payload size + payload data
    if (!data) {
        if (msg->flags & BIO_F_OVERFLOW) {
            HR_LOGE("buffer overflow ...\n");
        }
        return -1;
    }

    sid = shmget(data->key, 0, 0);
    if (sid < 0) {
        return -1;
    }
    wan = (hrif_wan_t *)shmat(sid, 0, 0);
    if (wan == (void *)-1) {
        printf("can not access memory ...\n");
        shmctl(sid, IPC_RMID, NULL);
        return -1;
    }

    size = data->max_size;

    if (0 != hrif_network_wan_array(wan, &size)) {
        shmdt(wan);
        shmctl(sid, IPC_RMID, NULL);
        return -1;
    }

    bio_put_uint32(reply, size);

    shmdt(wan);
    shmctl(sid, IPC_RMID, NULL);

    return 0;
}

HRIF_STRUCT_GET_WITH_ID(hrif_wan_t, hrif_network_get_wan)

static int _hrif_network_wan_get_proto(struct binder_io *msg, struct binder_io *reply) {
    hrif_protocol_e proto = HRIF_PROTO_NONE;
    uint32_t id = bio_get_uint32(msg);

    hrif_network_wan_get_proto(id, &proto);
    // 1. write result, direct  pass proto using result code
    bio_put_uint32(reply, proto);

    return 0;
}

static int _hrif_network_wan_set_proto(struct binder_io *msg, struct binder_io *reply) {
    uint32_t id = bio_get_uint32(msg);
    hrif_protocol_e proto = bio_get_uint32(msg);
    bio_put_uint32(reply, hrif_network_wan_set_proto(id, proto));
    return 0;
}

HRIF_STRUCT_GET_WITH_ID(hrif_ipv4_t, hrif_network_wan_get_ipv4)
HRIF_STRUCT_SET_WITH_ID(hrif_ipv4_t, hrif_network_wan_set_ipv4)

static int _hrif_network_wan_get_ipv6(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    (void)reply;
    return 0;
}

HRIF_STRUCT_SET_WITH_ID(hrif_ipv6_t, hrif_network_wan_set_ipv6)

HRIF_STRUCT_GET_WITH_ID(hrif_pppoe_t, hrif_network_wan_get_pppoe)
HRIF_STRUCT_SET_WITH_ID(hrif_pppoe_t, hrif_network_wan_set_pppoe)

static int _hrif_network_wan_commit(struct binder_io *msg, struct binder_io *reply) {
    uint32_t id = bio_get_uint32(msg);

    bio_put_uint32(reply, hrif_network_wan_commit(id));
    return 0;
}

static int _hrif_network_wan_discard(struct binder_io *msg, struct binder_io *reply) {
    uint32_t id = bio_get_uint32(msg);

    bio_put_uint32(reply, hrif_network_wan_discard(id));
    return 0;
}
HRIF_STRUCT_GET_WITH_ID(hrif_wan_status_t, hrif_network_wan_get_status)
HRIF_STRUCT_GET_WITH_ID(hrif_netstats_t, hrif_network_wan_netstats)

static int _hrif_network_get_workmode(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    hrif_workmode_e mode = hrif_network_get_workmode();
    bio_put_uint32(reply, mode);
    return 0;
}

static int _hrif_network_set_workmode(struct binder_io *msg, struct binder_io *reply) {
    hrif_workmode_e mode = bio_get_uint32(msg);

    int result = hrif_network_set_workmode(mode);

    bio_put_uint32(reply, result);
    return 0;
}

HRIF_STRUCT_GET(hrif_lan_t, hrif_network_lan_get)
HRIF_STRUCT_SET(hrif_lan_t, hrif_network_lan_set)

HRIF_STRUCT_GET(hrif_dhcp_t, hrif_network_dhcp_get)
HRIF_STRUCT_SET(hrif_dhcp_t, hrif_network_dhcp_set)

static int _hrif_network_lanhost_size(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    uint32_t size = 0;
    hrif_network_lanhost_size(&size);
    bio_put_uint32(reply, size);
    return 0;
}
static int _hrif_network_lanhost_array(struct binder_io *msg, struct binder_io *reply) {
    struct _data {
        key_t key;
        size_t max_size;
    };
    int sid = -1;
    hrif_lanhost_t *lh = NULL;
    uint32_t size = 0;

    struct _data *data = (struct _data *)bio_alloc(msg, sizeof(struct _data));  // result + payload size + payload data
    if (!data) {
        if (msg->flags & BIO_F_OVERFLOW) {
            HR_LOGE("buffer overflow ...\n");
        }
        return -1;
    }

    sid = shmget(data->key, 0, 0);
    if (sid < 0) {
        return -1;
    }
    lh = (hrif_lanhost_t *)shmat(sid, 0, 0);
    if (lh == (void *)-1) {
        printf("can not access memory ...\n");
        shmctl(sid, IPC_RMID, NULL);
        return -1;
    }

    size = data->max_size;

    if (0 != hrif_network_lanhost_array(lh, &size)) {
        printf("%s(%d): can not get lanhost ....\n", __FUNCTION__, __LINE__);
        shmdt(lh);
        shmctl(sid, IPC_RMID, NULL);
        return -1;
    }

    bio_put_uint32(reply, size);

    shmdt(lh);
    shmctl(sid, IPC_RMID, NULL);

    return 0;
}

static int _hrif_network_lanhost_online_size(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    uint32_t size = 0;
    hrif_network_lanhost_online_size(&size);
    bio_put_uint32(reply, size);
    return 0;
}

HRIF_STRUCT_SET(hrif_limitspeed_t, hrif_network_lanhost_limitspeed_set)

static int _hrif_network_limit_get(struct binder_io *msg, struct binder_io *reply) {
    uint32_t payload_length = 0;
    void *ptr = NULL;
    // we should reinit memory
    hrif_limit_t *lt = NULL;
    uint32_t size = 0;

    if (!msg || !reply) return -1;
    hrif_limit_e mode = bio_get_uint32(msg);

    int result = hrif_network_limit_get(mode, &lt, &size);
    if (result != 0 || !lt) {
        bio_put_uint32(reply, result);
        return -1;
    }

    payload_length = sizeof(hrif_limit_t) * size;
    // using our dynamic memory when it's large than reply default size
    if (payload_length + 4 + 4 > reply->data_avail) {
        // bio not use maxoffset, you should add it if you use it
        ptr = realloc(lt, payload_length + 4 + 4);  // result + result size + payload
        if (!ptr) {
            bio_put_uint32(reply, -1);
            return -1;
        }

        // skip result + payload length
        memmove((void *)((char *)ptr + 4 + 4), ptr, payload_length);
        // ptr will be freed auto in BR_TRANSACTION
        bio_init_with_prealloced(reply, ptr, payload_length + 4 + 4, 0);
    }
    // must call to alloc memory
    int *data = (int *)bio_alloc(reply, payload_length + 4 + 4);
    if (!data) {
        printf("not enough ..........\n");
        return -1;
    }
    *data = 0;
    *(data + 1) = payload_length;
    // not using dynamic memory, we should copy it manual
    if (!ptr) {
        memcpy((void *)(data + 2), lt, payload_length);
    }

    return 0;
}

static int _hrif_network_limit_set(struct binder_io *msg, struct binder_io *reply) {
    struct _data {
        key_t key;
        hrif_limit_e mode;
        size_t max_size;
    };
    int sid = -1;
    hrif_limit_t *lt = NULL;

    struct _data *data = (struct _data *)bio_alloc(msg, sizeof(struct _data));  // result + payload size + payload data
    if (!data) {
        if (msg->flags & BIO_F_OVERFLOW) {
            HR_LOGE("buffer overflow ...\n");
        }
        return -1;
    }

    if (data->max_size > 0) {
        sid = shmget(data->key, 0, 0);
        if (sid < 0) {
            return -1;
        }
        lt = (hrif_limit_t *)shmat(sid, 0, 0);
        if (lt == (void *)-1) {
            shmctl(sid, IPC_RMID, NULL);
            printf("can not access memory ...\n");
            return -1;
        }
    }

    int result = hrif_network_limit_set(data->mode, lt, data->max_size);

    bio_put_uint32(reply, result);

    if (lt) {
        shmdt(lt);
        shmctl(sid, IPC_RMID, NULL);
    }

    return 0;
}
#if 0
static int _hrif_network_limit_del(struct binder_io *msg, struct binder_io *reply) {
    (void)msg;
    (void)reply;
    return 0;
}
#endif
HRIF_STRUCT_SET(hrif_port_forwarding_t, hrif_network_port_forwarding_add)
HRIF_STRUCT_SET(hrif_port_forwarding_t, hrif_network_port_forwarding_mod)

static int _hrif_network_port_forwarding_del(struct binder_io *msg, struct binder_io *reply) {
    if (!msg || !reply) return -1;

    // directly access raw data
    char *ptr = (char *)msg->data;
    if (!ptr) return -1;

    return hrif_network_port_forwarding_del(ptr);
}

static int _hrif_network_port_forwarding_array(struct binder_io *msg, struct binder_io *reply) {
    uint32_t size = 0;
    uint32_t payload_length = 0;
    void *ptr = NULL;
    if (!msg || !reply) return -1;
    hrif_port_forwarding_t *pf = NULL;
    // we should reinit memory

    if (0 != hrif_network_port_forwarding_array(&pf, &size)) {
        bio_put_uint32(reply, -1);
        return -1;
    }
    
    if (size == 0) {
        bio_put_uint32(reply, 0);
        return 0;
    }

    payload_length = sizeof(hrif_port_forwarding_t) * size;
    // using our dynamic memory when it's large than reply default size
    if (payload_length + 4 + 4 > reply->data_avail) {
        // bio not use maxoffset, you should add it if you use it
        ptr = realloc(pf, payload_length + 4 + 4);  // result + result size + payload
        if (!ptr) {
            bio_put_uint32(reply, -1);
            return -1;
        }

        // skip result + payload length
        memmove((void *)((char *)ptr + 4 + 4), ptr, payload_length);
        // ptr will be freed auto in BR_TRANSACTION
        bio_init_with_prealloced(reply, ptr, payload_length + 4 + 4, 0);
    }
    // must call to alloc memory
    int *data = (int *)bio_alloc(reply, payload_length + 4 + 4);
    if (!data) {
        printf("not enough ..........\n");
        return -1;
    }

    *data = 0;                     // result
    *(data + 1) = payload_length;  // payload size
    // not using dynamic memory, we should copy it manual
    if (!ptr) {
        memcpy((void *)(data + 2), pf, payload_length);
    }

    return 0;
}

HRIF_STRUCT_GET(hrif_dmz_t, hrif_network_dmz_get)
HRIF_STRUCT_SET(hrif_dmz_t, hrif_network_dmz_set)

HRIF_STRUCT_GET(hrif_iptv_t, hrif_network_iptv_get)
HRIF_STRUCT_SET(hrif_iptv_t, hrif_network_iptv_set)

HRIF_STRUCT_GET(hrif_dos_t, hrif_network_dos_get)
HRIF_STRUCT_SET(hrif_dos_t, hrif_network_dos_set)

HRIF_STRUCT_GET(hrif_filter_mode_t, hrif_network_filter_mode_get)
HRIF_STRUCT_SET(hrif_filter_mode_t, hrif_network_filter_mode_set)

HRIF_STRUCT_GET(hrif_address_filter_t, hrif_network_address_filter_get)
HRIF_STRUCT_SET(hrif_address_filter_t, hrif_network_address_filter_set)

HRIF_STRUCT_GET(hrif_port_filter_t, hrif_network_port_filter_get)
HRIF_STRUCT_SET(hrif_port_filter_t, hrif_network_port_filter_set)

HRIF_STRUCT_GET(hrif_url_filter_t, hrif_network_url_filter_get)
HRIF_STRUCT_SET(hrif_url_filter_t, hrif_network_url_filter_set)

HRIF_STRUCT_GET(hrif_qos_t, hrif_network_qos_get)
HRIF_STRUCT_SET(hrif_qos_t, hrif_network_qos_set)

// clang-format off
static struct {
    unsigned int id;
    int (*transact)(struct binder_io *msg, struct binder_io *reply);
} _methods[] = {
    // force special index, avoiding someone write error! which may leading all method failed
    [HRIF_TRANSACT_CODE_NETWORK_INIT & HRIF_TRANSACT_CODE_ID_MASK]                       = {HRIF_TRANSACT_CODE_NETWORK_INIT, _hrif_network_init},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_SIZE & HRIF_TRANSACT_CODE_ID_MASK]                   = {HRIF_TRANSACT_CODE_NETWORK_WAN_SIZE, _hrif_network_wan_size},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_ARRAY & HRIF_TRANSACT_CODE_ID_MASK]                  = {HRIF_TRANSACT_CODE_NETWORK_WAN_ARRAY, _hrif_network_wan_array},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_GET & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_NETWORK_WAN_GET, _hrif_network_get_wan},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_GET_PROTO & HRIF_TRANSACT_CODE_ID_MASK]              = {HRIF_TRANSACT_CODE_NETWORK_WAN_GET_PROTO, _hrif_network_wan_get_proto},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_SET_PROTO & HRIF_TRANSACT_CODE_ID_MASK]              = {HRIF_TRANSACT_CODE_NETWORK_WAN_SET_PROTO, _hrif_network_wan_set_proto},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_GET_IPV4 & HRIF_TRANSACT_CODE_ID_MASK]               = {HRIF_TRANSACT_CODE_NETWORK_WAN_GET_IPV4, _hrif_network_wan_get_ipv4},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_SET_IPV4 & HRIF_TRANSACT_CODE_ID_MASK]               = {HRIF_TRANSACT_CODE_NETWORK_WAN_SET_IPV4, _hrif_network_wan_set_ipv4},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_GET_IPV6 & HRIF_TRANSACT_CODE_ID_MASK]               = {HRIF_TRANSACT_CODE_NETWORK_WAN_GET_IPV6, _hrif_network_wan_get_ipv6},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_SET_IPV6 & HRIF_TRANSACT_CODE_ID_MASK]               = {HRIF_TRANSACT_CODE_NETWORK_WAN_SET_IPV6, _hrif_network_wan_set_ipv6},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_GET_PPPOE & HRIF_TRANSACT_CODE_ID_MASK]              = {HRIF_TRANSACT_CODE_NETWORK_WAN_GET_PPPOE, _hrif_network_wan_get_pppoe},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_SET_PPPOE & HRIF_TRANSACT_CODE_ID_MASK]              = {HRIF_TRANSACT_CODE_NETWORK_WAN_SET_PPPOE, _hrif_network_wan_set_pppoe},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_COMMIT & HRIF_TRANSACT_CODE_ID_MASK]                 = {HRIF_TRANSACT_CODE_NETWORK_WAN_COMMIT, _hrif_network_wan_commit},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_DISCARD & HRIF_TRANSACT_CODE_ID_MASK]                = {HRIF_TRANSACT_CODE_NETWORK_WAN_DISCARD, _hrif_network_wan_discard},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_STATUS & HRIF_TRANSACT_CODE_ID_MASK]                 = {HRIF_TRANSACT_CODE_NETWORK_WAN_STATUS, _hrif_network_wan_get_status},
    [HRIF_TRANSACT_CODE_NETWORK_WAN_NETSTATS & HRIF_TRANSACT_CODE_ID_MASK]               = {HRIF_TRANSACT_CODE_NETWORK_WAN_NETSTATS, _hrif_network_wan_netstats},
    [HRIF_TRANSACT_CODE_NETWORK_GET_WORKMODE & HRIF_TRANSACT_CODE_ID_MASK]               = {HRIF_TRANSACT_CODE_NETWORK_GET_WORKMODE, _hrif_network_get_workmode},
    [HRIF_TRANSACT_CODE_NETWORK_SET_WORKMODE & HRIF_TRANSACT_CODE_ID_MASK]               = {HRIF_TRANSACT_CODE_NETWORK_SET_WORKMODE, _hrif_network_set_workmode},
    [HRIF_TRANSACT_CODE_NETWORK_LAN_GET & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_NETWORK_LAN_GET, _hrif_network_lan_get},
    [HRIF_TRANSACT_CODE_NETWORK_LAN_SET & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_NETWORK_LAN_SET, _hrif_network_lan_set},
    [HRIF_TRANSACT_CODE_NETWORK_DHCP_GET & HRIF_TRANSACT_CODE_ID_MASK]                   = {HRIF_TRANSACT_CODE_NETWORK_DHCP_GET, _hrif_network_dhcp_get},
    [HRIF_TRANSACT_CODE_NETWORK_DHCP_SET & HRIF_TRANSACT_CODE_ID_MASK]                   = {HRIF_TRANSACT_CODE_NETWORK_DHCP_SET, _hrif_network_dhcp_set},
    [HRIF_TRANSACT_CODE_NETWORK_LANHOST_SIZE & HRIF_TRANSACT_CODE_ID_MASK]               = {HRIF_TRANSACT_CODE_NETWORK_LANHOST_SIZE, _hrif_network_lanhost_size},
    [HRIF_TRANSACT_CODE_NETWORK_LANHOST_ARRAY & HRIF_TRANSACT_CODE_ID_MASK]              = {HRIF_TRANSACT_CODE_NETWORK_LANHOST_ARRAY, _hrif_network_lanhost_array},
    [HRIF_TRANSACT_CODE_NETWORK_LANHOST_ONLINE_SIZE & HRIF_TRANSACT_CODE_ID_MASK]        = {HRIF_TRANSACT_CODE_NETWORK_LANHOST_ONLINE_SIZE, _hrif_network_lanhost_online_size},
    [HRIF_TRANSACT_CODE_NETWORK_LANHOST_LIMITSPEED_SET & HRIF_TRANSACT_CODE_ID_MASK]     = {HRIF_TRANSACT_CODE_NETWORK_LANHOST_LIMITSPEED_SET, _hrif_network_lanhost_limitspeed_set},
    [HRIF_TRANSACT_CODE_NETWORK_LIMIT_GET & HRIF_TRANSACT_CODE_ID_MASK]                  = {HRIF_TRANSACT_CODE_NETWORK_LIMIT_GET, _hrif_network_limit_get},
    [HRIF_TRANSACT_CODE_NETWORK_LIMIT_SET & HRIF_TRANSACT_CODE_ID_MASK]                  = {HRIF_TRANSACT_CODE_NETWORK_LIMIT_SET, _hrif_network_limit_set},
    [HRIF_TRANSACT_CODE_NETWORK_LIMIT_DEL & HRIF_TRANSACT_CODE_ID_MASK]                  = {HRIF_TRANSACT_CODE_NETWORK_LIMIT_DEL, NULL /*_hrif_network_limit_del*/},
    [HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_ADD & HRIF_TRANSACT_CODE_ID_MASK]        = {HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_ADD, _hrif_network_port_forwarding_add},
    [HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_MOD & HRIF_TRANSACT_CODE_ID_MASK]        = {HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_MOD, _hrif_network_port_forwarding_mod},
    [HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_DEL & HRIF_TRANSACT_CODE_ID_MASK]        = {HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_DEL, _hrif_network_port_forwarding_del},
    [HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_ARRAY & HRIF_TRANSACT_CODE_ID_MASK]      = {HRIF_TRANSACT_CODE_NETWORK_PORT_FORWARDING_ARRAY, _hrif_network_port_forwarding_array},
    [HRIF_TRANSACT_CODE_NETWORK_DMZ_GET & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_NETWORK_DMZ_GET, _hrif_network_dmz_get},
    [HRIF_TRANSACT_CODE_NETWORK_DMZ_SET & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_NETWORK_DMZ_SET, _hrif_network_dmz_set},
    [HRIF_TRANSACT_CODE_NETWORK_IPTV_GET & HRIF_TRANSACT_CODE_ID_MASK]                   = {HRIF_TRANSACT_CODE_NETWORK_IPTV_GET, _hrif_network_iptv_get},
    [HRIF_TRANSACT_CODE_NETWORK_IPTV_SET & HRIF_TRANSACT_CODE_ID_MASK]                   = {HRIF_TRANSACT_CODE_NETWORK_IPTV_SET, _hrif_network_iptv_set},
    [HRIF_TRANSACT_CODE_NETWORK_DOS_GET & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_NETWORK_DOS_GET, _hrif_network_dos_get},
    [HRIF_TRANSACT_CODE_NETWORK_DOS_SET & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_NETWORK_DOS_SET, _hrif_network_dos_set},
    [HRIF_TRANSACT_CODE_NETWORK_FILTERMODE_GET & HRIF_TRANSACT_CODE_ID_MASK]             = {HRIF_TRANSACT_CODE_NETWORK_FILTERMODE_GET, _hrif_network_filter_mode_get},
    [HRIF_TRANSACT_CODE_NETWORK_FILTERMODE_SET & HRIF_TRANSACT_CODE_ID_MASK]             = {HRIF_TRANSACT_CODE_NETWORK_FILTERMODE_SET, _hrif_network_filter_mode_set},
    [HRIF_TRANSACT_CODE_NETWORK_ADDRESS_FILTER_GET & HRIF_TRANSACT_CODE_ID_MASK]         = {HRIF_TRANSACT_CODE_NETWORK_ADDRESS_FILTER_GET, _hrif_network_address_filter_get},
    [HRIF_TRANSACT_CODE_NETWORK_ADDRESS_FILTER_SET & HRIF_TRANSACT_CODE_ID_MASK]         = {HRIF_TRANSACT_CODE_NETWORK_ADDRESS_FILTER_SET, _hrif_network_address_filter_set},
    [HRIF_TRANSACT_CODE_NETWORK_PORT_FILTER_GET & HRIF_TRANSACT_CODE_ID_MASK]            = {HRIF_TRANSACT_CODE_NETWORK_PORT_FILTER_GET, _hrif_network_port_filter_get},
    [HRIF_TRANSACT_CODE_NETWORK_PORT_FILTER_SET & HRIF_TRANSACT_CODE_ID_MASK]            = {HRIF_TRANSACT_CODE_NETWORK_PORT_FILTER_SET, _hrif_network_port_filter_set},
    [HRIF_TRANSACT_CODE_NETWORK_URL_FILTER_GET & HRIF_TRANSACT_CODE_ID_MASK]             = {HRIF_TRANSACT_CODE_NETWORK_URL_FILTER_GET, _hrif_network_url_filter_get},
    [HRIF_TRANSACT_CODE_NETWORK_URL_FILTER_SET & HRIF_TRANSACT_CODE_ID_MASK]             = {HRIF_TRANSACT_CODE_NETWORK_URL_FILTER_SET, _hrif_network_url_filter_set},
    [HRIF_TRANSACT_CODE_NETWORK_QOS_GET & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_NETWORK_QOS_GET, _hrif_network_qos_get},
    [HRIF_TRANSACT_CODE_NETWORK_QOS_SET & HRIF_TRANSACT_CODE_ID_MASK]                    = {HRIF_TRANSACT_CODE_NETWORK_QOS_SET, _hrif_network_qos_set},
};
// clang-format on

static int _on_transact(uint32_t code, struct binder_io *msg, struct binder_io *reply) {
    if ((code & HRIF_TRANSACT_CODE_CATEGORY_MASK) != HRIF_TRANSACT_CODE_NETWORK_BASE) return -1;

    uint32_t id = code & ~HRIF_TRANSACT_CODE_CATEGORY_MASK;

    if (id > sizeof(_methods) / sizeof(_methods[0]) - 1) {
        HR_LOGE("id invalid ......\n");
        return -1;
    }

    if (!_methods[id].transact) {
        HR_LOGE("not support current transact code:%d\n", code);
        return -1;
    }

    if (_methods[id].id != code) {
        HR_LOGE("not support current transact code:0x%X(0x%X) your code maybe error!\n", code, _methods[id].id);
        return -1;
    }

    return _methods[id].transact(msg, reply);
}

int hrifd_network_init() {
    return hrifd_publish(SVC_NAME, _on_transact);
}
