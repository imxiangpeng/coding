
#include "VideoStream.h"

#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/DisplayInfo.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <utils/threads.h>

#include <cutils/ashmem.h>

#include <android-base/file.h>
#include <utils/Log.h>

// read pixel in special thread or not
// how detect remote client die ?
// #define THREAD_READ 1

#define VIDEOSTREAM_CAPTURE_FPS (30)
#define VIDEOSTREAM_CAPTURE_TIME_PER_FRAME_US (1000 * 1000 / VIDEOSTREAM_CAPTURE_FPS)

namespace android {

// mxp, 20211117, we use large buffer
// first w * h * 3 is reserved for rgb24
// last w * h * 4 is rgba which read from gl
static const int kGlBytesPerPixel = 4 + 3;

static inline uint32_t floorToEven(uint32_t num) {
    return num & ~1;
}


/*
 * Configures the virtual display.  When this completes, virtual display
 * frames will start arriving from the buffer producer.
 */
static status_t prepareVirtualDisplay(const DisplayInfo& mainDpyInfo,
                                      const sp<IGraphicBufferProducer>& bufferProducer,
                                      sp<IBinder>* pDisplayHandle) {
    sp<IBinder> dpy = SurfaceComposerClient::createDisplay(
        String8("SuperStream.VideoStream"), false /*secure*/);

    SurfaceComposerClient::Transaction t;
    t.setDisplaySurface(dpy, bufferProducer);
    Rect rect(mainDpyInfo.w, mainDpyInfo.h);
    t.setDisplayProjection(dpy, DISPLAY_ORIENTATION_0, rect, rect);
    t.setDisplayLayerStack(dpy, 0);    // default stack
    t.apply();

    *pDisplayHandle = dpy;

    return NO_ERROR;
}

VideoStream::VideoStream() : Thread(false),
    mAshmemFd(-1),
    mAshmemSize(-1),
    mAshmemPtr(nullptr),
    mReadPending(false) {


}
VideoStream::~VideoStream() {

}

bool VideoStream::prepare() {

    ALOGE("VideoStream::%s(%d): come in \n", __FUNCTION__, __LINE__);
    DisplayInfo mainDpyInfo;
    // Get main display parameters.
    sp<IBinder> mainDpy = SurfaceComposerClient::getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain);
    status_t err = SurfaceComposerClient::getDisplayInfo(mainDpy, &mainDpyInfo);
    if (err != NO_ERROR) {
        ALOGE("VideoStream::%s(%d): can not get display information...\n", __FUNCTION__, __LINE__);
        return false;
    }
    mWidth = floorToEven(mainDpyInfo.w);
    mHeight = floorToEven(mainDpyInfo.h);
    ALOGD("%s(%d): display: %d x %d orientation:%d\n", __FUNCTION__, __LINE__, mainDpyInfo.w, mainDpyInfo.h, mainDpyInfo.orientation);

    mAshmemSize = mWidth * mHeight * kGlBytesPerPixel;
    int fd = ashmem_create_region("SuperStream.Video", mAshmemSize);
    if (fd < 0) return NO_MEMORY;

    int result = ashmem_set_prot_region(fd, PROT_READ | PROT_WRITE);
    if (result < 0) {
        return false;
    }
    void* ptr = ::mmap(NULL, mAshmemSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        ::close(fd);
        return false;
    }

    mAshmemFd = fd;
    mAshmemPtr = ptr;

    /*mFd = ::open("/data/local/tmp/VideoStream.raw", O_RDWR | O_TRUNC | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);

    if (mFd < 0) {
        ALOGD("VideoStream::%s(%d) can not open the file:%d\n", __FUNCTION__, __LINE__, mFd);
    }*/

    return true;
}

int VideoStream::open() {

    if (mAshmemFd > 0) {
        ALOGE("VideoStream::%s(%d): have opened \n", __FUNCTION__, __LINE__);
        return mAshmemFd;
    }

    prepare();

    run("VideoStream");

    return mAshmemFd;
}
void VideoStream::close() {
    requestExit();

    mMutex.unlock();
}
status_t VideoStream::acquireBuffer() {

    //ALOGE("VideoStream::%s(%d): come in \n", __FUNCTION__, __LINE__);
    //nsize_t len = mFrameOutput->readFramePixels(mAshmemPtr, mAshmemSize);
    //return 0;
    //return mMutex.lock();
    status_t ret = mMutex.timedLock(1000000000 / 10);
    if (ret != 0) {
        ALOGE("VideoStream::%s(%d): warning .....wait lock timeout ret:%d\n", __FUNCTION__, __LINE__, ret);
    }
    return ret;
}
void VideoStream::releaseBuffer() {

    //ALOGE("VideoStream::%s(%d): come in \n", __FUNCTION__, __LINE__);
    mMutex.unlock();
}

status_t VideoStream::readyToRun() {

    DisplayInfo mainDpyInfo;
    // Get main display parameters.
    sp<IBinder> mainDpy = SurfaceComposerClient::getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain);
    status_t err = SurfaceComposerClient::getDisplayInfo(mainDpy, &mainDpyInfo);
    if (err != NO_ERROR) {
        ALOGE("VideoStream::%s(%d): can not get display information...\n", __FUNCTION__, __LINE__);
        return false;
    }
    mFrameOutput = new FrameOutput();

    err = mFrameOutput->createInputSurface(mWidth, mHeight, &mBufferProducer);

    prepareVirtualDisplay(mainDpyInfo, mBufferProducer, &mVirtualDisplay);
    // TODO: if we want to make this a proper feature, we should output
    //       an outer header with version info.  Right now we never change
    //       the frame size or format, so we could conceivably just send
    //       the current frame header once and then follow it with an
    //       unbroken stream of data.

    // Make the EGL context current again.  This gets unhooked if we're
    // using "--bugreport" mode.
    // TODO: figure out if we can eliminate this
    mFrameOutput->prepareToCopy();
    return NO_ERROR;
}


bool VideoStream::threadLoop() {
    status_t err = NO_ERROR;

    uint64_t startTime = systemTime(CLOCK_MONOTONIC);

    err = mFrameOutput->doComposition(mReadPending ? 0 : VIDEOSTREAM_CAPTURE_TIME_PER_FRAME_US);

    if (mReadPending || NO_ERROR == err) {
        if (NO_ERROR == mMutex.tryLock()) {

            size_t len = mFrameOutput->readFramePixels(mAshmemPtr, mAshmemSize);
            // debug
            //if (mFd > 0) {
            //android::base::WriteFully(mFd, mAshmemPtr, len);
            //}
            mMutex.unlock();
            mReadPending = false;
        } else {
            mReadPending = true;
        }

    }
    uint64_t endTime = systemTime(CLOCK_MONOTONIC);
    int64_t delay = (endTime - startTime) / 1000 - VIDEOSTREAM_CAPTURE_TIME_PER_FRAME_US;
    
    if (delay > 0) {
        usleep(delay);
    }

    return true;
}
} // namespace android

