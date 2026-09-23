#include "harness/vidsch/fake_vidsch.hpp"

#include "core/scheduling/hw_queue_submissions.hpp"

#include "core/ddi/dxgkarg_present_record.hpp"

#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_patch_record.hpp"
#include "core/ddi/dxgkarg_preempt_command_record.hpp"
#include "core/ddi/dxgkarg_query_current_fence_record.hpp"
#include "core/ddi/dxgkarg_render_record.hpp"
#include "core/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "core/ddi/dxgkarg_submit_command_record.hpp"
#include "core/ddi/dxgkarg_submit_command_virtual_record.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::int32_t dma_completed = 1, dma_preempted = 2, monitored_fence_signaled = 11;

}  // namespace

FakeVidSch::FakeVidSch(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlAdapter &dxgkrnl, void *adapter)
    : platform_(platform), table_(table), dxgkrnl_(dxgkrnl), adapter_(adapter), device_(nullptr), context_(nullptr),
      dma_{}, rendered_(0u), allocation_{}, patch_in_{}, patch_out_{}, present_allocations_{}, present_patches_{}, present_virtual_{},
      submitted_(0u), completed_(0u),
      preemption_fence_(0u), preempted_through_(0u), preemptions_(0u), seen_(dxgkrnl.interrupts()), fence_signals_(0u) {}

FakeVidSch::~FakeVidSch()
{
    platform_.set_irql(Irql::passive);
    if (context_ != nullptr) reinterpret_cast<std::int32_t (*)(void *)>(table_.routine(DdiSlot::DxgkDdiDestroyContext))(context_);
    if (device_ != nullptr) reinterpret_cast<std::int32_t (*)(void *)>(table_.routine(DdiSlot::DxgkDdiDestroyDevice))(device_);
}

std::int32_t FakeVidSch::create_context(std::uint32_t flags)
{
    platform_.set_irql(Irql::passive);
    DxgkargCreateDeviceRecord device = {};
    std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, DxgkargCreateDeviceRecord *)>(
        table_.routine(DdiSlot::DxgkDdiCreateDevice))(adapter_, &device);
    if (status < 0) return status;
    device_ = device.hDevice;
    DxgkargCreateContextRecord context = {};
    context.Flags = flags;
    status = reinterpret_cast<std::int32_t (*)(void *, DxgkargCreateContextRecord *)>(
        table_.routine(DdiSlot::DxgkDdiCreateContext))(device_, &context);
    if (status >= 0) context_ = context.hContext;
    return status;
}

std::int32_t FakeVidSch::render(const std::uint8_t *command, std::uint32_t length, std::uint32_t patch_offset,
                                std::int64_t address)
{
    platform_.set_irql(Irql::passive);
    allocation_.PhysicalAddress = address;
    patch_in_ = {0u, 0u, 0u, 0u, patch_offset, 0u};
    DxgkargRenderRecord render = {};
    render.pCommand = command;
    render.CommandLength = length;
    render.pDmaBuffer = dma_;
    render.DmaSize = dma_bytes;
    render.pAllocationList = &allocation_;
    render.AllocationListSize = 1u;
    render.pPatchLocationListIn = &patch_in_;
    render.PatchLocationListInSize = 1u;
    render.pPatchLocationListOut = &patch_out_;
    render.PatchLocationListOutSize = 1u;
    std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, DxgkargRenderRecord *)>(
        table_.routine(DdiSlot::DxgkDdiRender))(context_, &render);
    if (status < 0) return status;
    rendered_ = length;
    DxgkargPatchRecord patch = {};
    patch.hDevice = device_;
    patch.pDmaBuffer = dma_;
    patch.DmaBufferSize = dma_bytes;
    patch.DmaBufferSubmissionEndOffset = rendered_;
    patch.pAllocationList = &allocation_;
    patch.AllocationListSize = 1u;
    patch.pPatchLocationList = &patch_out_;
    patch.PatchLocationListSize = 1u;
    patch.PatchLocationListSubmissionLength = 1u;
    patch.SubmissionFenceId = submitted_ + 1u;
    return reinterpret_cast<std::int32_t (*)(void *, const DxgkargPatchRecord *)>(table_.routine(DdiSlot::DxgkDdiPatch))(
        adapter_, &patch);
}

