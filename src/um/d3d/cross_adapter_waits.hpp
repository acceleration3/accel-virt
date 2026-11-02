#ifndef AV_UM_CROSS_ADAPTER_WAITS_HPP
#define AV_UM_CROSS_ADAPTER_WAITS_HPP

#include "core/platform/thread_lock.hpp"
#include "guest/transport/release_route.hpp"
#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {

class KeyedLog;
class KmtTransport;

}

namespace av::d3d {

class CrossAdapterWaits final : public ReleaseRoute {
public:
    CrossAdapterWaits(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, KmtTransport &transport,
                      ThreadLock &lock, KeyedLog &lines, HANDLE context);
    ~CrossAdapterWaits();
    CrossAdapterWaits(const CrossAdapterWaits &) = delete;
    CrossAdapterWaits &operator=(const CrossAdapterWaits &) = delete;

    void shares();
    bool shared() const;
    Status release(const uint8_t *record, uint32_t size) override;
    void   waited_long() override;

private:
    const D3DDDI_DEVICECALLBACKS &kernel_;
    HANDLE                        device_;
    KmtTransport                 &transport_;
    ThreadLock                   &lock_;
    KeyedLog                     &lines_;
    HANDLE                        context_;
    bool                          shared_;
};

}

#endif
