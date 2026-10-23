/* One DxgkDdiGetStandardAllocationDriverData request: the runtime creating a
 * primary, shadow, staging or GDI surface without a user-mode driver asks the
 * driver to describe it, and passes the description back to CreateAllocation.
 * A cross-adapter texture (two adapters' devices sharing row-major memory,
 * Affinity's) and an existing heap (D3D12's OpenExistingHeapFromAddress, a
 * D3DKMT_STANDARDALLOCATIONTYPE_EXISTINGHEAP allocation) both arrive as
 * D3DKMDT_GDISURFACETYPE_TEXTURE_CROSSADAPTER; the heap is as wide as its
 * bytes, one row high, of no format. */
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

    /* Sizes only when the runtime passes no buffer; otherwise the descriptor,
     * and the pitch the surface type asks the driver for. */
    [[nodiscard]] Status describe();

private:
    static constexpr uint32_t staging_format = 21u;   /* D3DDDIFMT_A8R8G8B8 */
    enum : int32_t { shared_primary = 1, shadow = 2, staging = 3, gdi = 4 };
    enum : int32_t { gdi_texture = 1, gdi_staging_cpu_visible = 2, gdi_staging = 3, gdi_texture_cpu_visible = 6,
                     gdi_texture_cross_adapter = 7 };

    [[nodiscard]] Status layout_of(uint32_t width, uint32_t height, uint32_t format, SurfaceLayout &layout) const;
    [[nodiscard]] Status describe_shared_primary(AllocationDescriptor &descriptor) const;
    [[nodiscard]] Status describe_shadow(AllocationDescriptor &descriptor) const;
    [[nodiscard]] Status describe_staging(AllocationDescriptor &descriptor) const;
    [[nodiscard]] Status describe_gdi(AllocationDescriptor &descriptor) const;
    /* The process's own pages, which the processor reads and writes: a
     * texture of any format the driver lays out, or a heap. */
    [[nodiscard]] Status describe_cross_adapter(AllocationDescriptor &descriptor) const;

    Platform                                     &platform_;
    DxgkargGetStandardAllocationDriverDataRecord &request_;
};

}  // namespace av

#endif  // AV_STANDARD_ALLOCATION_HPP
