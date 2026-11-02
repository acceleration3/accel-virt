#ifndef AV_HARDWARE_CURSOR_HPP
#define AV_HARDWARE_CURSOR_HPP

#include "core/platform/platform.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DisplaySources;
class VirtioGpuDevice;
struct DxgkargSetPointerPositionRecord;
struct DxgkargSetPointerShapeRecord;

class HardwareCursor {
public:
    static constexpr uint32_t resource_id = 1u;
    static constexpr uint32_t max_side = 64u;

    HardwareCursor(Platform &platform, const DisplaySources &sources, ObjectStorage<VirtioGpuDevice> &device);
    ~HardwareCursor();
    HardwareCursor(const HardwareCursor &) = delete;
    HardwareCursor &operator=(const HardwareCursor &) = delete;

    [[nodiscard]] Status initialize();
    void                 release();
    void                 forget_device();
    [[nodiscard]] Status set_shape(const DxgkargSetPointerShapeRecord &shape);
    [[nodiscard]] Status set_position(const DxgkargSetPointerPositionRecord &position);

private:
    static constexpr uint32_t flag_visible = 1u;
    static constexpr uint32_t scanout = 0u;

    [[nodiscard]] Status create_resource(VirtioGpuDevice &device);
    [[nodiscard]] Status update(VirtioGpuDevice &device, uint32_t resource);

    Platform                       &platform_;
    const DisplaySources           &sources_;
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

}

#endif
