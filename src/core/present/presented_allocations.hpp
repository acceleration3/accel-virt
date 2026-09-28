/* The allocations a present names, in the two forms dxgkrnl passes them.
 *
 * Counted: NumSrcAllocations sources, then NumDstAllocations destinations -
 * dxgkrnl's own presents, one of each (a colour fill has no source).
 *
 * Fixed: no counts, and the allocations at DXGK_PRESENT_SOURCE_INDEX and
 * DXGK_PRESENT_DESTINATION_INDEX, the indices the present's DMA buffer refers
 * to - an application's blt-model present on a context with GPU virtual
 * addressing, which dxgkrnl builds into the window's redirection surface
 * (RedirectedBlt). Only such a context passes it: one addressed through patch
 * locations always counts.
 *
 * The list is DXGK_ALLOCATIONLIST on a patched context and
 * DXGK_PRESENTALLOCATIONINFO on one with GPU virtual addressing: two strides. */
#ifndef AV_PRESENTED_ALLOCATIONS_HPP
#define AV_PRESENTED_ALLOCATIONS_HPP

#include <stdint.h>

namespace av {

struct DxgkargPresentRecord;

class PresentedAllocations {
public:
    explicit PresentedAllocations(const DxgkargPresentRecord &request);
    PresentedAllocations(const PresentedAllocations &) = delete;
    PresentedAllocations &operator=(const PresentedAllocations &) = delete;

    /* The device-specific handle a blt reads from; null when the present names
     * no single source. */
    void *source() const;
    /* The handle a blt or a colour fill writes; null when it names no single
     * destination. */
    void *destination() const;

private:
    enum : uint32_t { source_index = 1u, destination_index = 2u };   /* DXGK_PRESENT_*_INDEX */

    bool  fixed() const;
    void *at(uint32_t index) const;

    const DxgkargPresentRecord &request_;
};

}  // namespace av

#endif  // AV_PRESENTED_ALLOCATIONS_HPP
