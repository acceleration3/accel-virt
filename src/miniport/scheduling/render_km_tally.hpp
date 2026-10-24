/* What CDD's kernel-mode command buffers ask of this driver: DxgkDdiRenderKm
 * walks each buffer's GDI commands, counts them by operation, and runs none -
 * the DMA buffer it answers holds a no-op, so the packet completes its fence
 * like any other. The counts are how an experiment learns which operations GDI
 * sends once the adapter reports SupportKernelModeCommandBuffer
 * (docs/stale-blt-frames.md). */
#ifndef AV_RENDER_KM_TALLY_HPP
#define AV_RENDER_KM_TALLY_HPP

#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DeviceTable;
class StateWriter;
struct DxgkRenderkmCommandRecord;
struct DxgkargRenderRecord;
struct RenderKmTallyProbe;

class RenderKmTally {
public:
    RenderKmTally(Platform &platform, DeviceTable &devices);
    RenderKmTally(const RenderKmTally &) = delete;
    RenderKmTally &operator=(const RenderKmTally &) = delete;

    /* Counts the commands in request.pCommand, then advances the DMA buffer
     * past the no-op it wrote. Nothing is added to the patch location list:
     * no command is run, so no DMA buffer refers to an allocation. */
    [[nodiscard]] NtStatus render(DxgkargRenderRecord &request);

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    /* Four zero bytes: nothing reads a DMA buffer of a context that runs no
     * commands, but dxgkrnl is spared an empty one. */
    static constexpr uint32_t no_op_bytes = 4u;
    static constexpr uint32_t insufficient_dma_buffer = 0xC01E0001u;   /* STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER */

    [[nodiscard]] Status walk(const DxgkargRenderRecord &request);
    void                 count(const DxgkRenderkmCommandRecord &command, uint32_t available,
                               const DxgkargRenderRecord &request);
    /* The AllocationKind of the allocation list's entry `index`, 0 for one
     * that names nothing this driver opened. */
    uint32_t             kind_at(const DxgkargRenderRecord &request, uint32_t index);

    Platform    &platform_;
    DeviceTable &devices_;
    uint64_t     calls_;
    uint64_t     bitblts_;
    uint64_t     color_fills_;
    uint64_t     alpha_blends_;
    uint64_t     stretch_blts_;
    uint64_t     transparent_blts_;
    uint64_t     cleartype_blends_;
    uint64_t     escapes_;
    /* An operation this driver does not know: counted so it is not mistaken
     * for a buffer that held nothing. */
    uint64_t     unknown_;
    uint32_t     last_bitblt_source_kind_;
    uint32_t     last_bitblt_destination_kind_;

    friend struct RenderKmTallyProbe;
};

}  // namespace av

#endif  // AV_RENDER_KM_TALLY_HPP
