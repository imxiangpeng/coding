/* Copyright 2008 The Android Open Source Project
 */

// mxp, 20240418 modified based on android sources
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/android/binder.h>
#include <linux/android/binderfs.h>

#include "hrbinder.h"

#define HRBINDERFS_DIR "/dev/binderfs"
#define HRBINDER_NAME "hrbinder"

#define ALOGI(x...) fprintf(stderr, "svcmgr: " x)
#define ALOGE(x...) fprintf(stderr, "svcmgr: " x)

const char *str8(const uint16_t *x, size_t x_len) {
    static char buf[128];
    size_t max = 127;
    char *p = buf;

    if (x_len < max) {
        max = x_len;
    }

    if (x) {
        while ((max > 0) && (*x != '\0')) {
            *p++ = *x++;
            max--;
        }
    }
    *p++ = 0;
    return buf;
}

int str16eq(const uint16_t *a, const char *b) {
    while (*a && *b)
        if (*a++ != *b++) return 0;
    if (*a || *b)
        return 0;
    return 1;
}

static char *service_manager_context;
static struct selabel_handle *sehandle;

static int svc_can_register(const uint16_t *name, size_t name_len, pid_t spid, uid_t uid) {
    return 1;
}

static int svc_can_list(pid_t spid, uid_t uid) {
    return 1;
}

static int svc_can_find(const uint16_t *name, size_t name_len, pid_t spid, uid_t uid) {
    return 1;
}

struct svcinfo {
    struct svcinfo *next;
    uint32_t handle;
    struct binder_death death;
    int allow_isolated;
    uint32_t dumpsys_priority;
    size_t len;
    uint16_t name[0];
};

struct svcinfo *svclist = NULL;

struct svcinfo *find_svc(const uint16_t *s16, size_t len) {
    struct svcinfo *si;

    for (si = svclist; si; si = si->next) {
        if ((len == si->len) &&
            !memcmp(s16, si->name, len * sizeof(uint16_t))) {
            return si;
        }
    }
    return NULL;
}

void svcinfo_death(struct binder_state *bs, void *ptr) {
    struct svcinfo *si = (struct svcinfo *)ptr;

    ALOGI("service '%s' died\n", str8(si->name, si->len));
    if (si->handle) {
        binder_release(bs, si->handle);
        si->handle = 0;
    }
}

uint16_t svcmgr_id[] = {
    'h', 'r', 's', 'e', 'r', 'v', 'i', 'c', 'e', 'm', 'a', 'n', 'a', 'g', 'e', 'r'};

uint32_t do_find_service(const uint16_t *s, size_t len, uid_t uid, pid_t spid) {
    struct svcinfo *si = find_svc(s, len);

    if (!si || !si->handle) {
        return 0;
    }
#if 0
    if (!si->allow_isolated) {
        // If this service doesn't allow access from isolated processes,
        // then check the uid to see if it is isolated.
        uid_t appid = uid % AID_USER;
        if (appid >= AID_ISOLATED_START && appid <= AID_ISOLATED_END) {
            return 0;
        }
    }
#endif

    if (!svc_can_find(s, len, spid, uid)) {
        return 0;
    }

    return si->handle;
}

int do_add_service(struct binder_state *bs, const uint16_t *s, size_t len, uint32_t handle,
                   uid_t uid, int allow_isolated, uint32_t dumpsys_priority, pid_t spid) {
    struct svcinfo *si;

    ALOGI("add_service('%s',%x,%s) uid=%d\n", str8(s, len), handle,
          allow_isolated ? "allow_isolated" : "!allow_isolated", uid);

    if (!handle || (len == 0) || (len > 127))
        return -1;

    if (!svc_can_register(s, len, spid, uid)) {
        ALOGE("add_service('%s',%x) uid=%d - PERMISSION DENIED\n",
              str8(s, len), handle, uid);
        return -1;
    }

    si = find_svc(s, len);
    if (si) {
        if (si->handle) {
            ALOGE("add_service('%s',%x) uid=%d - ALREADY REGISTERED, OVERRIDE\n",
                  str8(s, len), handle, uid);
            svcinfo_death(bs, si);
        }
        si->handle = handle;
    } else {
        si = malloc(sizeof(*si) + (len + 1) * sizeof(uint16_t));
        if (!si) {
            ALOGE("add_service('%s',%x) uid=%d - OUT OF MEMORY\n",
                  str8(s, len), handle, uid);
            return -1;
        }
        si->handle = handle;
        si->len = len;
        memcpy(si->name, s, (len + 1) * sizeof(uint16_t));
        si->name[len] = '\0';
        si->death.func = (void *)svcinfo_death;
        si->death.ptr = si;
        si->allow_isolated = allow_isolated;
        si->dumpsys_priority = dumpsys_priority;
        si->next = svclist;
        svclist = si;
    }

    binder_acquire(bs, handle);
    binder_link_to_death(bs, handle, &si->death);
    return 0;
}