std::int32_t FakeVidSch::present_blt_virtual(std::uint64_t source, std::uint64_t destination,
                                             const RectRecord *sub_rects, std::uint32_t count)
{
    platform_.set_irql(Irql::passive);
    present_virtual_[1].AllocationVirtualAddress = source;
    present_virtual_[2].AllocationVirtualAddress = destination;
    DxgkargPresentRecord present = {};
    present.pDmaBuffer = dma_;
    present.DmaSize = dma_bytes;
    present.pAllocationList = present_virtual_;
    present.SrcRect = {0, 0, 1, 1};
    present.DstRect = {0, 0, 1, 1};
    present.SubRectCnt = count;
    present.pDstSubRects = sub_rects;
    present.Flags = 1u;   /* Blt */
    const std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, DxgkargPresentRecord *)>(
        table_.routine(DdiSlot::DxgkDdiPresent))(context_, &present);
    if (status < 0) return status;
    return submit_virtual(present_virtual_[2].AllocationVirtualAddress);
}

std::int32_t FakeVidSch::present_blt(std::int64_t source, std::int64_t destination, const RectRecord *sub_rects,
                                     std::uint32_t count)
{
    platform_.set_irql(Irql::passive);
    present_allocations_[1].PhysicalAddress = source;
    present_allocations_[2].PhysicalAddress = destination;
    DxgkargPresentRecord present = {};
    present.pDmaBuffer = dma_;
    present.DmaSize = dma_bytes;
    present.pAllocationList = present_allocations_;
    present.pPatchLocationListOut = present_patches_;
    present.PatchLocationListOutSize = 2u;
    present.SrcRect = {0, 0, 1, 1};
    present.DstRect = {0, 0, 1, 1};
    present.SubRectCnt = count;
    present.pDstSubRects = sub_rects;
    present.Flags = 1u;   /* Blt */
    const std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, DxgkargPresentRecord *)>(
        table_.routine(DdiSlot::DxgkDdiPresent))(context_, &present);
    if (status < 0) return status;
    rendered_ = static_cast<std::uint32_t>(static_cast<std::uint8_t *>(present.pDmaBuffer) - dma_);
    DxgkargPatchRecord patch = {};
    patch.hDevice = device_;
    patch.pDmaBuffer = dma_;
    patch.DmaBufferSize = dma_bytes;
    patch.DmaBufferSubmissionEndOffset = rendered_;
    patch.pAllocationList = present_allocations_;
    patch.AllocationListSize = 3u;
    patch.pPatchLocationList = present_patches_;
    patch.PatchLocationListSize = 2u;
    patch.PatchLocationListSubmissionLength = static_cast<std::uint32_t>(present.pPatchLocationListOut - present_patches_);
    patch.SubmissionFenceId = submitted_ + 1u;
    return reinterpret_cast<std::int32_t (*)(void *, const DxgkargPatchRecord *)>(table_.routine(DdiSlot::DxgkDdiPatch))(
        adapter_, &patch);
}

std::int32_t FakeVidSch::submit()
{
    DxgkargSubmitCommandRecord submit = {};
    submit.hDevice = device_;
    submit.DmaBufferSize = dma_bytes;
    submit.DmaBufferSubmissionEndOffset = rendered_;
    submit.SubmissionFenceId = submitted_ + 1u;
    platform_.set_irql(Irql::dispatch);
    const std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, const DxgkargSubmitCommandRecord *)>(
        table_.routine(DdiSlot::DxgkDdiSubmitCommand))(adapter_, &submit);
    platform_.set_irql(Irql::passive);
    if (status >= 0) submitted_++;
    return status;
}

void FakeVidSch::set_root_page_table(std::uint64_t segment_offset, std::uint32_t entries)
{
    platform_.set_irql(Irql::passive);
    const DxgkargSetRootPageTableRecord root = {context_, root_segment_id, 0u, segment_offset, entries, 0u};
    reinterpret_cast<void (*)(void *, const DxgkargSetRootPageTableRecord *)>(
        table_.routine(DdiSlot::DxgkDdiSetRootPageTable))(adapter_, &root);
}

std::int32_t FakeVidSch::submit_virtual(std::uint64_t dma_address)
{
    platform_.set_irql(Irql::passive);
    DxgkargSubmitCommandVirtualRecord submit = {};
    submit.hContext = context_;
    submit.DmaBufferVirtualAddress = dma_address;
    submit.DmaBufferSize = dma_bytes;
    submit.SubmissionFenceId = submitted_ + 1u;
    const std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, const DxgkargSubmitCommandVirtualRecord *)>(
        table_.routine(DdiSlot::DxgkDdiSubmitCommandVirtual))(adapter_, &submit);
    if (status >= 0) submitted_++;
    return status;
}

