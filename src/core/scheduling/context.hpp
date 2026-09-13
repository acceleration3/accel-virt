/* One execution context: the unit a device submits DMA buffers from, bound to
 * one node. Its address is the hContext dxgkrnl passes back. */
#ifndef AV_CONTEXT_HPP
#define AV_CONTEXT_HPP

#include "core/memory/root_page_table.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Device;
class GpuMmuGeometry;
struct DxgkargCreateContextRecord;
struct DxgkargSetRootPageTableRecord;

class Context {
public:
    /* The DMA buffers of every context, including the privileged one dxgkrnl
     * makes for paging, come from the adapter's one segment: an empty
     * DmaBufferSegmentSet leaves VidMm with nowhere to take them from. */
    static constexpr uint32_t dma_buffer_bytes = 0x10000u;
    static constexpr uint32_t allocation_list_entries = 256u;
    static constexpr uint32_t patch_location_entries = 1024u;

    Context(Platform &platform, Device &device, const GpuMmuGeometry &geometry, uint32_t node, uint32_t flags);
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    Device  &device() const;
    uint32_t node() const;
    /* DXGK_CREATECONTEXTFLAGS: SystemContext, GdiContext, VirtualAddressing. */
    uint32_t flags() const;
    /* The handle, and the DXGK_CONTEXTINFO VidSch sizes this context's buffers by. */
    void     report_to(DxgkargCreateContextRecord &request);
    [[nodiscard]] Status assign_root(const DxgkargSetRootPageTableRecord &root);
    /* Makes this the guest's half of an rvk context: it remembers which
     * allocations are its ring, fence and reply, so the binding can publish
     * them and set the host context up (feature 043). */
    void     become_rvk(const RvkContextRecord &record);
    bool     is_rvk() const;
    const RvkContextRecord &rvk_record() const;
    /* Whether a DMA buffer at this GPU virtual address can run on the context. */
    bool     admits(uint64_t dma_address) const;

private:
    Platform &platform_;
    Device   &device_;
    uint32_t  node_;
    uint32_t  flags_;
    RootPageTable root_;
    RvkContextRecord rvk_record_;
    bool      is_rvk_;
};

}  // namespace av

#endif  // AV_CONTEXT_HPP
