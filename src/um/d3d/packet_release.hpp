#ifndef AV_UM_PACKET_RELEASE_HPP
#define AV_UM_PACKET_RELEASE_HPP

#include "core/platform/thread_lock.hpp"
#include "guest/transport/release_route.hpp"
#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {

class KeyedLog;
class KmtTransport;

}

namespace av::d3d {

class PacketRelease final : public ReleaseRoute {
public:
    PacketRelease(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, KmtTransport &transport,
                      ThreadLock &lock, KeyedLog &lines, HANDLE context);
    ~PacketRelease();
    PacketRelease(const PacketRelease &) = delete;
    PacketRelease &operator=(const PacketRelease &) = delete;

    void engage();
    void writing(D3DKMT_HANDLE allocation);
    bool engaged() const;
    Status release(const uint8_t *record, uint32_t size) override;
    void   waited_long() override;

private:
    const D3DDDI_DEVICECALLBACKS &kernel_;
    HANDLE                        device_;
    KmtTransport                 &transport_;
    ThreadLock                   &lock_;
    KeyedLog                     &lines_;
    HANDLE                        context_;
    bool                          engaged_;
    D3DKMT_HANDLE                 written_;
};

}

#endif
