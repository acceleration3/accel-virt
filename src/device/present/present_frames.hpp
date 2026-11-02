#ifndef AV_PRESENT_FRAMES_HPP
#define AV_PRESENT_FRAMES_HPP

#include "device/abi/av_executor.h"
#include "proto/context/device_uuid.hpp"

#include <stdint.h>

namespace av {


class SurfacePoint;

class ContextPlane;

class PresentFrames {
public:
    static constexpr uint32_t wait_ms = 500u;
    static constexpr uint32_t uuid_bytes = DeviceUuid::bytes;

    PresentFrames();
    PresentFrames(ContextPlane &plane, const uint8_t *gpu_uuid);

    bool readable() const;
    [[nodiscard]] bool map(const SurfacePoint &frame, AvFrameView &view) const;
    void               unmap(const SurfacePoint &frame) const;

private:
    ContextPlane *plane_;
    DeviceUuid    gpu_uuid_;
};

}

#endif
