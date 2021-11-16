
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

namespace android {

static const int kGlBytesPerPixel = 4;      // GL_RGBA

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
    mAshmemPtr(nullptr) {


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
    //memcpy(ptr,  (void*)"mxp.develop .....\n", strlen("mxp.develop .....\n"));
#if 0
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
#endif
    mFd = ::open("/data/local/tmp/VideoStream.raw", O_RDWR | O_TRUNC | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);

    if (mFd < 0) {
        ALOGD("VideoStream::%s(%d) can not open the file:%d\n", __FUNCTION__, __LINE__, mFd);
    }
#if 0
    int i = 0;
    while (i++ < 200) {
        err = mFrameOutput->copyFrame(mFd, 250000, false);

        ALOGD("VideoStream::%s(%d) status:%d\n", __FUNCTION__, __LINE__, err);
    }
#endif
    return true;
}

int VideoStream::open(){

    ALOGE("VideoStream::%s(%d): come in \n", __FUNCTION__, __LINE__);
    prepare();
    run("VideoStream");
    return mAshmemFd;
}
status_t VideoStream::acquireBuffer() {

    ALOGE("VideoStream::%s(%d): come in \n", __FUNCTION__, __LINE__);
    return mMutex.lock();
}
void VideoStream::releaseBuffer() {

    ALOGE("VideoStream::%s(%d): come in \n", __FUNCTION__, __LINE__);
    mMutex.unlock();
}

status_t VideoStream::readyToRun() {

    ALOGD("VideoStream::%s(%d) come in ...fd:%d\n", __FUNCTION__, __LINE__, mFd);
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
    //ALOGD("VideoStream::%s(%d) come in ...fd:%d\n", __FUNCTION__, __LINE__, mFd);

    err = mFrameOutput->doComposition(250000);

    if (NO_ERROR != err) {
        return true;
    }
    if (NO_ERROR == mMutex.tryLock()) {
        size_t len = mFrameOutput->readFramePixels(mAshmemPtr, mAshmemSize);

        // debug
        if (mFd > 0) {
            android::base::WriteFully(mFd, mAshmemPtr, len);
        }
        mMutex.unlock();
    }
    return true;
}
} // namespace android

