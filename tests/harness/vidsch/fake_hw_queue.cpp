#include "harness/vidsch/fake_hw_queue.hpp"

#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_submit_command_to_hw_queue_record.hpp"
#include "harness/framework/test_registry.hpp"

namespace av {

FakeHwQueue::FakeHwQueue(MockPlatform &platform, const DdiTable &table, void *adapter, void *context)
    : platform_(platform), table_(table), adapter_(adapter), context_(context), handle_(nullptr), progress_fence_(0u),
      submitted_(0u), completed_(0u) {}

FakeHwQueue::~FakeHwQueue()
{
    if (handle_ != nullptr) static_cast<void>(destroy());
}

std::int32_t FakeHwQueue::create(std::uint32_t flags)
{
    using CreateHwQueue = std::int32_t (*)(void *, DxgkargCreateHwQueueRecord *);
    DxgkargCreateHwQueueRecord request = {};
    request.hHwQueue = this;
    request.Flags = flags;
    request.hHwQueueProgressFence = fence_handle;
    request.HwQueueProgressFenceCPUVirtualAddress = &progress_fence_;
    request.HwQueueProgressFenceGPUVirtualAddress = fence_gpu_address;
    platform_.set_irql(Irql::passive);
    const std::int32_t status =
        reinterpret_cast<CreateHwQueue>(table_.routine(DdiSlot::DxgkDdiCreateHwQueue))(context_, &request);
    if (status != 0 && status != status_not_implemented && status != status_no_memory)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "CreateHwQueue: invalid NTSTATUS 0x%08X",
                                                static_cast<unsigned>(status));
    if (status >= 0) handle_ = request.hHwQueue;
    return status;
}

std::int32_t FakeHwQueue::submit(std::uint64_t dma_address, std::uint32_t flags)
{
    using SubmitCommandToHwQueue = std::int32_t (*)(void *, const DxgkargSubmitCommandToHwQueueRecord *);
    DxgkargSubmitCommandToHwQueueRecord request = {};
    request.hHwQueue = handle_;
    request.HwQueueProgressFenceId = submitted_ + 1u;
    request.DmaBufferVirtualAddress = dma_address;
    request.DmaBufferSize = dma_bytes;
    request.Flags = flags;
    request.HwQueueProgressFenceGpuVa = fence_gpu_address;
    request.HwQueueProgressFenceCpuVa = &progress_fence_;
    platform_.set_irql(Irql::dispatch);
    const std::int32_t status = reinterpret_cast<SubmitCommandToHwQueue>(
        table_.routine(DdiSlot::DxgkDdiSubmitCommandToHwQueue))(adapter_, &request);
    platform_.set_irql(Irql::passive);
    if (status != 0 && status != status_not_implemented && status != status_invalid_parameter)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "SubmitCommandToHwQueue: invalid NTSTATUS 0x%08X",
                                                static_cast<unsigned>(status));
    if (status >= 0) submitted_++;
    return status;
}

void FakeHwQueue::observe_progress()
{
    if (progress_fence_ < completed_ || progress_fence_ > submitted_)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "progress fence %llu outside %llu..%llu",
                                                static_cast<unsigned long long>(progress_fence_),
                                                static_cast<unsigned long long>(completed_),
                                                static_cast<unsigned long long>(submitted_));
    completed_ = progress_fence_;
}

std::int32_t FakeHwQueue::destroy()
{
    platform_.set_irql(Irql::passive);
    const std::int32_t status =
        reinterpret_cast<std::int32_t (*)(void *)>(table_.routine(DdiSlot::DxgkDdiDestroyHwQueue))(handle_);
    if (status != 0 && status != status_not_implemented)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "DestroyHwQueue: invalid NTSTATUS 0x%08X",
                                                static_cast<unsigned>(status));
    handle_ = nullptr;
    return status;
}

void *FakeHwQueue::handle() const { return handle_; }

std::uint64_t FakeHwQueue::submitted() const { return submitted_; }

std::uint64_t FakeHwQueue::completed() const { return completed_; }

}  // namespace av
