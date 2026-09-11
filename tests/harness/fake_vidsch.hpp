/* VidSch's side of one node for a driver that is not MultiEngineAware: a
 * device and a context, then Render at PASSIVE, Patch at PASSIVE, SubmitCommand
 * at DISPATCH with monotonically increasing fences, and the DPC at DISPATCH.
 * Every notification the driver raises is checked against what was submitted:
 * completions in increasing order, never past the last submission, and a
 * preemption naming the last completed fence. A breach fails the test. */
#ifndef AV_FAKE_VIDSCH_HPP
#define AV_FAKE_VIDSCH_HPP

#include "../../src/core/ddi/d3dddi_patch_location_list_record.hpp"
#include "../../src/core/ddi/dxgk_allocation_list_record.hpp"
#include "../../src/core/ddi_table.hpp"
#include "fake_dxgkrnl_adapter.hpp"
#include "mock_platform.hpp"

#include <cstdint>

namespace av {

class FakeVidSch {
public:
    static constexpr std::uint32_t dma_bytes = 0x10000u;

    FakeVidSch(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlAdapter &dxgkrnl, void *adapter);
    ~FakeVidSch();
    FakeVidSch(const FakeVidSch &) = delete;
    FakeVidSch &operator=(const FakeVidSch &) = delete;

    std::int32_t create_context();
    /* Render then Patch one command buffer whose patch location `patch_offset`
     * references an allocation at `address`; answers the first failure. */
    std::int32_t render(const std::uint8_t *command, std::uint32_t length, std::uint32_t patch_offset,
                        std::int64_t address);
    std::int32_t submit();
    std::int32_t preempt();
    void         run_dpc();
    std::int32_t query_current_fence(std::uint32_t &fence);

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
    std::uint32_t                  submitted_;
    std::uint32_t                  completed_;
    std::uint32_t                  preemption_fence_;
    std::uint32_t                  preemptions_;
    std::uint32_t                  seen_;
};

}  // namespace av

#endif  // AV_FAKE_VIDSCH_HPP
