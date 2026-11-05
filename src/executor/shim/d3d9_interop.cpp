#include "executor/shim/d3d9_interop.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/vulkan_buffer.hpp"

namespace av {

D3d9Interop::D3d9Interop() : interop_(nullptr)
{
    AV_SCOPE("D3d9Interop::D3d9Interop");
}

D3d9Interop::~D3d9Interop()
{
    AV_SCOPE("D3d9Interop::~D3d9Interop");
    release();
}

bool D3d9Interop::connect(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9Interop::connect");
    if (interop_ != nullptr) return true;
    return device != nullptr &&
           SUCCEEDED(device->QueryInterface(__uuidof(ID3D9VkInteropDevice1), reinterpret_cast<void **>(&interop_)));
}

void D3d9Interop::release()
{
    AV_SCOPE("D3d9Interop::release");
    if (interop_ != nullptr) interop_->Release();
    interop_ = nullptr;
}

bool D3d9Interop::connected() const
{
    AV_SCOPE("D3d9Interop::connected");
    return interop_ != nullptr;
}

void D3d9Interop::handles(VkInstance &instance, VkPhysicalDevice &physical, VkDevice &device) const
{
    AV_SCOPE("D3d9Interop::handles");
    interop_->GetVulkanHandles(&instance, &physical, &device);
}

bool D3d9Interop::bindable(IDirect3DResource9 *buffer) const
{
    AV_SCOPE("D3d9Interop::bindable");
    return interop_ != nullptr && SUCCEEDED(interop_->BufferStorageBindable(buffer));
}

bool D3d9Interop::bind_storage(IDirect3DResource9 *buffer, const VulkanBuffer &storage, uint64_t offset,
                               uint64_t bytes, void *host) const
{
    AV_SCOPE("D3d9Interop::bind_storage");
    return interop_ != nullptr && SUCCEEDED(interop_->BindBufferStorage(buffer, storage.buffer(), offset, bytes, host));
}

bool D3d9Interop::unbind_storage(IDirect3DResource9 *buffer) const
{
    AV_SCOPE("D3d9Interop::unbind_storage");
    return interop_ != nullptr && SUCCEEDED(interop_->UnbindBufferStorage(buffer));
}

bool D3d9Interop::mark_submission(void (*done)(void *context, uint64_t value), void *context, uint64_t value) const
{
    AV_SCOPE("D3d9Interop::mark_submission");
    return interop_ != nullptr && SUCCEEDED(interop_->MarkSubmission(done, context, value));
}

void D3d9Interop::wait_for_idle() const
{
    AV_SCOPE("D3d9Interop::wait_for_idle");
    if (interop_ != nullptr) interop_->WaitForIdle();
}

}
