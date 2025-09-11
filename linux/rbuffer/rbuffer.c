#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PENDING_MESSAGE_CAPABILITY (1 << 5) // 2^5

struct {
  unsigned int head;
  unsigned int tail;
  // int size;  // current element size
  unsigned int caps; // buffer max size
  const char **data;
} pmsg; // pending message

static int push_pending_message(const char *data) {
  int id = 0;
  if (!data)
    return -1;

  if (!pmsg.data) {
    // init pending message
    pmsg.caps = PENDING_MESSAGE_CAPABILITY;
    pmsg.data = (const char **)calloc(pmsg.caps, sizeof(const char *));
    if (!pmsg.data) {
      pmsg.caps = 0;
      return -1;
    }
  }
  // always put new data overwrite
  id = pmsg.head & (pmsg.caps - 1);
  printf("id: %d (%d - %d)-> %s\n", id, pmsg.head, pmsg.tail, data);
  // free old data
  if (pmsg.data[id]) {
    printf("free: %s\n", pmsg.data[id]);
    free((void *)pmsg.data[id]);
    // we should move tail
    pmsg.tail++;
  }
  pmsg.data[id] = data;
  pmsg.head++;

  return 0;
}

static int pop_pending_message(const char **data) {
  int id = 0;
  if (!pmsg.data || !data)
    return -1;

  printf("head - tail :%d\n", pmsg.head - pmsg.tail);
  // no pending message
  if (pmsg.head - pmsg.tail == 0) {
    return -1;
  }
  // always put new data overwrite
  id = pmsg.tail & (pmsg.caps - 1);

  *data = pmsg.data[id];
  pmsg.tail++;

  return 0;
}

int main(int argc, char **argv) {

  for (int i = 0; i < 33; i++) {
    char *data = (char *)calloc(256, 1);
    snprintf(data, 256, "this is test message: %d\n", i);
    push_pending_message(data);
  }

  for (int i = 0; i < 50; i++) {
    const char *data = NULL;
    if (pop_pending_message(&data) != 0) {
      printf("%d -> no pending message ...\n", i);
    } else {
      printf("%d -> got pending:%s\n", i, data);
      free((void*)data);
    }
  }

  // finally release memory
  free(pmsg.data);
  return 0;
}
