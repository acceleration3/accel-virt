#include "device/shim/d3d12_host_device.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/d3d_runtime.hpp"

namespace av {

namespace {

constexpr uint64_t mebibyte = 1u << 20u;
constexpr uint32_t level_12_2 = 0xc200u;
/* The highest shader model asked about; the answer is the device's highest
 * at or below it. */
constexpr uint32_t shader_model_6_6 = 0x66u;

}  // namespace

D3d12HostDevice::D3d12HostDevice() : runtime_(nullptr), device_(nullptr), caps_{}
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
    if (device_ != nullptr) return false;
    void *made = nullptr;
    if (FAILED(runtime.create_device12(uuid, made)) || made == nullptr) return false;
    runtime_ = &runtime;
    device_ = static_cast<ID3D12Device *>(made);
    /* The vendor and device ids come from the GPU's Direct3D 11 answer; a
     * Direct3D 12 device has no adapter description natively. */
    measure(0u, 0u, 0u);
    return true;
}

void D3d12HostDevice::close()
{
    AV_SCOPE("D3d12HostDevice::close");
    if (device_ != nullptr) device_->Release();
    device_ = nullptr;
    runtime_ = nullptr;
    caps_ = d3d12proto::Caps12{};
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

ID3D12Heap *D3d12HostDevice::import_heap(void *address, uint64_t size) const
{
    AV_SCOPE("D3d12HostDevice::import_heap");
    if (device_ == nullptr || runtime_ == nullptr) return nullptr;
    void *heap = nullptr;
    if (FAILED(runtime_->open_host_heap(device_, address, size, heap))) return nullptr;
    return static_cast<ID3D12Heap *>(heap);
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

void D3d12HostDevice::measure(uint32_t vendor_id, uint32_t device_id, uint64_t video_memory)
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
        caps_.tiled_resources_tier = static_cast<uint32_t>(options.TiledResourcesTier);
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
    caps_.vendor_id = vendor_id;
    caps_.device_id = device_id;
    caps_.dedicated_video_memory_mb = static_cast<uint32_t>(video_memory / mebibyte);
    for (uint32_t type = 0u; type < 4u; type++)
        caps_.descriptor_sizes[type] =
            device_->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));
}

}  // namespace av
