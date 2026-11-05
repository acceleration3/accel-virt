#include "um/d3d/allocation_lender.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d {

AllocationLender::AllocationLender(D3dSession &session) : session_(session), callbacks_(nullptr), runtime_(nullptr)
{
    AV_SCOPE("AllocationLender::AllocationLender");
}

void AllocationLender::bind(const D3DDDI_DEVICECALLBACKS *callbacks, HANDLE runtime)
{
    AV_SCOPE("AllocationLender::bind");
    callbacks_ = callbacks;
    runtime_ = runtime;
}

Status AllocationLender::lend(D3DKMT_HANDLE allocation, uint64_t bytes, HostAllocation *&made) const
{
    AV_SCOPE("AllocationLender::lend");
    made = nullptr;
    if (callbacks_ == nullptr || callbacks_->pfnLockCb == nullptr) return Status::no_memory;
    D3DDDICB_LOCK lock = {};
    lock.hAllocation = allocation;
    if (FAILED(callbacks_->pfnLockCb(runtime_, &lock)) || lock.pData == nullptr) return Status::no_memory;
    const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);
    const Status            lent = session_.adopt_memory(lock.pData, bytes, readback, made);
    D3DDDICB_UNLOCK unlock = {};
    unlock.NumAllocations = 1u;
    unlock.phAllocations = &allocation;
    if (callbacks_->pfnUnlockCb != nullptr) static_cast<void>(callbacks_->pfnUnlockCb(runtime_, &unlock));
    return lent;
}

void AllocationLender::close(HostAllocation &made) const
{
    AV_SCOPE("AllocationLender::close");
    session_.close_memory(made);
}

void AllocationLender::retire(HostAllocation &made) const
{
    AV_SCOPE("AllocationLender::retire");
    session_.shared_memory().retire_memory(made);
}

}
