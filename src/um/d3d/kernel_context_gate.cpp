#include "um/d3d/kernel_context_gate.hpp"

#include "core/log/keyed_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"

namespace av::d3d {

namespace {

constexpr uint32_t stall_report_every = 100u;

}

KernelContextGate::KernelContextGate(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, HANDLE context,
                                     KeyedLog &lines, DWORD bound_ms)
    : kernel_(kernel), device_(device), context_(context), lines_(lines), bound_ms_(bound_ms), stalled_(0u),
      fence_(0u), reached_(nullptr),
      signalled_(0u), event_(nullptr), refused_()
{
    AV_SCOPE("KernelContextGate::KernelContextGate");
}

KernelContextGate::~KernelContextGate()
{
    AV_SCOPE("KernelContextGate::~KernelContextGate");
    if (fence_ != 0u && kernel_.pfnDestroySynchronizationObjectCb != nullptr) {
        D3DDDICB_DESTROYSYNCHRONIZATIONOBJECT destroy = {};
        destroy.hSyncObject = fence_;
        static_cast<void>(kernel_.pfnDestroySynchronizationObjectCb(device_, &destroy));
    }
    if (event_ != nullptr) CloseHandle(event_);
}

void KernelContextGate::pass()
{
    AV_SCOPE("KernelContextGate::pass");
    const HRESULT waited = signal_and_wait();
    if (waited == S_OK) return;
    if (waited == S_FALSE) {
        stalled_++;
        if (stalled_ == 1u || stalled_ % stall_report_every == 0u)
            LogLine(lines_)
                .text("a batch went to the host before its kernel context finished, after ")
                .number(bound_ms_)
                .text(" ms; ")
                .number(stalled_)
                .text(" so far")
                .write();
        return;
    }
    if (!refused_.first()) return;
    LogLine(lines_)
        .text("a batch went to the host without waiting for its kernel context ")
        .hex(static_cast<uint32_t>(waited), 8u)
        .write();
}

uint32_t KernelContextGate::stalled() const
{
    AV_SCOPE("KernelContextGate::stalled");
    return stalled_;
}

HRESULT KernelContextGate::signal_and_wait()
{
    AV_SCOPE("KernelContextGate::signal_and_wait");
    if (context_ == nullptr || kernel_.pfnSignalSynchronizationObjectFromGpuCb == nullptr ||
        kernel_.pfnWaitForSynchronizationObjectFromCpuCb == nullptr)
        return E_NOTIMPL;
    const HRESULT made = make_fence();
    if (FAILED(made)) return made;
    const uint64_t                              next = signalled_ + 1u;
    D3DDDICB_SIGNALSYNCHRONIZATIONOBJECTFROMGPU signal = {};
    signal.hContext = context_;
    signal.ObjectCount = 1u;
    signal.ObjectHandleArray = &fence_;
    signal.MonitoredFenceValueArray = &next;
    const HRESULT signalling = kernel_.pfnSignalSynchronizationObjectFromGpuCb(device_, &signal);
    if (FAILED(signalling)) return signalling;
    signalled_ = next;
    return await_signalled();
}

HRESULT KernelContextGate::make_fence()
{
    AV_SCOPE("KernelContextGate::make_fence");
    if (fence_ != 0u) return S_OK;
    if (kernel_.pfnCreateSynchronizationObject2Cb == nullptr) return E_NOTIMPL;
    if (event_ == nullptr) event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (event_ == nullptr) return E_OUTOFMEMORY;
    D3DDDICB_CREATESYNCHRONIZATIONOBJECT2 create = {};
    create.Info.Type = D3DDDI_MONITORED_FENCE;
    create.Info.MonitoredFence.InitialFenceValue = 0u;
    const HRESULT made = kernel_.pfnCreateSynchronizationObject2Cb(device_, &create);
    if (FAILED(made)) return made;
    fence_ = create.hSyncObject;
    reached_ = static_cast<const volatile uint64_t *>(create.Info.MonitoredFence.FenceValueCPUVirtualAddress);
    return S_OK;
}

HRESULT KernelContextGate::await_signalled()
{
    AV_SCOPE("KernelContextGate::await_signalled");
    if (reached_ != nullptr && *reached_ >= signalled_) return S_OK;
    D3DDDICB_WAITFORSYNCHRONIZATIONOBJECTFROMCPU wait = {};
    wait.ObjectCount = 1u;
    wait.ObjectHandleArray = &fence_;
    wait.FenceValueArray = &signalled_;
    wait.hAsyncEvent = event_;
    const HRESULT asked = kernel_.pfnWaitForSynchronizationObjectFromCpuCb(device_, &wait);
    if (FAILED(asked)) return asked;
    for (;;) {
        if (WaitForSingleObject(event_, bound_ms_) != WAIT_OBJECT_0) return S_FALSE;
        if (reached_ == nullptr || *reached_ >= signalled_) return S_OK;
    }
}

}
