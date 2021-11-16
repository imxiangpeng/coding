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
    enum {
        // Note: BOOT_FINISHED must remain this value, it is called from
        // Java by ActivityManagerService.
        INIT = IBinder::FIRST_CALL_TRANSACTION,
        OPEN_VIDEO_STREAM,
        OPEN_AUDIO_STREAM,
        ACQUIRE_VIDEO_STREAM_BUFFER,
        RELEASE_VIDEO_STREAM_BUFFER

    };
    virtual status_t onTransact(uint32_t code, const Parcel& data,
            Parcel* reply, uint32_t flags = 0);
};

}; // namespace android
#endif // ISUPERSTREAM_H
