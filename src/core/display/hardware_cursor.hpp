/* dxgkrnl's pointer on the virtio-gpu cursor (virtio 1.2 5.7.6.9): resource 1,
 * a 64x64 B8G8R8A8 image in one contiguous backing, created and attached once
 * per device, transferred per shape, and shown, moved and hidden on the cursor
 * queue. Requests are not waited on; the DPC reclaims them. Pointer DDIs are
 * PASSIVE and serialized by dxgkrnl. */
#ifndef AV_HARDWARE_CURSOR_HPP
#define AV_HARDWARE_CURSOR_HPP

#include "core/platform/platform.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class VirtioGpuDevice;
struct DxgkargSetPointerPositionRecord;
struct DxgkargSetPointerShapeRecord;

class HardwareCursor {
public:
    /* Reserved for the cursor (docs/protocol.md). */
    static constexpr uint32_t resource_id = 1u;
    static constexpr uint32_t max_side = 64u;

    HardwareCursor(Platform &platform, ObjectStorage<VirtioGpuDevice> &device);
    ~HardwareCursor();
    HardwareCursor(const HardwareCursor &) = delete;
    HardwareCursor &operator=(const HardwareCursor &) = delete;

    /* The backing: StartDevice. */
    [[nodiscard]] Status initialize();
    void                 release();
    /* A new device has none of the old one's resources. */
    void                 forget_device();
    [[nodiscard]] Status set_shape(const DxgkargSetPointerShapeRecord &shape);
    [[nodiscard]] Status set_position(const DxgkargSetPointerPositionRecord &position);

private:
    static constexpr uint32_t flag_visible = 1u;   /* DXGK_SETPOINTERPOSITIONFLAGS.Visible */
    static constexpr uint32_t scanout = 0u;

    [[nodiscard]] Status create_resource(VirtioGpuDevice &device);
    /* UPDATE_CURSOR: this image at the last position, or resource 0 to hide. */
    [[nodiscard]] Status update(VirtioGpuDevice &device, uint32_t resource);

    Platform                       &platform_;
    ObjectStorage<VirtioGpuDevice> &device_;
    uint8_t                        *backing_;
    bool                            resource_created_;
    bool                            shape_set_;
    bool                            visible_;
    uint32_t                        hot_x_;
    uint32_t                        hot_y_;
    int32_t                         x_;
    int32_t                         y_;
};

}  // namespace av

#endif  // AV_HARDWARE_CURSOR_HPP
