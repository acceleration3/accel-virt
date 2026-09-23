/* VidSch's side of one node for a driver that is not MultiEngineAware: a
 * device and a context, then Render at PASSIVE, Patch at PASSIVE, SubmitCommand
 * at DISPATCH with monotonically increasing fences, and the DPC at DISPATCH.
 * Every notification the driver raises is checked against what was submitted:
 * completions in increasing order, never past the last submission, and a
 * preemption naming the last completed fence, after which nothing it
 * preempted may complete. A breach fails the test. */
#ifndef AV_FAKE_VIDSCH_HPP
#define AV_FAKE_VIDSCH_HPP

#include "core/ddi/d3dddi_patch_location_list_record.hpp"
#include "core/ddi/dxgk_allocation_list_record.hpp"
#include "core/ddi/dxgk_present_allocation_info_record.hpp"
#include "core/ddi/rect_record.hpp"
#include "core/miniport/ddi_table.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class FakeVidSch {
public:
    static constexpr std::uint32_t dma_bytes = 0x10000u;
    /* The implicit system memory segment VidMm adds for a GPU MMU adapter. */
    static constexpr std::uint32_t root_segment_id = 2u;

    FakeVidSch(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlAdapter &dxgkrnl, void *adapter);
    ~FakeVidSch();
    FakeVidSch(const FakeVidSch &) = delete;
    FakeVidSch &operator=(const FakeVidSch &) = delete;

    /* CreateDevice, then CreateContext with DXGK_CREATECONTEXTFLAGS. */
    std::int32_t create_context(std::uint32_t flags);
    /* Render then Patch one command buffer whose patch location `patch_offset`
     * references an allocation at `address`; answers the first failure. */
    std::int32_t render(const std::uint8_t *command, std::uint32_t length, std::uint32_t patch_offset,
                        std::int64_t address);
    /* Present a Blt from the allocation at `source` to the one at `destination`,
     * then Patch it, as dxgkrnl's present path does; answers the first failure. */
    std::int32_t present_blt(std::int64_t source, std::int64_t destination, const RectRecord *sub_rects,
                             std::uint32_t count);
    std::int32_t submit();
    /* DXGCONTEXT::SubmitPresentWithDmaBuffer @ 14031b804 for a context with GPU
     * virtual addressing: no patch list, the allocations by GPU virtual address,
     * then SubmitCommandVirtual. */
    std::int32_t present_blt_virtual(std::uint64_t source, std::uint64_t destination, const RectRecord *sub_rects,
                                     std::uint32_t count);
    /* VidSchiEnsureRootPageTableUpdated @ dxgmms2 1400fcb40: the context's root,
     * before a virtual submission. */
    void         set_root_page_table(std::uint64_t segment_offset, std::uint32_t entries);
    /* SubmitCommandVirtual at PASSIVE_LEVEL, as its annotation requires. */
    std::int32_t submit_virtual(std::uint64_t dma_address);
    std::int32_t preempt();
    void         run_dpc();
    /* TdrResetFromTimeout: VidSch aborts every packet it had submitted. */
    std::int32_t reset_from_timeout();
    std::int32_t restart_from_timeout();
    std::int32_t query_current_fence(std::uint32_t &fence);

    /* The driver's handle for the context create_context made. */
    void               *context() const;
    /* MONITORED_FENCE_SIGNALED reports seen, each for node 0 engine 0. */
    std::uint32_t       fence_signals() const;
    const std::uint8_t *dma() const;
    std::uint32_t       submitted() const;
    std::uint32_t       completed() const;
    std::uint32_t       preemptions() const;

private:
    void check_notification(const DxgkargcbNotifyInterruptDataRecord &data);

    MockPlatform                  &platform_;
    const DdiTable                &table_;
    FakeDxgkrnlAdapter            &dxgkrnl_;
    void                          *adapter_;
    void                          *device_;
    void                          *context_;
    std::uint8_t                   dma_[dma_bytes];
    std::uint32_t                  rendered_;
    DxgkAllocationListRecord       allocation_;
    D3dddiPatchLocationListRecord  patch_in_;
    D3dddiPatchLocationListRecord  patch_out_;
    DxgkAllocationListRecord       present_allocations_[3];
    D3dddiPatchLocationListRecord  present_patches_[2];
    DxgkPresentAllocationInfoRecord present_virtual_[3];
    std::uint32_t                  submitted_;
    std::uint32_t                  completed_;
    std::uint32_t                  preemption_fence_;
    std::uint32_t                  preempted_through_;
    std::uint32_t                  preemptions_;
    std::uint32_t                  seen_;
    std::uint32_t                  fence_signals_;
};

}  // namespace av

#endif  // AV_FAKE_VIDSCH_HPP
