#include "um/d3d/kernel_device.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/kernel_heap.hpp"
#include "icd/d3d/kernel_surface.hpp"

namespace av::d3d {

KernelDevice::KernelDevice() : callbacks_(nullptr), runtime_(nullptr)
{
    AV_SCOPE("KernelDevice::KernelDevice");
}

void KernelDevice::bind(const D3DDDI_DEVICECALLBACKS *callbacks, HANDLE runtime)
{
    AV_SCOPE("KernelDevice::bind");
    callbacks_ = callbacks;
    runtime_ = runtime;
}

bool KernelDevice::bound() const
{
    AV_SCOPE("KernelDevice::bound");
    return callbacks_ != nullptr;
}

HRESULT KernelDevice::make_context(HANDLE &context)
{
    AV_SCOPE("KernelDevice::make_context");
    D3DDDICB_CREATECONTEXTVIRTUAL create = {};
    create.NodeOrdinal = 0u;
    create.EngineAffinity = 1u;
    const HRESULT made = callbacks_->pfnCreateContextVirtualCb(runtime_, &create);
    if (SUCCEEDED(made)) context = create.hContext;
    return made;
}

HRESULT KernelDevice::acquire(HANDLE token, HANDLE context)
{
    AV_SCOPE("KernelDevice::acquire");
    if (callbacks_ == nullptr || callbacks_->pfnAcquireResourceCb == nullptr) return E_NOTIMPL;
    D3DDDICB_SYNCTOKEN acquire = {};
    acquire.hSyncToken = token;
    acquire.BroadcastContextCount = 1u;
    acquire.BroadcastContextArray = &context;
    return callbacks_->pfnAcquireResourceCb(runtime_, &acquire);
}

HRESULT KernelDevice::release(HANDLE token, HANDLE context)
{
    AV_SCOPE("KernelDevice::release");
    if (callbacks_ == nullptr || callbacks_->pfnReleaseResourceCb == nullptr) return E_NOTIMPL;
    D3DDDICB_SYNCTOKEN release = {};
    release.hSyncToken = token;
    release.BroadcastContextCount = 1u;
    release.BroadcastContextArray = &context;
    return callbacks_->pfnReleaseResourceCb(runtime_, &release);
}

void KernelDevice::destroy_context(HANDLE context)
{
    AV_SCOPE("KernelDevice::destroy_context");
    if (context == nullptr || callbacks_ == nullptr) return;
    D3DDDICB_DESTROYCONTEXT destroy = {};
    destroy.hContext = context;
    static_cast<void>(callbacks_->pfnDestroyContextCb(runtime_, &destroy));
}

HRESULT KernelDevice::allocate(KernelSurface &surface, HANDLE resource, D3DKMT_HANDLE &allocation)
{
    AV_SCOPE("KernelDevice::allocate");
    D3DDDI_ALLOCATIONINFO2 info = {};
    info.pPrivateDriverData = surface.allocation_data();
    info.PrivateDriverDataSize = KernelSurface::allocation_bytes;
    if (surface.primary()) {
        info.VidPnSourceId = surface.source();
        info.Flags.Primary = 1u;
    }
    D3DDDICB_ALLOCATE allocate = {};
    if (resource != nullptr) {
        allocate.pPrivateDriverData = surface.resource_data();
        allocate.PrivateDriverDataSize = KernelSurface::resource_bytes;
        allocate.hResource = resource;
    }
    allocate.NumAllocations = 1u;
    allocate.pAllocationInfo2 = &info;
    const HRESULT made = callbacks_->pfnAllocateCb(runtime_, &allocate);
    if (SUCCEEDED(made)) allocation = info.hAllocation;
    return made;
}

HRESULT KernelDevice::allocate(KernelHeap &heap, HANDLE resource, D3DKMT_HANDLE &allocation)
{
    AV_SCOPE("KernelDevice::allocate");
    D3DDDI_ALLOCATIONINFO2 info = {};
    info.pPrivateDriverData = heap.allocation_data();
    info.PrivateDriverDataSize = KernelHeap::allocation_bytes;
    D3DDDICB_ALLOCATE allocate = {};
    allocate.pPrivateDriverData = heap.resource_data();
    allocate.PrivateDriverDataSize = KernelHeap::resource_bytes;
    allocate.hResource = resource;
    allocate.NumAllocations = 1u;
    allocate.pAllocationInfo2 = &info;
    const HRESULT made = callbacks_->pfnAllocateCb(runtime_, &allocate);
    if (SUCCEEDED(made)) allocation = info.hAllocation;
    return made;
}

HRESULT KernelDevice::deallocate(D3DKMT_HANDLE allocation, HANDLE resource)
{
    AV_SCOPE("KernelDevice::deallocate");
    if (allocation == 0u || callbacks_ == nullptr) return S_OK;
    D3DDDICB_DEALLOCATE deallocate = {};
    deallocate.hResource = resource;
    if (resource == nullptr) {
        deallocate.NumAllocations = 1u;
        deallocate.HandleList = &allocation;
    }
    return callbacks_->pfnDeallocateCb(runtime_, &deallocate);
}

void *KernelDevice::lock(D3DDDICB_LOCK &asked)
{
    AV_SCOPE("KernelDevice::lock");
    if (FAILED(callbacks_->pfnLockCb(runtime_, &asked))) return nullptr;
    return asked.pData;
}

void *KernelDevice::lock_to_write(D3DKMT_HANDLE allocation)
{
    AV_SCOPE("KernelDevice::lock_to_write");
    D3DDDICB_LOCK asked = {};
    asked.hAllocation = allocation;
    asked.Flags.WriteOnly = 1u;
    return lock(asked);
}

void *KernelDevice::lock_to_read(D3DKMT_HANDLE allocation)
{
    AV_SCOPE("KernelDevice::lock_to_read");
    D3DDDICB_LOCK asked = {};
    asked.hAllocation = allocation;
    asked.Flags.ReadOnly = 1u;
    return lock(asked);
}

void KernelDevice::unlock(D3DKMT_HANDLE allocation)
{
    AV_SCOPE("KernelDevice::unlock");
    D3DDDICB_UNLOCK unlock = {};
    unlock.NumAllocations = 1u;
    unlock.phAllocations = &allocation;
    static_cast<void>(callbacks_->pfnUnlockCb(runtime_, &unlock));
}

const D3DDDI_DEVICECALLBACKS &KernelDevice::callbacks() const
{
    AV_SCOPE("KernelDevice::callbacks");
    return *callbacks_;
}

HANDLE KernelDevice::runtime() const
{
    AV_SCOPE("KernelDevice::runtime");
    return runtime_;
}

HRESULT KernelDevice::present(D3DDDICB_PRESENT &present)
{
    AV_SCOPE("KernelDevice::present");
    return callbacks_->pfnPresentCb(runtime_, &present);
}

HRESULT KernelDevice::set_display_mode(D3DDDICB_SETDISPLAYMODE &mode)
{
    AV_SCOPE("KernelDevice::set_display_mode");
    return callbacks_->pfnSetDisplayModeCb(runtime_, &mode);
}

}
