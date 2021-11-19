/*
 * superstream video input device
  */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "binder.h"

//#include "libavutil/internal.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/parseutils.h"
#include "libavutil/time.h"

#include "libavformat/avformat.h"
#include "libavformat/internal.h"

#define SUPERSTREAM_SVC_NAME "SuperStream"

#define SUPERSTREAM_VIDEO_WIDTH (1280)
#define SUPERSTREAM_VIDEO_HEIGHT (720)
#define SUPERSTREAM_VIDEO_BYTES_PER_PIXEL (4) // we use rgba format
//#define SUPERSTREAM_VIDEO_OUT_BYTES_PER_PIXEL (3)

typedef struct {
    const AVClass *class;
    int binder_token;
    struct binder_state* binder_state;
    char* device;
    int width;
    int height;
    int frame_size;
    AVRational framerate;
    //int64_t time_frame;
    int64_t frame_last;
    int64_t frame_delay;
    AVRational time_base;
    void* video_ptr;
    int8_t *video_buffer;
} SuperStreamContext;

enum {
    INIT = 1,
    OPEN_VIDEO_STREAM,
    OPEN_AUDIO_STREAM,
    ACQUIRE_VIDEO_STREAM_BUFFER,
    RELEASE_VIDEO_STREAM_BUFFER
};


static uint32_t superstream_binder_svcmgr_lookup(struct binder_state* bs, uint32_t target, const char* name) {
    uint32_t handle;
    unsigned iodata[512 / 4];
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

static int superstream_binder_service(SuperStreamContext* ctx) {

    if (!ctx) {
        return -1;
    }
    av_log(ctx, AV_LOG_ERROR, "superstream_binder_service: binder token:%d, binder_state:%p\n", ctx->binder_token, (void*)ctx->binder_state);
    if (ctx->binder_token == -1) {
        int target = superstream_binder_svcmgr_lookup(ctx->binder_state, BINDER_SERVICE_MANAGER, SUPERSTREAM_SVC_NAME);

        av_log(ctx, AV_LOG_ERROR, "superstream_binder_service: binder token:%d, target:%d\n", ctx->binder_token, target);
        ctx->binder_token = target;
    }
    return ctx->binder_token;
}

static int superstream_binder_init(SuperStreamContext* ctx) {
    unsigned iodata[512 / 4];
    struct binder_io msg, reply;
    av_log(ctx, AV_LOG_ERROR, "superstream_binder_init ... \n");
    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, "SuperStream");

    if (binder_call(ctx->binder_state, &msg, &reply, superstream_binder_service(ctx), INIT)) {
        av_log(ctx, AV_LOG_ERROR, "superstream_binder_init call error... \n");
        return -1;
    }
    av_log(ctx, AV_LOG_ERROR, "superstream_binder_init out ... \n");
    return 0;
}

static int superstream_binder_video_open(SuperStreamContext* ctx) {

    // fd
    unsigned iodata[512 / 4];
    struct binder_io msg, reply;

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, "SuperStream");
    
    av_log(ctx, AV_LOG_ERROR, "superstream_binder_video_open in ... \n");
    if (binder_call(ctx->binder_state, &msg, &reply, superstream_binder_service(ctx), OPEN_VIDEO_STREAM)) {
        printf("%s(%d): error .....\n", __FUNCTION__, __LINE__);
        return -1;
    }

    int fd = bio_get_ref(BINDER_TYPE_FD, &reply);
    printf("fd:%d\n", fd);
    return fd;
}

static int superstream_acquire_video_stream(SuperStreamContext* ctx) {
    unsigned iodata[512 / 4];
    struct binder_io msg, reply;

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, "SuperStream");

    if (binder_call(ctx->binder_state, &msg, &reply, superstream_binder_service(ctx), ACQUIRE_VIDEO_STREAM_BUFFER)) {
        printf("%s(%d): error .....\n", __FUNCTION__, __LINE__);
        return -1;
    }
    return 0;
}

