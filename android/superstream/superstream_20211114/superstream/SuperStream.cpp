
#include "SuperStream.h"

#include <utils/Log.h>

namespace android {

SuperStream::SuperStream(){

}
SuperStream::~SuperStream(){

}
status_t SuperStream::onTransact(uint32_t code, const Parcel& data,
                                 __unused Parcel* reply, uint32_t flags) {
    ALOGE("%s(%d): code:0x%x, flags:0x%x\n", __FUNCTION__, __LINE__, code, flags);
    BnSuperStream::onTransact(code, data, reply, flags);
    return NO_ERROR;
}

} // namespace android
