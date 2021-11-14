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
    // We're reference counted, never destroy SurfaceFlinger directly
    virtual ~SuperStream();
    status_t onTransact(uint32_t code, const Parcel& data,
            Parcel* reply, uint32_t flags = 0) override;
};
}; // namespace android
#endif // SUPERSTREAM_H
