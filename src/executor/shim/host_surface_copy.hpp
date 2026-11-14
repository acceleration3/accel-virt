#ifndef AV_HOST_SURFACE_COPY_HPP
#define AV_HOST_SURFACE_COPY_HPP

#include "device/abi/av_executor.h"
#include "device/shim/posix_lock.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/host_format_converter.hpp"

#include <stdint.h>

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct ID3D11Fence;
struct ID3D11Texture2D;

namespace av {

struct HostSurfaceCopyProbe;

class HostSurfaceCopy {
public:
    HostSurfaceCopy();
    ~HostSurfaceCopy();
    HostSurfaceCopy(const HostSurfaceCopy &) = delete;
    HostSurfaceCopy &operator=(const HostSurfaceCopy &) = delete;

    [[nodiscard]] bool submit(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                              ID3D11Texture2D *destination, const AvCopyRegion *regions, uint32_t count,
                              ID3D11Fence *&fence, uint64_t &value);
    [[nodiscard]] bool wait(ID3D11Fence *fence, uint64_t value, uint32_t wait_ms);
    void               release();

private:
    [[nodiscard]] bool compatible(ID3D11Texture2D *source, ID3D11Texture2D *destination, uint32_t &width,
                                  uint32_t &height) const;
    ID3D11Texture2D   *converted(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                                 ID3D11Texture2D *destination, uint32_t &width, uint32_t &height);
    [[nodiscard]] bool fits(const AvCopyRegion &region, uint32_t width, uint32_t height) const;
    [[nodiscard]] bool arm(ID3D11Device5 *device);
    void               drain();

    friend struct HostSurfaceCopyProbe;

    ID3D11Fence   *fence_;
    uint64_t       value_;
    OwnedFd        event_;
    PosixLock      waiting_;
    MonotonicClock clock_;
    HostFormatConverter converter_;
};

}

#endif
