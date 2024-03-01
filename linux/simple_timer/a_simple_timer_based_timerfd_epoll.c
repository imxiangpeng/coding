#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <time.h>

#include <errno.h>

// slist
#include <sys/queue.h>

#include <pthread.h>

#define MILLISECS_PER_SECOND 1000
#define MICROSECS_PER_MILLISECOND 1000
#define MICROSECS_PER_SECOND (MICROSECS_PER_MILLISECOND * MILLISECS_PER_SECOND)
#define NANOSECS_PER_MICROSECOND 1000

#define TEMP_FAILURE_RETRY(exp)                                                \
  ({                                                                           \
    typeof(exp) _rc;                                                           \
    do {                                                                       \
      _rc = (exp);                                                             \
    } while (_rc == -1 && errno == EINTR);                                     \
    _rc;                                                                       \
  })

struct _timer {
  int id;
  int (*timer_cb)(void *);
  void *data;
  uint64_t time_in_micros;
  SLIST_ENTRY(_timer) next;
};

struct __timer {
  int epoll_fd;
  int timer_fd;
  pthread_t thread;
  SLIST_HEAD(__timer_head, _timer) head;
} _timer;

static uint64_t now_in_microseconds() {
  int ret = 0;
  uint64_t result = 0;
  struct timespec ts;
  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret != 0) {
    printf("error this should not fail ....\n");
  }

  result = ts.tv_sec;
  result *= MICROSECS_PER_SECOND;
  result += (ts.tv_nsec / NANOSECS_PER_MICROSECOND);

  return result;
}

int _reschdule_timer() {
  struct timespec ts;
  struct __timer_head *head = &_timer.head;
  struct _timer *timer = SLIST_FIRST(head);
  struct itimerspec alarm_time = {};
  uint64_t now = now_in_microseconds();
  uint64_t elapse_micros = 0;

  if (!timer) {
    // empty ...
    return 0;
  }

  if (timer->time_in_micros < now) {
    elapse_micros = 1;
  } else {
    elapse_micros = timer->time_in_micros - now;
  }

  alarm_time.it_value.tv_sec = elapse_micros / MICROSECS_PER_SECOND;
  alarm_time.it_value.tv_nsec =
      elapse_micros % MICROSECS_PER_SECOND * NANOSECS_PER_MICROSECOND;

  printf("time:%ld, alarm_time.it_value.tv_sec:%ld, "
         "alarm_time.it_value.tv_nsec:%ld\n",
         elapse_micros, alarm_time.it_value.tv_sec,
         alarm_time.it_value.tv_nsec);
  timerfd_settime(_timer.timer_fd, 0, &alarm_time, NULL);

  return 0;
}

struct _timer *_request_timer(int ms) {

  struct _timer *loop = NULL;
  struct _timer *prev = NULL;
  struct __timer_head *head = &_timer.head;

  struct _timer *timer = (struct _timer *)malloc(sizeof(struct _timer));

  if (!timer || ms <= 0)
    return NULL;

  memset((void *)timer, 0, sizeof(*timer));

  timer->time_in_micros =
      now_in_microseconds() + ms * MICROSECS_PER_MILLISECOND;

  SLIST_FOREACH(loop, head, next) {
    if (timer->time_in_micros < loop->time_in_micros) {
      break;
    }
    prev = loop;
  }

  // empty list
  if (!prev) {
    SLIST_INSERT_HEAD(head, timer, next);
  } else {
    SLIST_INSERT_AFTER(prev, timer, next);
  }

  return timer;
}

struct _timer *_timer_oneshot(uint32_t ms, int (*timer_cb)(void *), void *arg) {

  struct __timer_head *head = &_timer.head;
  struct _timer *timer = _request_timer(ms);

  if (!timer)
    return NULL;

  timer->timer_cb = timer_cb;
  timer->data = arg;
  _reschdule_timer();
  return timer;
}