std::int32_t FakeVidSch::preempt()
{
    /* VidSch draws the preemption fence from the node's own sequence, after
     * every packet it submitted. */
    DxgkargPreemptCommandRecord preempt = {};
    preemption_fence_ = ++submitted_;
    preempt.PreemptionFenceId = preemption_fence_;
    platform_.set_irql(Irql::dispatch);
    const std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, const DxgkargPreemptCommandRecord *)>(
        table_.routine(DdiSlot::DxgkDdiPreemptCommand))(adapter_, &preempt);
    platform_.set_irql(Irql::passive);
    return status;
}

void FakeVidSch::check_notification(const DxgkargcbNotifyInterruptDataRecord &data)
{
    if (data.InterruptType == dma_completed) {
        if (data.FenceId <= completed_ || data.FenceId <= preempted_through_ || data.FenceId > submitted_)
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "completion of fence %u out of order (%u..%u)",
                                                    data.FenceId, completed_, submitted_);
        completed_ = data.FenceId;
        return;
    }
    if (data.InterruptType == dma_preempted) {
        if (data.FenceId != preemption_fence_ || data.Argument1 != completed_)
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "preemption %u after %u, expected %u after %u",
                                                    data.FenceId, data.Argument1, preemption_fence_, completed_);
        /* VidSchiProcessIsrPreemptedPacket (dxgmms2.sys @1400357a4) retires
         * every packet still queued as preempted, to be resubmitted under new
         * fences; VidSchiProcessIsrCompletedPacket bugchecks 0x119/1 when one
         * of them is reported completed afterwards. */
        preempted_through_ = preemption_fence_;
        preemptions_++;
        return;
    }
    if (data.InterruptType == monitored_fence_signaled) {
        /* MonitoredFenceSignaled's NodeOrdinal and EngineOrdinal overlay FenceId
         * and Argument1; the queues run on the hardware-scheduled node
         * (feature 049, research.md §16). */
        if (data.FenceId != av::HwQueueSubmissions::node || data.Argument1 != 0u)
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "fence signalled for node %u engine %u",
                                                    data.FenceId, data.Argument1);
        fence_signals_++;
        return;
    }
    TestRegistry::instance().record_failure(__FILE__, __LINE__, "unexpected interrupt type %d", data.InterruptType);
}

void FakeVidSch::run_dpc()
{
    platform_.set_irql(Irql::dispatch);
    reinterpret_cast<void (*)(void *)>(table_.routine(DdiSlot::DxgkDdiDpcRoutine))(adapter_);
    platform_.set_irql(Irql::passive);
    for (; seen_ < dxgkrnl_.interrupts(); seen_++) check_notification(dxgkrnl_.interrupt(seen_));
}

std::int32_t FakeVidSch::reset_from_timeout()
{
    platform_.set_irql(Irql::passive);
    const std::int32_t status =
        reinterpret_cast<std::int32_t (*)(void *)>(table_.routine(DdiSlot::DxgkDdiResetFromTimeout))(adapter_);
    if (status >= 0) completed_ = submitted_;
    return status;
}

std::int32_t FakeVidSch::restart_from_timeout()
{
    platform_.set_irql(Irql::passive);
    return reinterpret_cast<std::int32_t (*)(void *)>(table_.routine(DdiSlot::DxgkDdiRestartFromTimeout))(adapter_);
}

std::int32_t FakeVidSch::query_current_fence(std::uint32_t &fence)
{
    platform_.set_irql(Irql::passive);
    DxgkargQueryCurrentFenceRecord query = {};
    const std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, DxgkargQueryCurrentFenceRecord *)>(
        table_.routine(DdiSlot::DxgkDdiQueryCurrentFence))(adapter_, &query);
    fence = query.CurrentFence;
    return status;
}

void *FakeVidSch::context() const { return context_; }

std::uint32_t FakeVidSch::fence_signals() const { return fence_signals_; }

const std::uint8_t *FakeVidSch::dma() const { return dma_; }
std::uint32_t       FakeVidSch::submitted() const { return submitted_; }
std::uint32_t       FakeVidSch::completed() const { return completed_; }
std::uint32_t       FakeVidSch::preemptions() const { return preemptions_; }

}  // namespace av
