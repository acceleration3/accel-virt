/* The present snapshots an image source names (docs/protocol.md, "Present
 * engine"): what a Direct3D session took of a blt-model present's buffer, in
 * its own command order, kept by the executor on the device's host GPU and
 * read here through the context plane. None without a plane that reads them
 * - contexts in a renderer process keep theirs where this cannot see. */
#ifndef AV_PRESENT_FRAMES_HPP
#define AV_PRESENT_FRAMES_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/context/device_uuid.hpp"

#include <stdint.h>

namespace av {


class SurfacePoint;

class ContextPlane;

class PresentFrames {
public:
    /* How long a present waits for its session to reach the snapshot and the
     * GPU to copy it; past it the present is refused. */
    static constexpr uint32_t wait_ms = 500u;
    static constexpr uint32_t uuid_bytes = DeviceUuid::bytes;

    PresentFrames();
    /* `gpu_uuid` is the host GPU's Vulkan device UUID, copied. */
    PresentFrames(ContextPlane &plane, const uint8_t *gpu_uuid);

    bool readable() const;
    /* Any thread: the snapshot under `key` mapped into `view`; false when
     * none comes in time or none can be read here. */
    [[nodiscard]] bool map(const SurfacePoint &frame, AvFrameView &view) const;
    void               unmap(const SurfacePoint &frame) const;
    /* Any thread: the snapshot under `frame` copied on the GPU into the lent
     * memory `copy` names, and done: ok; not_found when it never came or the
     * GPU never finished (the present is refused); not_supported where it
     * cannot be copied that way, which map reads instead. Let go with unmap
     * either way. */
    [[nodiscard]] Status copy(const SurfacePoint &frame, const AvFrameCopy &copy) const;

private:
    ContextPlane *plane_;
    DeviceUuid    gpu_uuid_;
};

}  // namespace av

#endif  // AV_PRESENT_FRAMES_HPP
