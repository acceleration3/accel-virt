#ifndef AV_STANDARD_ALLOCATION_HPP
#define AV_STANDARD_ALLOCATION_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class AllocationDescriptor;
class SurfaceLayout;
struct DxgkargGetStandardAllocationDriverDataRecord;

class StandardAllocation {
public:
    StandardAllocation(Platform &platform, DxgkargGetStandardAllocationDriverDataRecord &request);
    StandardAllocation(const StandardAllocation &) = delete;
    StandardAllocation &operator=(const StandardAllocation &) = delete;

    [[nodiscard]] Status describe();

private:
    static constexpr uint32_t staging_format = 21u;
    enum : int32_t { shared_primary = 1, shadow = 2, staging = 3, gdi = 4 };
    enum : int32_t { gdi_texture = 1, gdi_staging_cpu_visible = 2, gdi_staging = 3, gdi_lookup_table = 4,
                     gdi_existing_sys_mem = 5, gdi_texture_cpu_visible = 6, gdi_texture_cross_adapter = 7,
                     gdi_texture_cpu_visible_cross_adapter = 8 };

    [[nodiscard]] Status layout_of(uint32_t width, uint32_t height, uint32_t format, SurfaceLayout &layout) const;
    [[nodiscard]] Status describe_shared_primary(AllocationDescriptor &descriptor) const;
    [[nodiscard]] Status describe_shadow(AllocationDescriptor &descriptor) const;
    [[nodiscard]] Status describe_staging(AllocationDescriptor &descriptor) const;
    [[nodiscard]] Status describe_gdi(AllocationDescriptor &descriptor) const;
    [[nodiscard]] Status describe_cross_adapter(AllocationDescriptor &descriptor) const;

    Platform                                     &platform_;
    DxgkargGetStandardAllocationDriverDataRecord &request_;
};

}

#endif
