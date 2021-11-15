#ifndef SUPERSTREAM_H
#define SUPERSTREAM_H
#include "ISuperStream.h"

namespace android {
class SuperStream : public BnSuperStream {
public:
    SuperStream();
    static char const* getServiceName(void) {
        return "SuperStream";
    }
private:
    int mAshmemFd;
    size_t mAshmemSize;
    void* mAshmemPtr;
    // We're reference counted, never destroy SurfaceFlinger directly
    virtual ~SuperStream();
    int init();
    void deInit();
    status_t onTransact(uint32_t code, const Parcel& data,
            Parcel* reply, uint32_t flags = 0) override;
};
}; // namespace android
#endif // SUPERSTREAM_H
