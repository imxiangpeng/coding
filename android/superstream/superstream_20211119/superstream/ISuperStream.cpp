#include "ISuperStream.h"

namespace android {
class BpSuperStream : public BpInterface<ISuperStream> {
public:
    explicit BpSuperStream(const sp<IBinder>& impl);
    virtual ~BpSuperStream();
};

BpSuperStream::BpSuperStream(const sp<IBinder>& impl)
    :BpInterface<ISuperStream> (impl) {
    (void)impl;
}
BpSuperStream::~BpSuperStream(){}

IMPLEMENT_META_INTERFACE(SuperStream, "ISuperStream");

status_t BnSuperStream::onTransact(
    __unused uint32_t code, __unused const Parcel& data, __unused Parcel* reply, __unused uint32_t flags)
{
    return BBinder::onTransact(code, data, reply, flags);
}
}; // namespace android
