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

#include "hrifd.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <libunwind.h>

#include <ucontext.h>
#include <sys/mman.h>

#define ALOGE(x...) fprintf(stderr, "hrifd: " x)

#ifndef _UNUSED
#define _UNUSED __attribute__((__unused__))
#endif

// mxp, 20240430, current alt sig stack is not working
#define ENABLE_ALTSIG_STACK 0

#define MAX_BACKTRACE_LINE_LENGTH 512
#define MAX_BACKTRACE_DEPTH 16

static struct binder_state *_bs = NULL;

static int hrifd_srv_handler(struct binder_state *bs,
                             struct binder_transaction_data *txn,
                             struct binder_io *msg,
                             struct binder_io *reply) {
    (void)bs;
    // ALOGE("hrifd handler : tnarget=0x%llX code=%d pid=%d uid=%d\n",
    //      txn->target.ptr, txn->code, txn->sender_pid, txn->sender_euid);
    switch (txn->code) {
        case PING_TRANSACTION:
            bio_put_uint32(reply, 0);
            break;
        default: {
            hrifd_on_transact on_transact = (hrifd_on_transact)(uintptr_t)(txn->target.ptr);
            if (on_transact) {
                return on_transact(txn->code, msg, reply);
            }
            break;
        }
    }

    return 0;
}

int hrifd_publish(const char *name, hrifd_on_transact on_transact) {
    int status;
    unsigned iodata[512 / 4] = {0};
    struct binder_io msg, reply;

    if (!name) return -1;

    if (!_bs) {
        ALOGE("binder maybe not initialized ...\n");
        return -1;
    }

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, SVC_MGR_NAME);
    bio_put_string16_x(&msg, name);
    bio_put_obj(&msg, (void *)on_transact);  // obj
    bio_put_uint32(&msg, 1);                 // allow isolate
    bio_put_uint32(&msg, 0 /*1 << 3*/);      // dumpsys priority

    if (binder_call(_bs, &msg, &reply, BINDER_SERVICE_MANAGER, SVC_MGR_ADD_SERVICE)) {
        ALOGE("add service failed ...\n");
        return -1;
    }

    status = bio_get_uint32(&reply);

    binder_done(_bs, &msg, &reply);

    return status;
}

// link to death
// BC_REQUEST_DEATH_NOTIFICATION
// BC_CLEAR_DEATH_NOTIFICATION
static int _hrifd_ping(void) {
    unsigned iodata[512 / 4] = {0};
    struct binder_io msg, reply;

    if (!_bs) {
        ALOGE("binder maybe not initialized ...\n");
        return -1;
    }

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, SVC_MGR_NAME);

    if (binder_call(_bs, &msg, &reply, BINDER_SERVICE_MANAGER, PING_TRANSACTION)) {
        ALOGE("ping hrsvc failed ...\n");
        return -1;
    }

    binder_done(_bs, &msg, &reply);

    return 0;
}

