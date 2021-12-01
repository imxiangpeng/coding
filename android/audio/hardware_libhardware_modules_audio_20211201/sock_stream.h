#ifndef SOCK_STREAM_H
#define SOCK_STREAM_H

#include <pthread.h>

// the cycle buffer structure
typedef struct {
    unsigned char *buff;
    unsigned int buff_size;
    unsigned int pos_in;
    unsigned int pos_out;
    pthread_mutex_t lock;
}cycle_buffer_t;


typedef struct {
    cycle_buffer_t *buffer;
    int is_running;
    int stream_enabled;
    pthread_t thread_id;
    int dump_produce_fd;
    int dump_consume_fd;
} sock_stream_t;

sock_stream_t *sock_stream_init(void);
void sock_stream_destroy(sock_stream_t *stream);
void sock_stream_open(sock_stream_t *stream);
void sock_stream_close(sock_stream_t *stream);
ssize_t sock_stream_read(sock_stream_t *stream, void* buffer, size_t bytes);
#endif // SOCK_STREAM_H