int svcmgr_handler(struct binder_state *bs,
                   struct binder_transaction_data *txn,
                   struct binder_io *msg,
                   struct binder_io *reply) {
    struct svcinfo *si;
    uint16_t *s;
    size_t len;
    uint32_t handle;
    uint32_t strict_policy;
    int allow_isolated;
    uint32_t dumpsys_priority;

    ALOGI("target=%p code=%d pid=%d uid=%d\n",
          txn->target.ptr, txn->code, txn->sender_pid, txn->sender_euid);

    if (txn->target.ptr != BINDER_SERVICE_MANAGER)
        return -1;

    if (txn->code == PING_TRANSACTION)
        return 0;

    // Equivalent to Parcel::enforceInterface(), reading the RPC
    // header with the strict mode policy mask and the interface name.
    // Note that we ignore the strict_policy and don't propagate it
    // further (since we do no outbound RPCs anyway).
    strict_policy = bio_get_uint32(msg);
    s = bio_get_string16(msg, &len);
    if (s == NULL) {
        return -1;
    }

    if ((len != (sizeof(svcmgr_id) / 2)) ||
        memcmp(svcmgr_id, s, sizeof(svcmgr_id))) {
        fprintf(stderr, "invalid id %s\n", str8(s, len));
        return -1;
    }

    switch (txn->code) {
        case SVC_MGR_GET_SERVICE:
        case SVC_MGR_CHECK_SERVICE:
            s = bio_get_string16(msg, &len);
            if (s == NULL) {
                return -1;
            }
            handle = do_find_service(s, len, txn->sender_euid, txn->sender_pid);
            if (!handle)
                break;
            bio_put_ref(reply, handle);
            return 0;

        case SVC_MGR_ADD_SERVICE:
            s = bio_get_string16(msg, &len);
            if (s == NULL) {
                return -1;
            }
            handle = bio_get_ref(msg);
            allow_isolated = bio_get_uint32(msg) ? 1 : 0;
            dumpsys_priority = bio_get_uint32(msg);
            if (do_add_service(bs, s, len, handle, txn->sender_euid, allow_isolated, dumpsys_priority,
                               txn->sender_pid))
                return -1;
            break;

        case SVC_MGR_LIST_SERVICES: {
            uint32_t n = bio_get_uint32(msg);
            uint32_t req_dumpsys_priority = bio_get_uint32(msg);

            if (!svc_can_list(txn->sender_pid, txn->sender_euid)) {
                ALOGE("list_service() uid=%d - PERMISSION DENIED\n",
                      txn->sender_euid);
                return -1;
            }
            si = svclist;
            // walk through the list of services n times skipping services that
            // do not support the requested priority
            while (si) {
                if (si->dumpsys_priority & req_dumpsys_priority) {
                    if (n == 0) break;
                    n--;
                }
                si = si->next;
            }
            if (si) {
                bio_put_string16(reply, si->name);
                return 0;
            }
            return -1;
        }
        default:
            ALOGE("unknown code %d\n", txn->code);
            return -1;
    }

    bio_put_uint32(reply, 0);
    return 0;
}

// see binderfs_example.c
static int init_binder() {
    int fd, ret, saved_errno;
    struct binderfs_device device = {0};
#if 0
	ret = unshare(CLONE_NEWNS);
	if (ret < 0) {
		fprintf(stderr, "%s - Failed to unshare mount namespace\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	ret = mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, 0);
	if (ret < 0) {
		fprintf(stderr, "%s - Failed to mount / as private\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif
    struct stat sb;
    if (lstat(HRBINDERFS_DIR, &sb) != 0) {
        if (errno == ENOENT)
            goto create;
        return -1;
    }
    if (S_ISDIR(sb.st_mode)) {
        fprintf(stderr, "binderfs has been created\n");
        return 0;  // have created
    }

create:
    ret = mkdir(HRBINDERFS_DIR, 0755);
    if (ret < 0 && errno != EEXIST) {
        fprintf(stderr, "%s - Failed to create binderfs mountpoint\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    ret = mount(NULL, HRBINDERFS_DIR, "binder", 0, 0);
    if (ret < 0) {
        fprintf(stderr, "%s - Failed to mount binderfs\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    memcpy(device.name, HRBINDER_NAME, strlen(HRBINDER_NAME));
    fd = open("/dev/binderfs/binder-control", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "%s - Failed to open binder-control device\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    ret = ioctl(fd, BINDER_CTL_ADD, &device);
    saved_errno = errno;
    close(fd);
    errno = saved_errno;
    if (ret < 0) {
        fprintf(stderr, "%s - Failed to allocate new binder device\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("Allocated new binder device with major %d, minor %d, and name %s\n",
           device.major, device.minor, device.name);
#if 0
	ret = unlink("/dev/binderfs/my-binder");
	if (ret < 0) {
		fprintf(stderr, "%s - Failed to delete binder device\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif
}
int main(int argc, char **argv) {
    struct binder_state *bs;

    char *driver = HRBINDERFS_DIR "/" HRBINDER_NAME;

    if (init_binder() != 0) {
        printf("binder init failed ...\n");
        exit(EXIT_FAILURE);
    }

    bs = binder_open(driver, 128 * 1024);
    if (!bs) {
#ifdef VENDORSERVICEMANAGER
        ALOGW("failed to open binder driver %s\n", driver);
        while (true) {
            sleep(UINT_MAX);
        }
#else
        ALOGE("failed to open binder driver %s\n", driver);
#endif
        return -1;
    }

    if (binder_become_context_manager(bs)) {
        ALOGE("cannot become context manager (%s)\n", strerror(errno));
        return -1;
    }

    binder_loop(bs, svcmgr_handler);

    return 0;
}