int _timer_cancel(struct _timer *timer) {
  struct _timer *loop = NULL;
  struct __timer_head *head = &_timer.head;

  if (!timer)
    return -1;

  SLIST_FOREACH(loop, head, next) {
    printf("timer:%d, time in micros:%ld, later:%ld\n", loop->id,
           loop->time_in_micros, loop->time_in_micros - now_in_microseconds());
  }

  return 0;
}
void *_timer_thread(void *arg) {

  int epoll_timeout_ms = 0;
  struct epoll_event ev;
  struct __timer *timer = (struct __timer *)arg;

  if (!timer) {
    return NULL;
  }

  while (1) {
    int nr = TEMP_FAILURE_RETRY(
        epoll_wait(timer->epoll_fd, &ev, 1, epoll_timeout_ms));
    if (nr == -1) {
      printf("can not wait event ....\n");
      continue;
    } else if (nr == 1) {
      printf("got data ...\n");
      //((void (*)())ev.data.ptr)();
      //_data_available(ev.data.fd);
      uint64_t data = 0;
      int ret =
          TEMP_FAILURE_RETRY(read(ev.data.fd, (void *)&data, sizeof(data)));
      printf("ack: %ld\n", data);

      struct _timer *loop = NULL;
      struct _timer *fire = NULL;
      struct __timer_head *head = &_timer.head;
      uint64_t now = now_in_microseconds();
      SLIST_FOREACH(loop, head, next) {
        printf("now:%ld, timer:%ld, fire ...%d\n", now, loop->time_in_micros,
               loop->id);
        if (now >= loop->time_in_micros) {
          // fire ...
          fire = loop;
          printf("now:%ld, timer:%ld, fire ...%d\n", now, loop->time_in_micros,
                 loop->id);

          printf("12 list is empty:%d\n", SLIST_EMPTY(head));
          break;
        } else {
          break;
        }
      }
      if (fire) {
        SLIST_REMOVE(head, fire, _timer, next);
        // must call after loop, some boby may adjust link in fire func
        if (fire->timer_cb)
          fire->timer_cb(fire->data);
        // auto free ...
        free(fire);
      }
      printf("list is empty:%d\n", SLIST_EMPTY(head));
      {
        struct _timer *i = NULL;
        SLIST_FOREACH(i, head, next) {
          printf("timer:%d, time in micros:%ld, later:%ld\n", i->id,
                 i->time_in_micros, i->time_in_micros - now_in_microseconds());
        }
      }

      _reschdule_timer();
    }
  }

  return NULL;
}
static int _fire(void *arg) {

  printf("fire:...################################.....\n");

  return 0;
}
static int _fire2(void *arg) {
  printf("%s(%d) \n", __FUNCTION__, __LINE__);
  return 0;
}

static int _fire3(void *arg) {
  printf("%s(%d) \n", __FUNCTION__, __LINE__);
  return 0;
}
static int _fire_repeate(void *arg) {
  printf("%s(%d) \n", __FUNCTION__, __LINE__);
  struct _timer *tt =
      _timer_oneshot(5000, _fire_repeate, (void *)_fire_repeate);
  return 0;
}
int main(int argc, char **argv) {

  pthread_attr_t attributes;
  struct epoll_event ev;
  struct __timer_head *head = &_timer.head;

  memset((void *)&_timer, 0, sizeof(_timer));

  SLIST_INIT(&_timer.head);

  _timer.epoll_fd = epoll_create1(EPOLL_CLOEXEC);

  if (_timer.epoll_fd < 0) {
    return -1;
  }

  _timer.timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
  if (_timer.timer_fd < 0) {
    close(_timer.epoll_fd);
    return -1;
  }

  ev.events = EPOLLIN;
  ev.data.fd = _timer.timer_fd;
  // ev.data.ptr = (void *)_data_available;
  if (epoll_ctl(_timer.epoll_fd, EPOLL_CTL_ADD, _timer.timer_fd, &ev) == -1) {
    close(_timer.epoll_fd);
    close(_timer.timer_fd);
    printf("epoll ctl on fd failed! ...:%s\n", strerror(errno));
    return -1;
  }

  pthread_attr_init(&attributes);
  // pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
  // pthread_attr_setstacksize(&attributes, stack_size);
  int err = pthread_create(&_timer.thread, &attributes, _timer_thread,
                           (void *)&_timer);
  if (err != 0) {
    printf("thread create failed! ...:%s\n", strerror(errno));
    close(_timer.epoll_fd);
    close(_timer.timer_fd);

    return -1;
  }

  struct _timer *t = (struct _timer *)malloc(sizeof(struct _timer));
  memset((void *)t, 0, sizeof(struct _timer));
  t->id = 0;
  t->timer_cb = _fire2;

  if (SLIST_EMPTY(head)) {
    SLIST_INSERT_HEAD(head, t, next);
  } else {
    struct _timer *i = NULL;
    struct _timer *l = NULL;
    SLIST_FOREACH(i, head, next) { l = i; };
    /*insert at the end of list*/
    SLIST_INSERT_AFTER(l, t, next);
  }
  t = (struct _timer *)malloc(sizeof(struct _timer));
  memset((void *)t, 0, sizeof(struct _timer));
  t->id = 1;
  t->timer_cb = _fire3;

  if (SLIST_EMPTY(head)) {
    SLIST_INSERT_HEAD(head, t, next);
  } else {
    struct _timer *i = NULL;
    struct _timer *l = NULL;
    SLIST_FOREACH(i, head, next) { l = i; };
    /*insert at the end of list*/
    SLIST_INSERT_AFTER(l, t, next);
  }
  t = (struct _timer *)malloc(sizeof(struct _timer));
  memset((void *)t, 0, sizeof(struct _timer));
  t->id = 2;
  t->timer_cb = _fire2;

  SLIST_INSERT_HEAD(head, t, next);

  {
    struct _timer *i = NULL;
    SLIST_FOREACH(i, head, next) { printf("timer:%d\n", i->id); };
  }

  struct itimerspec timeout = {};
  timeout.it_value.tv_sec = 10;
  if (timerfd_settime(_timer.timer_fd, 0, &timeout, NULL) < 0) {
    printf("settime failed ....");
  }

  struct _timer *tt = _timer_oneshot(5000, _fire, (void *)_fire);
  tt = _timer_oneshot(60000, _fire_repeate, (void *)_fire_repeate);

  pthread_join(_timer.thread, NULL);

  close(_timer.epoll_fd);
  close(_timer.timer_fd);

  return 0;
}
