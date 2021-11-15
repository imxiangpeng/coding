/* Copyright 2008 The Android Open Source Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include "binder.h"

uint32_t svcmgr_lookup(struct binder_state *bs, uint32_t target, const char *name)
{
    uint32_t handle;
    unsigned iodata[512/4];
    struct binder_io msg, reply;

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, SVC_MGR_NAME);
    bio_put_string16_x(&msg, name);

    if (binder_call(bs, &msg, &reply, target, SVC_MGR_CHECK_SERVICE))
        return 0;

    handle = bio_get_ref(BINDER_TYPE_HANDLE, &reply);

    if (handle)
        binder_acquire(bs, handle);

    binder_done(bs, &msg, &reply);

    return handle;
}

int svcmgr_publish(struct binder_state *bs, uint32_t target, const char *name, void *ptr)
{
    int status;
    unsigned iodata[512/4];
    struct binder_io msg, reply;

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, SVC_MGR_NAME);
    bio_put_string16_x(&msg, name);
    bio_put_obj(&msg, ptr);

    if (binder_call(bs, &msg, &reply, target, SVC_MGR_ADD_SERVICE))
        return -1;

    status = bio_get_uint32(&reply);

    binder_done(bs, &msg, &reply);

    return status;
}

uint32_t svcmgr_call(struct binder_state *bs, const char *name, int code)
{
    uint32_t handle;
    unsigned iodata[512/4];
    struct binder_io msg, reply;

    int target = svcmgr_lookup(bs, BINDER_SERVICE_MANAGER, name);

    printf("we got target:%d\n", target);
    if (target <= 0) {
      return -1;
    }

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, "SuperStream");
    //bio_put_uint32(&msg, code);
    //bio_put_string16_x(&msg, name);

    if (binder_call(bs, &msg, &reply, target, code)){
        printf("%s(%d): error .....\n", __FUNCTION__, __LINE__);
        return 0;
    }
#if 0
    handle = bio_get_ref(BINDER_TYPE_HANDLE, &reply);

    if (handle)
        binder_acquire(bs, handle);
#endif
    int fd = bio_get_ref(BINDER_TYPE_FD, &reply);
    printf("fd:%d\n", fd);
    if (fd > 0) {
      void* ptr = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      printf("ptr:%p\n", ptr);
      char buf[10] = { 0 };
      memcpy(buf, ptr, 10);
      buf[9] = '\0';
      printf("data:%s\n", buf);
    }

    binder_release(bs, handle);

    binder_done(bs, &msg, &reply);

    return handle;
}

unsigned token;

int main(int argc, char **argv)
{
    struct binder_state *bs;
    uint32_t svcmgr = BINDER_SERVICE_MANAGER;
    uint32_t handle;

    //bs = binder_open("/dev/binder", 128*1024);
    bs = binder_open("/dev/aosp9_binder105", 128*1024);
    if (!bs) {
        fprintf(stderr, "failed to open binder driver\n");
        return -1;
    }

    argc--;
    argv++;
    while (argc > 0) {
        if (!strcmp(argv[0],"alt")) {
            handle = svcmgr_lookup(bs, svcmgr, "alt_svc_mgr");
            if (!handle) {
                fprintf(stderr,"cannot find alt_svc_mgr\n");
                return -1;
            }
            svcmgr = handle;
            fprintf(stderr,"svcmgr is via %x\n", handle);
        } else if (!strcmp(argv[0],"lookup")) {
            if (argc < 2) {
                fprintf(stderr,"argument required\n");
                return -1;
            }
            handle = svcmgr_lookup(bs, svcmgr, argv[1]);
            fprintf(stderr,"lookup(%s) = %x\n", argv[1], handle);
            argc--;
            argv++;
        } else if (!strcmp(argv[0],"publish")) {
            if (argc < 2) {
                fprintf(stderr,"argument required\n");
                return -1;
            }
            svcmgr_publish(bs, svcmgr, argv[1], &token);
            argc--;
            argv++;
        } else if (!strcmp(argv[0],"call")) {
            if (argc < 3) {
                fprintf(stderr,"argument required\n");
                return -1;
            }
            svcmgr_call(bs, argv[1], atoi(argv[2]));
            argc -= 2;
            argv += 2;
        } else {
            fprintf(stderr,"unknown command %s\n", argv[0]);
            return -1;
        }
        argc--;
        argv++;
    }
    return 0;
}
