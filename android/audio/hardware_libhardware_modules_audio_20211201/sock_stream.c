
#define LOG_TAG "audio_sock_stream"

#include "sock_stream.h"
#include <pthread.h>
#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <log/log.h>

#define DUMP_ENABLE (0)

#define LISTEN_PORT (7788)

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(exp) (exp) // KISS implementation
#endif


#define LISTEN_BACKLOG 4

/*the len must be power of 2 */
#define MAX_CYCLE_BUFFER_LEN (0x80000) /*(0x10000)*/ /*1M/ 4k = 256 , it means we can transport 256 sections at least*/
#define MIN(X, Y) (X) > (Y) ? (Y) : (X)
#define MAX(X, Y) (X) > (Y) > (X) : (Y)

static pthread_t g_daemon_thread_id = -1;
static cycle_buffer_t *g_cycle_buffer = NULL;
static int g_stream_enabled = 0;
static int g_peer_socket = -1;

#if DUMP_ENABLE
static int g_dump_produce_fd = -1;
//static int g_dump_consume_fd = -1;
#endif

static int cycle_buffer_init(cycle_buffer_t **cycle_buffer, unsigned int data_len){
    int size = data_len; //MAX_ESINJECT_BUFFER_LEN;
    if ( !cycle_buffer || data_len <= 0 ) {
        return -1;
    }
    if ( size & (size - 1) ) {
        ALOGE(" size must be power of 2 \n");
        return -1;
    }
    *cycle_buffer = (cycle_buffer_t *)malloc(sizeof(cycle_buffer_t));
    if ( !(*cycle_buffer) ) {
        return -1;
    }
    memset((void*) (*cycle_buffer), 0, sizeof(cycle_buffer_t));

    (*cycle_buffer)->buff_size = size;
    (*cycle_buffer)->pos_in = (*cycle_buffer)->pos_out = 0;


    (*cycle_buffer)->buff = (unsigned char *)malloc((*cycle_buffer)->buff_size);
    if ( !(*cycle_buffer)->buff ) {

        free(*cycle_buffer);
        return -1;
    }

    memset((*cycle_buffer)->buff, 0, (*cycle_buffer)->buff_size);
    pthread_mutex_init(&(*cycle_buffer)->lock, NULL);


    return 0;
}

static unsigned int cycle_buffer_destroy(cycle_buffer_t **cycle_buffer){
    if ( !cycle_buffer || !(*cycle_buffer) ) {
        return -1;
    }
    pthread_mutex_lock(&(*cycle_buffer)->lock);
    if ( (*cycle_buffer)->buff ) {
        free((*cycle_buffer)->buff);
        (*cycle_buffer)->buff = NULL;
    }
    pthread_mutex_unlock(&(*cycle_buffer)->lock);
    pthread_mutex_destroy(&(*cycle_buffer)->lock);
    memset((void *)(*cycle_buffer), 0, sizeof(cycle_buffer_t));
    free(*cycle_buffer);
    *cycle_buffer = NULL;


    return 0;
}

static int cycle_buffer_reset(cycle_buffer_t *cycle_buffer){


    if ( !cycle_buffer ) {
        return -1;
    }
    pthread_mutex_lock(&(cycle_buffer->lock));
    cycle_buffer->pos_in = cycle_buffer->pos_out = 0;
    pthread_mutex_unlock(&(cycle_buffer->lock));
    return 0;
}

static unsigned int cycle_buffer_get(cycle_buffer_t *cycle_buf, unsigned char *buf, unsigned int len){
    cycle_buffer_t *cycle_buff = cycle_buf;
    if ( !cycle_buff ) {
        ALOGE("%s(%d): invalid handle ...\n", __FUNCTION__, __LINE__);
        return 0;
    }
    unsigned int left = 0;

    pthread_mutex_lock(&(cycle_buff->lock));
    len = MIN(len, (cycle_buff->pos_in - cycle_buff->pos_out));
    left = MIN(len, cycle_buff->buff_size - (cycle_buff->pos_out & (cycle_buff->buff_size - 1)));
    memcpy((void *)buf, cycle_buff->buff + (cycle_buff->pos_out & (cycle_buff->buff_size - 1)), left);
    memcpy((void *)(buf + left), cycle_buff->buff , len - left);
    cycle_buff->pos_out += len;
    pthread_mutex_unlock(&(cycle_buff->lock));
    return len;
}

