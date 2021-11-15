
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

namespace android {

SuperStream::SuperStream()
    :mAshmemFd (-1),
     mAshmemSize(-1),
     mAshmemPtr(nullptr){

}
SuperStream::~SuperStream(){

}
void SuperStream::deInit(){
  if (mAshmemFd > 0) {
    if (mAshmemPtr != nullptr) {
      munmap(mAshmemPtr, mAshmemSize);
      mAshmemPtr = nullptr;
      mAshmemSize = 0;
    }
    close(mAshmemFd);
    mAshmemFd = -1;
  }
}
int SuperStream::init(){
    size_t len  = 1280 * 720 * 8;
    int fd = ashmem_create_region("SuperStream", len);
    if (fd < 0) return NO_MEMORY;

    int result = ashmem_set_prot_region(fd, PROT_READ | PROT_WRITE);
    if (result < 0) {
      return result;
    }
    void* ptr = ::mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
      ::close(fd);
      return -errno;
    }

    mAshmemFd = fd;
    mAshmemPtr = ptr;
    mAshmemSize = len;
    memcpy(ptr,  (void*)"mxp.develop .....\n", strlen("mxp.develop .....\n"));
    return NO_ERROR;
}
status_t SuperStream::onTransact(uint32_t code, const Parcel& data,
                                 __unused Parcel* reply, uint32_t flags) {
    ALOGE("%s(%d): code:0x%x, flags:0x%x\n", __FUNCTION__, __LINE__, code, flags);

    switch (code) {
      case INIT:
        init();
        reply->writeFileDescriptor(mAshmemFd, false);
      break;
      default:
        break;
    }

    BnSuperStream::onTransact(code, data, reply, flags);
    return NO_ERROR;
}

} // namespace android
