#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <unistd.h>

#define DATABASE_TABLE_DIR "./j2stbls"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define TEMP_FAILURE_RETRY(exp)                \
    ({                                         \
        typeof(exp) _rc;                       \
        do {                                   \
            _rc = (exp);                       \
        } while (_rc == -1 && errno == EINTR); \
        _rc;                                   \
    })

typedef struct hrqueue {
    struct hrqueue *next;
    struct hrqueue *prev;

} hrqueue;
typedef struct {
    int epoll_fd;
    int running;
    struct hrqueue pending_task;

} hrtbl_global_t;

struct hrtbl_task {
    int fd;
    int events;          // used for epoll
    int pending_events;  // cared events which will be used for epoll in next loop
    void (*routin)(struct hrtbl_task *);
    struct hrqueue queue;
    int wd;
};

static hrtbl_global_t _global_data;

static int hrqueue_init(struct hrqueue *q) {
    q->next = q->prev = q;
    return 0;
}
static int hrqueue_empty(struct hrqueue *q) {
    return q->next == q;
}
static inline void hrqueue_insert_tail(struct hrqueue *h,
                                       struct hrqueue *q) {
    q->next = h;
    q->prev = h->prev;
    q->prev->next = q;
    h->prev = q;
}
static inline void hrqueue_remove(struct hrqueue *q) {
    q->prev->next = q->next;
    q->next->prev = q->prev;
}

static void inotify_task_routin(struct hrtbl_task *task) {
    printf("task:%p\n", task);

    const char *p;
    const struct inotify_event *e;
    char buf[4096] = {0};
    ssize_t size;

    if (!task) return;
    size = TEMP_FAILURE_RETRY(read(task->fd, buf, sizeof(buf)));
    printf("read size:%ld\n", size);
    if (size == -1)
        return;

    /* Now we have one or more inotify_event structs. */
    for (p = buf; p < buf + size; p += sizeof(*e) + e->len) {
        e = (const struct inotify_event *)p;

        printf("mask:0x%X, len:%d, name:%s\n", e->mask, e->len, e->name);
        if (e->mask & (IN_ATTRIB | IN_MODIFY)) {
            printf("modify .....\n");
        }
        if (e->mask & ~(IN_ATTRIB | IN_MODIFY)) {
            printf("rename.....\n");
        }

        if (e->mask & IN_DELETE && e->len) {
            printf("The file '%s' was deleted. Recreating...\n", e->name);
            // recreate_file(argv[1]);
        }
        //        w = find_watcher(loop, e->wd);
        //        if (w == NULL)
        //            continue; /* Stale event, no watchers left. */
    }
}
static int _init() {
    memset((void *)&_global_data, 0, sizeof(_global_data));
    hrqueue_init(&_global_data.pending_task);
    _global_data.epoll_fd = epoll_create1(O_CLOEXEC);
    if (_global_data.epoll_fd == -1)
        return errno;

    return 0;
}

static int _run(hrtbl_global_t *data) {
    int timeout = 0;

    struct epoll_event evs[24];
    data->running = 1;

    while (data->running != 0) {
        int epoll_loop_break = 0;
        // add all fds which have in pending queue
        printf("detect again ...empty:%d\n", hrqueue_empty(&data->pending_task));
        while (!hrqueue_empty(&data->pending_task)) {
            int epoll_op = -1;
            struct epoll_event ev;
            struct hrqueue *q = data->pending_task.next;

            struct hrtbl_task *task = (struct hrtbl_task *)((char *)(q)-offsetof(struct hrtbl_task, queue));

            epoll_op = EPOLL_CTL_MOD;
            if (task->events == 0) {
                epoll_op = EPOLL_CTL_ADD;
            }

            task->events = task->pending_events;

            memset((void *)&ev, 0, sizeof(ev));
            ev.events = task->events;  // EPOLLIN
            // ev.data.fd = task->fd;
            ev.data.ptr = (void *)task;
            printf("epoll op:0x%X, events:0x%x, fd:%d, task:%p\n", epoll_op, ev.events, ev.data.fd, ev.data.ptr);
            hrqueue_remove(q);
            printf("adding new task to poll:%d vs %d\n", ev.data.fd, task->fd);
            printf("task:%p\n", task);
            epoll_ctl(data->epoll_fd, epoll_op, task->fd, &ev);
        }

        for (;;) {
            timeout = -1;//12000;

            int nr = TEMP_FAILURE_RETRY(epoll_wait(data->epoll_fd, evs, ARRAY_SIZE(evs), timeout));
            printf("nr:%d\n", nr);
            if (nr == 0 || nr == -1) {
                // timeout
                printf("maybe timeout ...\n");
                epoll_loop_break = 1;
                break;
            }

            if (epoll_loop_break) break;

            for (int i = 0; i < nr; i++) {
                struct epoll_event *e = evs + i;

                printf("ready: fd:%d, ptr:%p\n", e->data.fd, e->data.ptr);
                struct hrtbl_task *task = (struct hrtbl_task *)e->data.ptr;

                task->routin(task);
            }
        }
    }

    return 0;
}
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    const char *path = DATABASE_TABLE_DIR;
    struct hrtbl_task *task = (struct hrtbl_task *)calloc(1, sizeof(struct hrtbl_task));
    _init();

    hrqueue_init(&task->queue);
    task->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (task->fd < 0) {
        return -1;
    }
    task->pending_events = EPOLLIN | EPOLLOUT;
    int events = IN_ATTRIB | IN_CREATE | IN_MODIFY | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO;

    task->wd = inotify_add_watch(task->fd, path, events);
    if (task->wd < 0) {
        // xxx
        return -1;
    }

    printf("inotify task:%p, fd:%d, wd:%d\n", task, task->fd, task->wd);
    task->routin = inotify_task_routin;

    hrqueue_insert_tail(&_global_data.pending_task, &task->queue);

    _run(&_global_data);

    inotify_rm_watch(task->fd, task->wd);
    return 0;
}
