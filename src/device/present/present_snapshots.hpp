/* The present snapshots an image source names (docs/protocol.md, "Present
 * engine"): what a Direct3D session took of a blt-model present's buffer, in
 * its own command order, kept by the executor on the device's host GPU and
 * read here through the context plane. None without a plane that reads them
 * - contexts in a renderer process keep theirs where this cannot see. */
#ifndef AV_PRESENT_SNAPSHOTS_HPP
#define AV_PRESENT_SNAPSHOTS_HPP

#include "device/abi/rvk_executor.h"

#include <stdint.h>

namespace av {

class ContextPlane;

class PresentSnapshots {
public:
    /* How long a present waits for its session to reach the snapshot and the
     * GPU to copy it; past it the present is refused. */
    static constexpr uint32_t wait_ms = 500u;
    static constexpr uint32_t uuid_bytes = 16u;

    PresentSnapshots();
    /* `gpu_uuid` is the host GPU's Vulkan device UUID, copied. */
    PresentSnapshots(ContextPlane &plane, const uint8_t *gpu_uuid);

    bool readable() const;
    /* Any thread: the snapshot under `key` mapped into `view`; false when
     * none comes in time or none can be read here. */
    [[nodiscard]] bool map(uint64_t key, RvkSnapshotView &view) const;
    void               unmap(uint64_t key) const;

private:
    ContextPlane *plane_;
    uint8_t       gpu_uuid_[uuid_bytes];
};

}  // namespace av

#endif  // AV_PRESENT_SNAPSHOTS_HPP
