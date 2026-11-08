#ifndef AV_PRESENT_FRAMES_HPP
#define AV_PRESENT_FRAMES_HPP

#include "device/abi/av_executor.h"
#include "proto/context/device_uuid.hpp"

#include <stdint.h>

namespace av {


class PresentRegion;
class SurfacePair;
class SurfacePoint;

class ContextPlane;

class PresentFrames {
public:
    static constexpr uint32_t wait_ms = 500u;
    static constexpr uint32_t copy_wait_ms = 2000u;
    static constexpr uint32_t surface_wait_ms = 2000u;
    static constexpr uint32_t copy_regions = 256u;
    static constexpr uint32_t uuid_bytes = DeviceUuid::bytes;

    PresentFrames();
    PresentFrames(ContextPlane &plane, const uint8_t *gpu_uuid);

    bool readable() const;
    [[nodiscard]] bool map(const SurfacePoint &frame, AvFrameView &view) const;
    void               unmap(const SurfacePoint &frame) const;
    bool               copies() const;
    [[nodiscard]] bool map_surface(uint64_t surface, AvFrameView &view) const;
    void               unmap_surface(uint64_t surface) const;
    [[nodiscard]] bool copy(const SurfacePair &surfaces, const PresentRegion *regions, uint32_t count) const;

private:
    ContextPlane *plane_;
    DeviceUuid    gpu_uuid_;
};

}

#endif
