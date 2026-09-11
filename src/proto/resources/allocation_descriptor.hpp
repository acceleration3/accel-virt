/* The private driver data of one allocation (docs/protocol.md): written by
 * DxgkDdiGetStandardAllocationDriverData for the runtime's standard allocations
 * and by a user-mode driver for its own, read back by DxgkDdiCreateAllocation.
 * The bytes come from user mode, so decoding validates every field. */
#ifndef AV_ALLOCATION_DESCRIPTOR_HPP
#define AV_ALLOCATION_DESCRIPTOR_HPP

#include "core/support/status.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/surface_layout.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class AllocationDescriptor {
public:
    static constexpr size_t encoded_size = 48u;

    AllocationDescriptor();
    AllocationDescriptor(AllocationKind kind, bool cpu_visible, const SurfaceLayout &layout, uint64_t size,
                         const ScanoutTarget &scanout);

    AllocationKind       kind() const;
    bool                 cpu_visible() const;
    const SurfaceLayout &layout() const;
    uint64_t             size() const;
    const ScanoutTarget &scanout() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x414B5652u;   /* "RVKA" */
    static constexpr uint16_t flag_cpu_visible = 0x1u;

    bool is_valid() const;

    AllocationKind kind_;
    bool           cpu_visible_;
    SurfaceLayout  layout_;
    uint64_t       size_;
    ScanoutTarget  scanout_;
};

}  // namespace av

#endif  // AV_ALLOCATION_DESCRIPTOR_HPP
