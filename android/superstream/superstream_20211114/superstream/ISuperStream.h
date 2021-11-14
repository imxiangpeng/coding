#ifndef ISUPERSTREAM_H
#define ISUPERSTREAM_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <binder/IInterface.h>

namespace android {

class ISuperStream: public IInterface {
public:
    DECLARE_META_INTERFACE(SuperStream)

};

class BnSuperStream: public BnInterface<ISuperStream> {
public:
    virtual status_t onTransact(uint32_t code, const Parcel& data,
            Parcel* reply, uint32_t flags = 0);
};

}; // namespace android
#endif // ISUPERSTREAM_H
