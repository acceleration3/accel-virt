/* One execution context: the unit a device submits DMA buffers from, bound to
 * one node. Its address is the hContext dxgkrnl passes back. */
#ifndef AV_CONTEXT_HPP
#define AV_CONTEXT_HPP

#include "platform.hpp"
#include "root_page_table.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class Device;
class GpuMmuGeometry;
struct DxgkargCreateContextRecord;
struct DxgkargSetRootPageTableRecord;

class Context {
public:
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
    /* Whether a DMA buffer at this GPU virtual address can run on the context. */
    bool     admits(uint64_t dma_address) const;

private:
    Platform &platform_;
    Device   &device_;
    uint32_t  node_;
    uint32_t  flags_;
    RootPageTable root_;
};

}  // namespace av

#endif  // AV_CONTEXT_HPP
