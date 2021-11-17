#ifndef VIDEOSTREAM_H
#define VIDEOSTREAM_H


#include <gui/IGraphicBufferProducer.h>

#include "FrameOutput.h"

namespace android {


class VideoStream : public Thread {
 public:
    VideoStream();
    ~VideoStream();
    
    int open();
    void close();
    status_t acquireBuffer();
    void releaseBuffer();
 private:

    bool prepare();
    status_t readyToRun()override;
    bool threadLoop();

    sp<FrameOutput> mFrameOutput;

    sp<IGraphicBufferProducer> mBufferProducer;
    sp<IBinder> mVirtualDisplay;
    uint32_t mWidth;
    uint32_t mHeight;
    // for test
    //int mFd;
    int mAshmemFd;
    size_t mAshmemSize;
    void* mAshmemPtr;
    Mutex mMutex;
    bool mReadPending;
};
}; // namespace android
#endif // VIDEOSTREAM_H