static int superstream_release_video_stream(SuperStreamContext* ctx) {
    unsigned iodata[512 / 4];
    struct binder_io msg, reply;

    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0);  // strict mode header
    bio_put_string16_x(&msg, "SuperStream");

    if (binder_call(ctx->binder_state, &msg, &reply, superstream_binder_service(ctx), RELEASE_VIDEO_STREAM_BUFFER)) {
        printf("%s(%d): error .....\n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}
static void superstream_mmap_release_buffer(void* opaque, uint8_t* data) {
    // release buffer
    SuperStreamContext* c = (SuperStreamContext*)opaque;
    av_log(c, AV_LOG_ERROR, "%s(%d) opaque:%p, data:%p\n", __FUNCTION__, __LINE__, opaque, data);
    //superstream_release_video_stream(c);
}

static int superstream_read_probe(const AVProbeData* p) {
    //bs = binder_open("/dev/aosp9_binder101", 128*1024);
    return 0;
}

static int superstream_read_header(AVFormatContext* ctx) {
    SuperStreamContext* c = ctx->priv_data;
    AVStream* st = NULL;
    int err = 0;


    av_log(c, AV_LOG_ERROR, "%s(%d) binder device:%s\n", __FUNCTION__, __LINE__, c->device ? c->device : "unkown");
    c->binder_state = binder_open(c->device, 128 * 1024);
    if (!c->binder_state) {
        av_log(ctx, AV_LOG_ERROR, "can not open device:%s\n", c->device);
        err = AVERROR(ENOENT);
        goto fail;
    }

    superstream_binder_init(c);
    int fd = superstream_binder_video_open(c);
    av_log(ctx, AV_LOG_ERROR, "device:%s, video fd:%d\n", c->device, fd);
    if (fd <= 0) {
        err = AVERROR(EIO);
        av_log(ctx, AV_LOG_ERROR, "can not open video stream:%s\n", c->device);
        goto fail;
    }

    c->width = SUPERSTREAM_VIDEO_WIDTH;
    c->height = SUPERSTREAM_VIDEO_HEIGHT;
    c->frame_size = c->width * c->height * SUPERSTREAM_VIDEO_BYTES_PER_PIXEL;

    c->video_ptr = mmap(NULL, c->frame_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!c->video_ptr) {
        av_log(ctx, AV_LOG_ERROR, "can not mmap video stream:%s\n", c->device);
        err = AVERROR(ENOMEM);
        goto fail;
    }

    c->video_buffer = av_mallocz(c->frame_size);

    st = avformat_new_stream(ctx, NULL);

#if 0
    if (c->framerate &&
        av_parse_video_rate(&st->avg_frame_rate, c->framerate) < 0) {
        av_log(ctx, AV_LOG_ERROR, "Could not parse framerate '%s'.\n",
               c->framerate);
        err = AVERROR(EBADF);
        goto fail;
    }
#endif

    c->time_base = st->avg_frame_rate = (AVRational) { c->framerate.den, c->framerate.num };

    c->frame_delay =  av_rescale_q(1, c->time_base, AV_TIME_BASE_Q);
    c->frame_last = av_gettime_relative();

    st->codecpar->format = AV_PIX_FMT_RGBA;
    st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    st->codecpar->codec_id   = AV_CODEC_ID_RAWVIDEO;
    st->codecpar->width      = c->width;
    st->codecpar->height     = c->height;


    avpriv_set_pts_info(st, 64, 1, 1000000); /* 64 bits pts in us */

    //if (st->avg_frame_rate.den)
    //st->codecpar->bit_rate = c->frame_size * av_q2d(st->avg_frame_rate) * 8;

    st->codecpar->bit_rate   = av_rescale(c->frame_size * 8, st->avg_frame_rate.num, st->avg_frame_rate.den);

    av_log(ctx, AV_LOG_ERROR, "frame_duration:%ld, time_frame:%ld, bitrate:%ld, frame rate num:%d, frame rate den:%d\n", c->frame_delay, c->frame_last, st->codecpar->bit_rate, st->avg_frame_rate.num, st->avg_frame_rate.den);

    return 0;
 fail:
    if (c->binder_state) {
        binder_close(c->binder_state);
        c->binder_state = NULL;
    }

    return err;
}


static int superstream_read_packet(AVFormatContext* ctx, AVPacket* pkt) {
    SuperStreamContext* c = ctx->priv_data;
    int64_t now = 0;

    now = av_gettime_relative();

    //av_log(ctx, AV_LOG_ERROR, "%s(%d) frame_duration:%ld, time_frame:%ld, current pts:%ld\n", __FUNCTION__, __LINE__, c->frame_delay, c->frame_last, now);
    if (c->frame_last) {
        int64_t delay;
        while (1) {
            delay = c->frame_last + c->frame_delay - now;
            if (delay <= 0)
                break;
            //av_log(ctx, AV_LOG_ERROR, "%s(%d) frame last:%ld, frame delay:%ld, delay:%ld\n", __FUNCTION__, __LINE__, c->frame_last, c->frame_delay, delay);
            av_usleep(delay);
            now = av_gettime_relative();
        }
    }
    c->frame_last = now;
    now = av_gettime();

    //av_log(ctx, AV_LOG_ERROR, "%s(%d) frame_duration:%ld, time_frame:%ld, current pts:%ld\n", __FUNCTION__, __LINE__, c->frame_delay, c->frame_delay, now);

    // got data
    // convert rgba to rgb
    //void* mmap_ptr = NULL;
    //pkt->data = mmap_ptr;
    //if (0 != superstream_acquire_video_stream(c)) {
    //av_log(ctx, AV_LOG_ERROR, "%s(%d) can not request the video stream\n", __FUNCTION__, __LINE__);
    //return AVERROR_EXTERNAL;
    //}

    //memmove((void*)c->video_buffer, c->video_ptr, c->frame_size);
    pkt->data = c->video_ptr;
    //pkt->data = c->video_buffer;
    pkt->size = c->frame_size;
    pkt->pts = pkt->dts = now;
    //pkt->buf = av_buffer_create(pkt->data, pkt->size, superstream_mmap_release_buffer, (void*)c, 0);
    //pkt->buf = av_buffer_create((uint8_t*)c->video_buffer, c->frame_size, &kmsgrab_free_desc, avctx, 0);
    //pkt->flags |= AV_PKT_FLAG_TRUSTED;

    //superstream_release_video_stream(c);

    return c->frame_size;
}

static int superstream_read_close(AVFormatContext* ctx) {

    return 0;
}
#define OFFSET(x) offsetof(SuperStreamContext, x)
#define DEC AV_OPT_FLAG_DECODING_PARAM

static const AVOption options[] = {
    { "device",     "binder device",       OFFSET(device),     AV_OPT_TYPE_STRING, { .str = "/dev/aosp9_binder101" }, 0, 0,       DEC },
    //{ "framerate", "", OFFSET(framerate), AV_OPT_TYPE_STRING, { .str = "ntsc" }, 0, 0, DEC },
    { "framerate", "Framerate to capture at", OFFSET(framerate), AV_OPT_TYPE_RATIONAL, { .dbl = 30.0 }, 0, 1000, DEC },
    { "binder_token", "binder token.", OFFSET(binder_token), AV_OPT_TYPE_INT, { .i64 = -1 }, -1, INT_MAX, DEC },
    { NULL },
};

static const AVClass superstream_class = {
    .class_name = "superstream indev",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
    .category   = AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT,
};
const AVInputFormat ff_superstream_demuxer = {
    .name           = "superstream",
    .long_name      = NULL_IF_CONFIG_SMALL("Share memory stream with SuperStream"),
    .priv_data_size = sizeof(SuperStreamContext),
    //.read_probe    = superstream_read_probe,
    .read_header    = superstream_read_header,
    .read_packet    = superstream_read_packet,
    .read_close     = superstream_read_close,
    .flags          = AVFMT_NOFILE,
    .priv_class     = &superstream_class,
};
