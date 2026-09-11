/* One allocation VidMm manages: the descriptor its creator wrote, its size
 * rounded to a page, and how many devices have it open. Its address is the
 * hAllocation dxgkrnl passes back. Reached under the allocation table's mutex,
 * so every method is legal up to APC. */
#ifndef AV_ALLOCATION_HPP
#define AV_ALLOCATION_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/resources/allocation_descriptor.hpp"

#include <stdint.h>

namespace av {

struct AllocationProbe;
struct DxgkAllocationInfoRecord;

class Allocation {
public:
    static constexpr uint64_t page_bytes = 0x1000u;

    /* `size` is the descriptor's size rounded up to a page. */
    Allocation(Platform &platform, const AllocationDescriptor &descriptor, uint64_t size);
    Allocation(const Allocation &) = delete;
    Allocation &operator=(const Allocation &) = delete;

    const AllocationDescriptor &descriptor() const;
    uint64_t                    size() const;
    uint32_t                    opens() const;

    /* The DXGK_ALLOCATIONINFO fields the driver answers. */
    void report_to(DxgkAllocationInfoRecord &info);
    [[nodiscard]] Status open();
    [[nodiscard]] Status close();

    bool invariants_hold() const;

private:
    static constexpr uint32_t flag_cpu_visible = 0x1u;            /* DXGK_ALLOCATIONINFOFLAGS */
    static constexpr uint32_t priority_normal = 0x28000000u;      /* D3DDDI_ALLOCATIONPRIORITY_NORMAL */

    Platform            &platform_;
    AllocationDescriptor descriptor_;
    uint64_t             size_;
    uint32_t             opens_;

    friend struct AllocationProbe;
};

}  // namespace av

#endif  // AV_ALLOCATION_HPP