static void _signal_action(int signum, siginfo_t *siginfo _UNUSED, void *sigcontext) {
    (void)sigcontext;
    char line[MAX_BACKTRACE_LINE_LENGTH] = {0};
    ucontext_t *uct = sigcontext;
    uint32_t i = 0;
    unw_cursor_t cursor;
    unw_context_t uc;

    (void)uct;
    signal(signum, SIG_DFL);

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    // unw_init_local2(&cursor, &uc, UNW_INIT_SIGNAL_FRAME);

    // dont assign reg manual which leading loss stack
#ifdef __arm__
    unw_set_reg(&cursor, UNW_ARM_R0, uct->uc_mcontext.arm_r0);
    unw_set_reg(&cursor, UNW_ARM_R1, uct->uc_mcontext.arm_r1);
    unw_set_reg(&cursor, UNW_ARM_R2, uct->uc_mcontext.arm_r2);
    unw_set_reg(&cursor, UNW_ARM_R3, uct->uc_mcontext.arm_r3);
    unw_set_reg(&cursor, UNW_ARM_R4, uct->uc_mcontext.arm_r4);
    unw_set_reg(&cursor, UNW_ARM_R5, uct->uc_mcontext.arm_r5);
    unw_set_reg(&cursor, UNW_ARM_R6, uct->uc_mcontext.arm_r6);
    unw_set_reg(&cursor, UNW_ARM_R7, uct->uc_mcontext.arm_r7);
    unw_set_reg(&cursor, UNW_ARM_R8, uct->uc_mcontext.arm_r8);
    unw_set_reg(&cursor, UNW_ARM_R9, uct->uc_mcontext.arm_r9);
    unw_set_reg(&cursor, UNW_ARM_R10, uct->uc_mcontext.arm_r10);
    unw_set_reg(&cursor, UNW_ARM_R11, uct->uc_mcontext.arm_fp);
    unw_set_reg(&cursor, UNW_ARM_R12, uct->uc_mcontext.arm_ip);
    unw_set_reg(&cursor, UNW_ARM_R13, uct->uc_mcontext.arm_sp);
    unw_set_reg(&cursor, UNW_ARM_R14, uct->uc_mcontext.arm_lr);
    unw_set_reg(&cursor, UNW_ARM_R15, uct->uc_mcontext.arm_pc);
    unw_set_reg(&cursor, UNW_REG_IP, uct->uc_mcontext.arm_pc);
    unw_set_reg(&cursor, UNW_REG_SP, uct->uc_mcontext.arm_sp);
#endif

    printf("RECV SIGNAL: %d\n", signum);

    do {
        unw_word_t pc;
        _UNUSED unw_word_t offset;
        char sym[256] = {0};
        char filename[256] = {0};

        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (unw_is_signal_frame(&cursor)) {
            printf("skip signal ......\n");
            // continue;
        }

        unw_get_proc_name(&cursor, sym, sizeof(sym), &offset);

        unw_get_elf_filename(&cursor, filename, sizeof(filename), NULL);

        snprintf(line, sizeof(line), "#%02u pc %08x %.*s (%.*s+%d)", i, pc, 30 /**/, filename, 30, sym, offset);
        printf("%s\n", line);

        i++;
    } while (unw_step(&cursor) >= 0 && i < MAX_BACKTRACE_DEPTH);

    exit(EXIT_FAILURE);
}

static void _signal_init() {
#if ENABLE_ALTSIG_STACK
    stack_t stack;
    memset(&stack, 0, sizeof(stack));
    /* Reserver the system default stack size. We don't need that much by the way. */
    stack.ss_size = SIGSTKSZ;
    stack.ss_sp = malloc(stack.ss_size);
    stack.ss_flags = 0;
    /* Install alternate stack size. Be sure the memory region is valid until you revert it. */
    sigaltstack(&stack, NULL);
#endif

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = _signal_action;
    action.sa_flags = SA_RESTART | SA_SIGINFO;

#if ENABLE_ALTSIG_STACK
    // mxp, 20240430, must set SA_ONSTACK otherwise we may not obtion full stack
    // Use the alternate signal stack if available so we can catch stack overflows.
    action.sa_flags |= SA_ONSTACK;
#endif

    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGBUS, &action, NULL);
    sigaction(SIGFPE, &action, NULL);
    sigaction(SIGILL, &action, NULL);
    sigaction(SIGPIPE, &action, NULL);
    sigaction(SIGSEGV, &action, NULL);
#if defined(SIGSTKFLT)
    sigaction(SIGSTKFLT, &action, NULL);
#endif
    sigaction(SIGTRAP, &action, NULL);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int retries = 20;

    _signal_init();

    // adjust output line buffered mode
    setvbuf(stdout, NULL, _IOLBF, 0);

    while (retries-- > 0) {
        _bs = binder_open("/dev/binderfs/hrbinder", 128 * 1024);
        if (_bs) {
            break;
        }
        usleep(1000 * 500);
        continue;
    }

    // do not set maxthreads it will leading crash
    // binder_set_maxthreads(_bs, 2);

    // do not reset retries, loop continue
    while (retries-- > 0) {
        if (_hrifd_ping() == 0) {
            break;
        }
        usleep(1000 * 500);
    }

    if (retries <= 0) {
        ALOGE("can not find hrservicemanager .......\n");
        return -1;
    }

    hrifd_system_init();
    hrifd_network_init();
    hrifd_wireless_init();

    binder_loop(_bs, hrifd_srv_handler);
    return 0;
}