static unsigned int cycle_buffer_put(cycle_buffer_t *cycle_buf, unsigned char *buf, unsigned int len, int immediate){
    cycle_buffer_t *cycle_buff = cycle_buf;
    unsigned real_len = len;
    if ( !cycle_buff ) {

        ALOGE("%s(%d): invalid handle ...\n", __FUNCTION__, __LINE__);
        return 0;
    }
    unsigned int left = 0;
    /*2014-11-16, firstly check enough space otherwise return immediate do not wait block*/
    if( immediate)
    {
        real_len = MIN(real_len, cycle_buff->buff_size - (cycle_buff->pos_in - cycle_buff->pos_out));

        if( real_len < len)
        {
            ALOGE("%s(%d): no enough space for :%d\n", __FUNCTION__, __LINE__, len);
            return 0;
        }
    }

    pthread_mutex_lock(&(cycle_buff->lock));
    real_len = len;
    real_len = MIN(real_len, cycle_buff->buff_size - (cycle_buff->pos_in - cycle_buff->pos_out));

    if( immediate && real_len < len)
    {
        pthread_mutex_unlock(&(cycle_buff->lock));
        ALOGE("%s(%d): no enough space for :%d\n", __FUNCTION__, __LINE__, len);
        return 0;
    }
    left = MIN(real_len, cycle_buff->buff_size - (cycle_buff->pos_in & (cycle_buff->buff_size - 1)));
    memcpy((void *)(cycle_buff->buff + (cycle_buff->pos_in & (cycle_buff->buff_size - 1))), (void *)buf, left);
    memcpy((void *)cycle_buff->buff , buf + left, real_len - left);


    cycle_buff->pos_in += real_len;
    pthread_mutex_unlock(&(cycle_buff->lock));

    return real_len;
}



/* open listen() port on any interface */
static int socket_inaddr_any_server(int port, int type)
{

    int s, n;
    struct sockaddr_in addr;
    //struct sockaddr_in peer_addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    s = socket(AF_INET, type, 0);
    if (s < 0) return -1;

    n = 1;

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *) &n, sizeof(n));

    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }

    int ret = listen(s, LISTEN_BACKLOG);
    
    if (ret != 0) {
        
        close(s);
        return -1; 
    }

    return s;
}

static int _socket_wait_ready(int sock, int timeout_ms){
    if (timeout_ms <= 0) {
        return 0;
    }
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(sock, &read_set);

    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int result = TEMP_FAILURE_RETRY(select(sock + 1, &read_set, NULL, NULL, &timeout));

    return result == 1;
}

