#ifndef AV_UM_BATCH_FOLLOWER_HPP
#define AV_UM_BATCH_FOLLOWER_HPP

#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {

class KmtTransport;

}

namespace av::d3d {

class BatchFollower {
public:
    BatchFollower(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, KmtTransport &transport);
    BatchFollower(const BatchFollower &) = delete;
    BatchFollower &operator=(const BatchFollower &) = delete;

    [[nodiscard]] HRESULT follow(HANDLE context);

private:
    const D3DDDI_DEVICECALLBACKS &kernel_;
    HANDLE                        device_;
    KmtTransport                 &transport_;
    HANDLE                        followed_context_;
    uint64_t                      followed_fence_;
};

}

#endif
