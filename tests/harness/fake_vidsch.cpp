#include "fake_vidsch.hpp"

#include "../../src/core/ddi/dxgkarg_create_context_record.hpp"
#include "../../src/core/ddi/dxgkarg_create_device_record.hpp"
#include "../../src/core/ddi/dxgkarg_patch_record.hpp"
#include "../../src/core/ddi/dxgkarg_preempt_command_record.hpp"
#include "../../src/core/ddi/dxgkarg_query_current_fence_record.hpp"
#include "../../src/core/ddi/dxgkarg_render_record.hpp"
#include "../../src/core/ddi/dxgkarg_submit_command_record.hpp"
#include "test_registry.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::int32_t dma_completed = 1, dma_preempted = 2;

}  // namespace

FakeVidSch::FakeVidSch(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlAdapter &dxgkrnl, void *adapter)
    : platform_(platform), table_(table), dxgkrnl_(dxgkrnl), adapter_(adapter), device_(nullptr), context_(nullptr),
      dma_{}, rendered_(0u), allocation_{}, patch_in_{}, patch_out_{}, submitted_(0u), completed_(0u),
      preemption_fence_(0x80000000u), preemptions_(0u), seen_(dxgkrnl.interrupts()) {}

FakeVidSch::~FakeVidSch()
{
    platform_.set_irql(Irql::passive);
    if (context_ != nullptr) reinterpret_cast<std::int32_t (*)(void *)>(table_.routine(DdiSlot::DxgkDdiDestroyContext))(context_);
    if (device_ != nullptr) reinterpret_cast<std::int32_t (*)(void *)>(table_.routine(DdiSlot::DxgkDdiDestroyDevice))(device_);
}

std::int32_t FakeVidSch::create_context()
{
    platform_.set_irql(Irql::passive);
    DxgkargCreateDeviceRecord device = {};
    std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, DxgkargCreateDeviceRecord *)>(
        table_.routine(DdiSlot::DxgkDdiCreateDevice))(adapter_, &device);
    if (status < 0) return status;
    device_ = device.hDevice;
    DxgkargCreateContextRecord context = {};
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

std::int32_t FakeVidSch::preempt()
{
    DxgkargPreemptCommandRecord preempt = {};
    preempt.PreemptionFenceId = ++preemption_fence_;
    platform_.set_irql(Irql::dispatch);
    const std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, const DxgkargPreemptCommandRecord *)>(
        table_.routine(DdiSlot::DxgkDdiPreemptCommand))(adapter_, &preempt);
    platform_.set_irql(Irql::passive);
    return status;
}

void FakeVidSch::check_notification(const DxgkargcbNotifyInterruptDataRecord &data)
{
    if (data.InterruptType == dma_completed) {
        if (data.FenceId <= completed_ || data.FenceId > submitted_)
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "completion of fence %u out of order (%u..%u)",
                                                    data.FenceId, completed_, submitted_);
        completed_ = data.FenceId;
        return;
    }
    if (data.InterruptType == dma_preempted) {
        if (data.FenceId != preemption_fence_ || data.Argument1 != completed_)
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "preemption %u after %u, expected %u after %u",
                                                    data.FenceId, data.Argument1, preemption_fence_, completed_);
        preemptions_++;
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

const std::uint8_t *FakeVidSch::dma() const { return dma_; }
std::uint32_t       FakeVidSch::submitted() const { return submitted_; }
std::uint32_t       FakeVidSch::completed() const { return completed_; }
std::uint32_t       FakeVidSch::preemptions() const { return preemptions_; }

}  // namespace av
