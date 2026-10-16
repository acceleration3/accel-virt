#include "executor/shim/d3d12_host_device.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/d3d_runtime.hpp"
#include "core/log/log_line.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/vulkan_device_table.hpp"

namespace av {

D3d12HostDevice::D3d12HostDevice()
    : runtime_(nullptr), uuid_(), tenant_heap_(), sink_(stderr), lines_(sink_, "virtio-gpu-av: d3d12: "),
      renewal_(tenant_heap_, lines_), shown_by_(nullptr),
      device_(nullptr), capabilities_(), frames_()
{
    AV_SCOPE("D3d12HostDevice::D3d12HostDevice");
}

D3d12HostDevice::~D3d12HostDevice()
{
    AV_SCOPE("D3d12HostDevice::~D3d12HostDevice");
    close();
}

bool D3d12HostDevice::open(const D3dRuntime &runtime, const uint8_t *uuid)
{
    AV_SCOPE("D3d12HostDevice::open");
    if (runtime_ != nullptr) return false;
    runtime_ = &runtime;
    uuid_ = DeviceUuid(uuid);
    if (!make()) {
        runtime_ = nullptr;
        return false;
    }
    capabilities_.measure(*device_);
    return true;
}

void D3d12HostDevice::close()
{
    AV_SCOPE("D3d12HostDevice::close");
    frames_.bind(nullptr);
    if (device_ != nullptr) device_->Release();
    device_ = nullptr;
    runtime_ = nullptr;
}

bool D3d12HostDevice::make()
{
    AV_SCOPE("D3d12HostDevice::make");
    void *made = nullptr;
    if (FAILED(runtime_->create_device12(uuid_.raw(), made)) || made == nullptr) return false;
    device_ = static_cast<ID3D12Device *>(made);
    frames_.bind(device_);
    return true;
}

bool D3d12HostDevice::placed() const
{
    AV_SCOPE("D3d12HostDevice::placed");
    return runtime_ != nullptr;
}

bool D3d12HostDevice::admit(WakeSignal &wake)
{
    AV_SCOPE("D3d12HostDevice::admit");
    return device_ != nullptr && renewal_.admit(wake);
}

void D3d12HostDevice::dismiss(WakeSignal &wake)
{
    AV_SCOPE("D3d12HostDevice::dismiss");
    renewal_.dismiss(wake);
}

bool D3d12HostDevice::lost()
{
    AV_SCOPE("D3d12HostDevice::lost");
    return renewal_.lost(*this);
}

bool D3d12HostDevice::present() const
{
    AV_SCOPE("D3d12HostDevice::present");
    return device_ != nullptr;
}

bool D3d12HostDevice::removed()
{
    AV_SCOPE("D3d12HostDevice::removed");
    return device_ != nullptr && FAILED(device_->GetDeviceRemovedReason());
}

void D3d12HostDevice::pair(D3d11HostDevice *shown_by)
{
    AV_SCOPE("D3d12HostDevice::pair");
    shown_by_ = shown_by;
}

bool D3d12HostDevice::renew(uint32_t wait_ms)
{
    AV_SCOPE("D3d12HostDevice::renew");
    return runtime_ != nullptr && renewal_.renew(*this, wait_ms);
}

/* The old device's objects outlive this where something still holds them:
 * vkd3d-proton keeps a device alive for its objects. */
bool D3d12HostDevice::remake()
{
    AV_SCOPE("D3d12HostDevice::remake");
    if (shown_by_ != nullptr) shown_by_->forget_direct3d12();
    ID3D12Device *const old = device_;
    device_ = nullptr;
    const bool made = make();
    if (old != nullptr) old->Release();
    return made;
}

bool D3d12HostDevice::lose()
{
    AV_SCOPE("D3d12HostDevice::lose");
    ID3D12DeviceExt *ext = nullptr;
    if (device_ == nullptr ||
        FAILED(device_->QueryInterface(__uuidof(ID3D12DeviceExt), reinterpret_cast<void **>(&ext))))
        return false;
    VkInstance        instance = VK_NULL_HANDLE;
    VkPhysicalDevice  physical = VK_NULL_HANDLE;
    VkDevice          vulkan = VK_NULL_HANDLE;
    const HRESULT     asked = ext->GetVulkanHandles(&instance, &physical, &vulkan);
    ext->Release();
    VulkanDeviceTable table;
    if (FAILED(asked) || !table.load(instance, physical, vulkan) || !table.lose()) return false;
    renewal_.declare("a test lost it on purpose");
    return true;
}

bool D3d12HostDevice::opened() const
{
    AV_SCOPE("D3d12HostDevice::opened");
    return device_ != nullptr;
}

ID3D12Device *D3d12HostDevice::device() const
{
    AV_SCOPE("D3d12HostDevice::device");
    return device_;
}

const D3d12Capabilities &D3d12HostDevice::capabilities() const
{
    AV_SCOPE("D3d12HostDevice::capabilities");
    return capabilities_;
}

ID3D12Heap *D3d12HostDevice::import_dmabuf_heap(void *address, int fd, uint64_t size) const
{
    AV_SCOPE("D3d12HostDevice::import_dmabuf_heap");
    if (device_ == nullptr || runtime_ == nullptr) return nullptr;
    void *heap = nullptr;
    if (FAILED(runtime_->open_dmabuf_heap(device_, address, fd, size, heap))) return nullptr;
    return static_cast<ID3D12Heap *>(heap);
}

bool D3d12HostDevice::export_image(ID3D12Resource *resource, Vkd3dImageExport &image) const
{
    AV_SCOPE("D3d12HostDevice::export_image");
    return runtime_ != nullptr && SUCCEEDED(runtime_->export_image(resource, image));
}

bool D3d12HostDevice::export_fence(ID3D12Fence *fence, OwnedFd &descriptor) const
{
    AV_SCOPE("D3d12HostDevice::export_fence");
    int        fd = -1;
    const bool exported = runtime_ != nullptr && SUCCEEDED(runtime_->export_fence(fence, fd));
    descriptor.hold(exported ? fd : -1);
    return exported;
}

ID3D12RootSignature *D3d12HostDevice::root_signature(const void *desc) const
{
    AV_SCOPE("D3d12HostDevice::root_signature");
    if (device_ == nullptr || runtime_ == nullptr) return nullptr;
    void *blob = nullptr;
    if (FAILED(runtime_->serialize_root_signature(desc, blob))) return nullptr;
    ID3DBlob *const serialized = static_cast<ID3DBlob *>(blob);
    ID3D12RootSignature *made = nullptr;
    if (FAILED(device_->CreateRootSignature(0u, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                            __uuidof(ID3D12RootSignature), reinterpret_cast<void **>(&made))))
        made = nullptr;
    serialized->Release();
    return made;
}

StagedFrame *D3d12HostDevice::stage_frame(ID3D12CommandQueue *queue, ID3D12Resource *source)
{
    AV_SCOPE("D3d12HostDevice::stage_frame");
    return frames_.stage(queue, source);
}

}  // namespace av
