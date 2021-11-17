
#include "SuperStream.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <cutils/ashmem.h>
#include <utils/Log.h>
#include <binder/Parcel.h>
#include <binder/BpBinder.h>

namespace android {

SuperStream::SuperStream() {

    mVideoStream = new VideoStream();

}
SuperStream::~SuperStream() {

}
void SuperStream::deInit() {

}
int SuperStream::init() {

    ALOGE("%s(%d): come in \n", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

int SuperStream::openVideoStream() {

    ALOGE("%s(%d): come in \n", __FUNCTION__, __LINE__);
    return mVideoStream->open();
}
void SuperStream::closeVideoStream() {
    mVideoStream->close();
}
void SuperStream::acquireVideoStreamBuffer() {

    ALOGE("%s(%d): come in \n", __FUNCTION__, __LINE__);
    mVideoStream->acquireBuffer();

    ALOGE("%s(%d): come out \n", __FUNCTION__, __LINE__);
}
void SuperStream::releaseVideoStreamBuffer() {

    ALOGE("%s(%d): come in \n", __FUNCTION__, __LINE__);
    mVideoStream->releaseBuffer();
}
status_t SuperStream::onTransact(uint32_t code, const Parcel& data,
                                 __unused Parcel* reply, uint32_t flags) {
    ALOGE("%s(%d): code:0x%x, flags:0x%x\n", __FUNCTION__, __LINE__, code, flags);

    switch (code) {

      case INIT:
          init();
          break;
      case OPEN_VIDEO_STREAM:
          reply->writeFileDescriptor(openVideoStream(), false);
          break;
      case ACQUIRE_VIDEO_STREAM_BUFFER:
          acquireVideoStreamBuffer();
          break;
      case RELEASE_VIDEO_STREAM_BUFFER:
          releaseVideoStreamBuffer();
          break;
      default:
          break;
    }

    BnSuperStream::onTransact(code, data, reply, flags);
    return NO_ERROR;
}

} // namespace android
