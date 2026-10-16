#include "executor/shim/d3d12_host_device.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/d3d_runtime.hpp"
#include "core/log/log_line.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/vulkan_device_table.hpp"

namespace av {

namespace {

constexpr uint32_t level_12_2 = 0xc200u;
/* The highest shader model asked about; the answer is the device's highest
 * at or below it. */
constexpr uint32_t shader_model_6_6 = 0x66u;

}  // namespace

D3d12HostDevice::D3d12HostDevice()
    : runtime_(nullptr), uuid_(), tenant_heap_(), sink_(stderr), lines_(sink_, "virtio-gpu-av: d3d12: "),
      renewal_(tenant_heap_, lines_), shown_by_(nullptr),
      device_(nullptr), caps_{}, frames_()
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
    measure();
    return true;
}

void D3d12HostDevice::close()
{
    AV_SCOPE("D3d12HostDevice::close");
    frames_.bind(nullptr);
    if (device_ != nullptr) device_->Release();
    device_ = nullptr;
    runtime_ = nullptr;
    caps_ = d3d12proto::Caps12{};
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

const d3d12proto::Caps12 &D3d12HostDevice::caps() const
{
    AV_SCOPE("D3d12HostDevice::caps");
    return caps_;
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

bool D3d12HostDevice::export_fence(ID3D12Fence *fence, int &fd) const
{
    AV_SCOPE("D3d12HostDevice::export_fence");
    fd = -1;
    return runtime_ != nullptr && SUCCEEDED(runtime_->export_fence(fence, fd));
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

void D3d12HostDevice::measure()
{
    AV_SCOPE("D3d12HostDevice::measure");
    /* 12_2 is 0xc200, newer than the native headers' enum. */
    const D3D_FEATURE_LEVEL asked[] = {static_cast<D3D_FEATURE_LEVEL>(level_12_2), D3D_FEATURE_LEVEL_12_1,
                                       D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {};
    levels.NumFeatureLevels = static_cast<UINT>(sizeof(asked) / sizeof(asked[0]));
    levels.pFeatureLevelsRequested = asked;
    if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels))))
        caps_.feature_level = static_cast<uint32_t>(levels.MaxSupportedFeatureLevel);
    D3D12_FEATURE_DATA_SHADER_MODEL model = {};
    model.HighestShaderModel = static_cast<D3D_SHADER_MODEL>(shader_model_6_6);
    if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &model, sizeof(model))))
        caps_.shader_model = static_cast<uint32_t>(model.HighestShaderModel);
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)))) {
        caps_.resource_binding_tier = static_cast<uint32_t>(options.ResourceBindingTier);
        /* Read as a plain integer: drivers report tier 4, past the last
         * value the headers' enum names, and loading that through the enum
         * is undefined. */
        uint32_t tiled_tier = 0u;
        __builtin_memcpy(&tiled_tier, &options.TiledResourcesTier, sizeof tiled_tier);
        caps_.tiled_resources_tier = tiled_tier;
        caps_.resource_heap_tier = static_cast<uint32_t>(options.ResourceHeapTier);
        caps_.conservative_rasterization_tier = static_cast<uint32_t>(options.ConservativeRasterizationTier);
        caps_.typed_uav_load_additional_formats = options.TypedUAVLoadAdditionalFormats ? 1u : 0u;
        caps_.rovs = options.ROVsSupported ? 1u : 0u;
        caps_.double_ops = options.DoublePrecisionFloatShaderOps ? 1u : 0u;
    }
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 waves = {};
    if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &waves, sizeof(waves)))) {
        caps_.wave_ops = waves.WaveOps ? 1u : 0u;
        caps_.wave_lane_min = waves.WaveLaneCountMin;
        caps_.wave_lane_max = waves.WaveLaneCountMax;
        caps_.total_lanes = waves.TotalLaneCount;
        caps_.int64_ops = waves.Int64ShaderOps ? 1u : 0u;
    }
    D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT address = {};
    if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &address, sizeof(address)))) {
        caps_.max_gpu_va_bits_per_resource = address.MaxGPUVirtualAddressBitsPerResource;
        caps_.max_gpu_va_bits_per_process = address.MaxGPUVirtualAddressBitsPerProcess;
    }
    for (uint32_t type = 0u; type < 4u; type++)
        caps_.descriptor_sizes[type] =
            device_->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));
}

StagedFrame *D3d12HostDevice::stage_frame(ID3D12CommandQueue *queue, ID3D12Resource *source)
{
    AV_SCOPE("D3d12HostDevice::stage_frame");
    return frames_.stage(queue, source);
}

}  // namespace av