void * sock_stream_daemon(void* data) {

    fd_set read_set;
    int timeout_ms = 200 * 1000;
    unsigned char buffer[1024] = {0};
    sock_stream_t *stream = (sock_stream_t*) data;

    if (!stream) {
        //return NULL;
    }

    int server_socket = socket_inaddr_any_server(LISTEN_PORT, SOCK_STREAM);

    while (1 /*stream->is_running*/) {
        int client_socket = -1;
        FD_ZERO(&read_set);
        FD_SET(server_socket, &read_set);
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        int result = TEMP_FAILURE_RETRY(select(server_socket + 1, &read_set, NULL, NULL, &timeout));

        if (result <= 0) {
            continue;
        }

        if (FD_ISSET(server_socket, &read_set) != 0) {
            
            ALOGD("%s(%d), we got new coming \n", __FUNCTION__, __LINE__);
            struct sockaddr_in peer_addr;
            socklen_t peer_len = sizeof(peer_addr);
            client_socket = accept(server_socket, (struct sockaddr *) &peer_addr, &peer_len);
        }

        if (client_socket < 0) {
            usleep(1000 * 1000);
            continue;
        }
	
        //stream->peer_socket = client_socket;
        g_peer_socket = client_socket;

        /*setting for the accept socket*/
        if (fcntl(client_socket, F_SETFL, O_NONBLOCK) < 0) {
            ALOGD("%s(%d), can not set it to nonblock client socket:%d\n", __FUNCTION__, __LINE__, client_socket);
            close(client_socket);
            continue;
        }

        while (1 /*stream->is_running*/) {
            if (0 == _socket_wait_ready(client_socket, 1000)) {
                //ALOGD("audio socket stream timeout no stream..:%d\n", client_socket);
                continue;
            }

            memset((void*)buffer, 0, sizeof(buffer));
            int result = TEMP_FAILURE_RETRY(recv(client_socket , buffer, sizeof(buffer), 0));
            //ALOGD("audio socket stream new data size:%d...errno:%d .error:%s\n", result, errno, strerror(errno));
            if (result == -1 && errno == EAGAIN) {
                continue;
            }

            //ALOGD("audio socket stream new data size:%d....error:%s\n", result, strerror(errno));
            if (result <= 0) {
                ALOGD("audio socket stream client gone  ....\n");
                close(client_socket);
                g_peer_socket = -1;
                break;
            }
            //int l = 0;
            //while (l < result ) {
            //unsigned int ret = cycle_buffer_put(g_cycle_buffer /*stream->buffer*/, buffer + l , result - l, true);
            //l += ret;
            //}
            if (/*stream->stream_enabled*/ g_stream_enabled != 0) {
                int r = cycle_buffer_put(g_cycle_buffer /*stream->buffer*/, buffer, result, true);

                if ( r != result) {
                    ALOGD("maybe data lose %d < %d\n", r, result);
                }
#if DUMP_ENABLE
                //ALOGD("%s(%d)...\n", __FUNCTION__, __LINE__);
                if (g_dump_produce_fd/*stream->dump_produce_fd*/ > 0) {
                    write(g_dump_produce_fd/*stream->dump_produce_fd*/, buffer, r);
                }
#endif
            }
        }
    }

    return NULL;

}

sock_stream_t *sock_stream_init(void) {
    pthread_attr_t attr;
    sock_stream_t *stream = (sock_stream_t*)malloc(sizeof(sock_stream_t));
    
    //int wake_fds[2] = { -1 };
    int result = -1;

    if (!stream) {
        return NULL;
    }
    memset((void*)stream, 0, sizeof(sock_stream_t));

    if (!g_cycle_buffer) {
    // init the cycle buffer
    if ( 0 != cycle_buffer_init(&g_cycle_buffer /*stream->buffer*/, MAX_CYCLE_BUFFER_LEN)){
        free(stream);
        ALOGE("%s(%d): can not init cycle buffer \n",__FUNCTION__,__LINE__);
        return NULL;
    }
    }
    
    stream->buffer = g_cycle_buffer;

    stream->is_running = 1;

#if 0
    pipe(wake_fds);
    stream->wake_read_fd = wake_fds[0];
    stream->wake_write_fd = wake_fds[1];

    result = fcntl(stream->wake_read_fd, F_SETFL, O_NONBLOCK);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not make wake read pipe non-blocking.  errno=%d",
            errno);

    result = fcntl(stream->wake_write_fd, F_SETFL, O_NONBLOCK);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not make wake write pipe non-blocking.  errno=%d",
            errno);
#endif
    if (g_daemon_thread_id == -1) {
        pthread_attr_init(&attr);
        //pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        
        ALOGE("%s(%d): try create daemon thread \n",__FUNCTION__,__LINE__);
        result = pthread_create(&g_daemon_thread_id /*stream->thread_id*/, &attr,
                                sock_stream_daemon , NULL /*stream*/);
        pthread_attr_destroy(&attr);
        if (result != 0) {
            cycle_buffer_destroy(&stream->buffer);
            free(stream);
            ALOGE("%s(%d): result:%d, error:%s\n", __FUNCTION__, __LINE__, result, strerror(errno));
            return NULL;
        }
    }
    stream->thread_id = g_daemon_thread_id;
	
    //ALOGE("%s(%d): result:%d, error:%s\n", __FUNCTION__, __LINE__, result, strerror(errno));
    return stream;
}

void sock_stream_destroy(sock_stream_t *stream) {
    if (!stream) {
        return;
    }
    // stop thread
    ALOGD("%s(%d) is runing:%d\n", __FUNCTION__, __LINE__, stream->is_running);
    stream->is_running = 0;
#if 0
    cycle_buffer_destroy(&stream->buffer);

    ALOGD("%s(%d) is runing:%d\n", __FUNCTION__, __LINE__, stream->is_running);
    //pthread_cancel(stream->thread_id);

    ALOGD("%s(%d) is runing:%d\n", __FUNCTION__, __LINE__, stream->is_running);
    pthread_join(stream->thread_id, NULL);

    ALOGD("%s(%d) joined ... is runing:%d\n", __FUNCTION__, __LINE__, stream->is_running);
#endif
    memset((void*)stream, 0, sizeof(sock_stream_t));
    free(stream);
}
void sock_stream_open(sock_stream_t *stream) {
    if (!stream) {
        return;
    }
    ALOGD("%s(%d) open ... \n", __FUNCTION__, __LINE__);
    if (stream->buffer) {
        cycle_buffer_reset(stream->buffer);
        stream->stream_enabled = 1;
        ALOGD(" pos in:%d, pos out:%d\n", stream->buffer->pos_in, stream->buffer->pos_out);
    }
    g_stream_enabled = 1;

    if (g_peer_socket > 0) {
        write(g_peer_socket, (void*)"1", 1);
    }
#if DUMP_ENABLE
    char path[128] = { 0 };

    snprintf(path, sizeof(path), "/sdcard/stock_stream_produce_%p.wav", (void*)stream);

    g_dump_produce_fd /*stream->dump_produce_fd*/ = open(path, O_RDWR |O_TRUNC | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);

    snprintf(path, sizeof(path), "/sdcard/stock_stream_consume_%p.wav", (void*)stream);
    stream->dump_consume_fd = open(path, O_RDWR |O_TRUNC | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);
#endif
}
void sock_stream_close(sock_stream_t *stream) {
    ALOGD("%s(%d) close ... \n", __FUNCTION__, __LINE__);
    g_stream_enabled = 0;
    if (!stream) {
        return;
    }

#if DUMP_ENABLE
    /*if (stream->dump_produce_fd > 0) {
        close(stream->dump_produce_fd);
        stream->dump_produce_fd = -1;
    }*/

    if (g_dump_produce_fd > 0) {
        close(g_dump_produce_fd);
        g_dump_produce_fd = -1;
    }

    if (stream->dump_consume_fd > 0) {
        close(stream->dump_consume_fd);
        stream->dump_consume_fd = -1;
    }
#endif
    /*if (g_peer_socket > 0) {
        close(g_peer_socket);
        g_peer_socket = -1;
    }*/
    cycle_buffer_reset(stream->buffer);
    stream->stream_enabled = 0;
}
ssize_t sock_stream_read(sock_stream_t *stream, void* buffer, size_t bytes) {

    if (!stream || !buffer || bytes == 0) {
        return -1;
    }

    unsigned ret = cycle_buffer_get(stream->buffer, (unsigned char*)buffer, bytes);
#if DUMP_ENABLE
    if (stream->dump_consume_fd > 0) {
        write(stream->dump_consume_fd, buffer, ret);
    }
#endif
    return ret;
}

