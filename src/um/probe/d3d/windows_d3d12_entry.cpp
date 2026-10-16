#include "um/probe/d3d/windows_d3d12_entry.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

namespace {

constexpr uint32_t channels = 4u;
constexpr float    full = 255.0F;
/* What D3D12 rounds a texture copy's rows up to. */
constexpr uint32_t row_alignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

uint32_t aligned_row(uint32_t width)
{
    AV_SCOPE("aligned_row");
    const uint32_t row = width * channels;
    return (row + row_alignment - 1u) / row_alignment * row_alignment;
}

}  // namespace

WindowsD3d12Entry::WindowsD3d12Entry()
    : factory_(nullptr), adapter_(nullptr), device_(nullptr), queue_(nullptr), allocator_(nullptr),
      commands_(nullptr), views_(nullptr), target_(nullptr), readback_(nullptr), fence_(nullptr),
      reached_(nullptr), signalled_(0u), width_(0u), height_(0u), row_pitch_(0u), library_blob_{}
{
    AV_SCOPE("WindowsD3d12Entry::WindowsD3d12Entry");
}

WindowsD3d12Entry::~WindowsD3d12Entry()
{
    AV_SCOPE("WindowsD3d12Entry::~WindowsD3d12Entry");
    release();
}

int32_t WindowsD3d12Entry::create_device(uint32_t &feature_level)
{
    AV_SCOPE("WindowsD3d12Entry::create_device");
    HRESULT asked = CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void **>(&factory_));
    if (asked < 0) return asked;
    asked = factory_->EnumAdapters1(0u, &adapter_);
    if (asked < 0) return asked;
    asked = D3D12CreateDevice(adapter_, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                              reinterpret_cast<void **>(&device_));
    if (asked < 0) return asked;
    /* What the device answers when asked for the levels in order: the highest
     * it supports of the two vkd3d-proton offers. */
    const D3D_FEATURE_LEVEL offered[] = {D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0,
                                         D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {};
    levels.NumFeatureLevels = sizeof offered / sizeof offered[0];
    levels.pFeatureLevelsRequested = offered;
    if (device_->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof levels) >= 0)
        feature_level = static_cast<uint32_t>(levels.MaxSupportedFeatureLevel);
    else
        feature_level = static_cast<uint32_t>(D3D_FEATURE_LEVEL_11_0);
    return asked;
}

int32_t WindowsD3d12Entry::adapter_luid(uint64_t &luid)
{
    AV_SCOPE("WindowsD3d12Entry::adapter_luid");
    DXGI_ADAPTER_DESC1 described = {};
    const HRESULT      asked = adapter_->GetDesc1(&described);
    if (asked < 0) return asked;
    luid = (static_cast<uint64_t>(static_cast<uint32_t>(described.AdapterLuid.HighPart)) << 32) |
           described.AdapterLuid.LowPart;
    return asked;
}

int32_t WindowsD3d12Entry::pipeline_library(uint32_t &serialized)
{
    AV_SCOPE("WindowsD3d12Entry::pipeline_library");
    serialized = 0u;
    D3D12_FEATURE_DATA_SHADER_CACHE cache = {};
    HRESULT asked = device_->CheckFeatureSupport(D3D12_FEATURE_SHADER_CACHE, &cache, sizeof cache);
    if (asked < 0) return asked;
    if ((cache.SupportFlags & D3D12_SHADER_CACHE_SUPPORT_LIBRARY) == 0) return asked;
    ID3D12Device1 *device = nullptr;
    asked = device_->QueryInterface(__uuidof(ID3D12Device1), reinterpret_cast<void **>(&device));
    if (asked < 0) return asked;
    ID3D12PipelineLibrary *empty = nullptr;
    asked = device->CreatePipelineLibrary(nullptr, 0u, __uuidof(ID3D12PipelineLibrary),
                                          reinterpret_cast<void **>(&empty));
    if (asked >= 0) {
        const SIZE_T bytes = empty->GetSerializedSize();
        asked = bytes <= sizeof library_blob_ ? empty->Serialize(library_blob_, bytes)
                                              : static_cast<HRESULT>(E_OUTOFMEMORY);
        empty->Release();
        ID3D12PipelineLibrary *again = nullptr;
        if (asked >= 0)
            asked = device->CreatePipelineLibrary(library_blob_, bytes, __uuidof(ID3D12PipelineLibrary),
                                                  reinterpret_cast<void **>(&again));
        if (again != nullptr) again->Release();
        serialized = static_cast<uint32_t>(bytes);
    }
    device->Release();
    if (asked < 0) return asked;
    /* A driver that refused a slot may have had the runtime remove the
     * device instead of failing the call. */
    return device_->GetDeviceRemovedReason();
}

int32_t WindowsD3d12Entry::create_queue()
{
    AV_SCOPE("WindowsD3d12Entry::create_queue");
    D3D12_COMMAND_QUEUE_DESC described = {};
    described.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    HRESULT asked =
        device_->CreateCommandQueue(&described, __uuidof(ID3D12CommandQueue), reinterpret_cast<void **>(&queue_));
    if (asked < 0) return asked;
    asked = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                            reinterpret_cast<void **>(&allocator_));
    if (asked < 0) return asked;
    asked = device_->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_, nullptr,
                                       __uuidof(ID3D12GraphicsCommandList),
                                       reinterpret_cast<void **>(&commands_));
    if (asked < 0) return asked;
    asked = commands_->Close();
    if (asked < 0) return asked;
    asked = device_->CreateFence(0u, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                 reinterpret_cast<void **>(&fence_));
    if (asked < 0) return asked;
    reached_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return reached_ != nullptr ? asked : static_cast<int32_t>(E_OUTOFMEMORY);
}

int32_t WindowsD3d12Entry::create_render_target(uint32_t width, uint32_t height)
{
    AV_SCOPE("WindowsD3d12Entry::create_render_target");
    width_ = width;
    height_ = height;
    row_pitch_ = aligned_row(width);
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC described = {};
    described.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    described.Width = width;
    described.Height = height;
    described.DepthOrArraySize = 1u;
    described.MipLevels = 1u;
    described.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    described.SampleDesc.Count = 1u;
    described.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    described.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    HRESULT asked = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &described,
                                                     D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr,
                                                     __uuidof(ID3D12Resource),
                                                     reinterpret_cast<void **>(&target_));
    if (asked < 0) return asked;
    D3D12_DESCRIPTOR_HEAP_DESC views = {};
    views.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    views.NumDescriptors = 1u;
    asked = device_->CreateDescriptorHeap(&views, __uuidof(ID3D12DescriptorHeap),
                                          reinterpret_cast<void **>(&views_));
    if (asked < 0) return asked;
    device_->CreateRenderTargetView(target_, nullptr, views_->GetCPUDescriptorHandleForHeapStart());
    D3D12_HEAP_PROPERTIES readback = {};
    readback.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC rows = {};
    rows.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rows.Width = static_cast<UINT64>(row_pitch_) * height;
    rows.Height = 1u;
    rows.DepthOrArraySize = 1u;
    rows.MipLevels = 1u;
    rows.Format = DXGI_FORMAT_UNKNOWN;
    rows.SampleDesc.Count = 1u;
    rows.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return device_->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &rows,
                                            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource),
                                            reinterpret_cast<void **>(&readback_));
}

int32_t WindowsD3d12Entry::clear_and_copy(const uint8_t *colour)
{
    AV_SCOPE("WindowsD3d12Entry::clear_and_copy");
    HRESULT asked = allocator_->Reset();
    if (asked < 0) return asked;
    asked = commands_->Reset(allocator_, nullptr);
    if (asked < 0) return asked;
    const float asked_colour[channels] = {static_cast<float>(colour[0]) / full,
                                          static_cast<float>(colour[1]) / full,
                                          static_cast<float>(colour[2]) / full,
                                          static_cast<float>(colour[3]) / full};
    commands_->ClearRenderTargetView(views_->GetCPUDescriptorHandleForHeapStart(), asked_colour, 0u, nullptr);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = target_;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    commands_->ResourceBarrier(1u, &barrier);
    D3D12_TEXTURE_COPY_LOCATION from = {};
    from.pResource = target_;
    from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION into = {};
    into.pResource = readback_;
    into.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    into.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    into.PlacedFootprint.Footprint.Width = width_;
    into.PlacedFootprint.Footprint.Height = height_;
    into.PlacedFootprint.Footprint.Depth = 1u;
    into.PlacedFootprint.Footprint.RowPitch = row_pitch_;
    commands_->CopyTextureRegion(&into, 0u, 0u, 0u, &from, nullptr);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commands_->ResourceBarrier(1u, &barrier);
    asked = commands_->Close();
    if (asked < 0) return asked;
    ID3D12CommandList *const list = commands_;
    queue_->ExecuteCommandLists(1u, &list);
    signalled_++;
    return queue_->Signal(fence_, signalled_);
}

int32_t WindowsD3d12Entry::wait()
{
    AV_SCOPE("WindowsD3d12Entry::wait");
    if (fence_->GetCompletedValue() >= signalled_) return 0;
    const HRESULT asked = fence_->SetEventOnCompletion(signalled_, static_cast<HANDLE>(reached_));
    if (asked < 0) return asked;
    /* Two seconds: a clear the host has not finished by then is a hang, not a
     * slow GPU. */
    return WaitForSingleObject(static_cast<HANDLE>(reached_), 2000u) == WAIT_OBJECT_0
               ? 0
               : static_cast<int32_t>(E_FAIL);
}

int32_t WindowsD3d12Entry::read_back(uint8_t *pixels, uint32_t bytes)
{
    AV_SCOPE("WindowsD3d12Entry::read_back");
    void         *mapped = nullptr;
    const HRESULT asked = readback_->Map(0u, nullptr, &mapped);
    if (asked < 0) return asked;
    const uint32_t row = width_ * channels;
    for (uint32_t line = 0u; line < height_ && (line + 1u) * row <= bytes; line++) {
        const auto *const source = static_cast<const uint8_t *>(mapped) + line * row_pitch_;
        for (uint32_t byte = 0u; byte < row; byte++) pixels[line * row + byte] = source[byte];
    }
    readback_->Unmap(0u, nullptr);
    return asked;
}

void WindowsD3d12Entry::release()
{
    AV_SCOPE("WindowsD3d12Entry::release");
    if (reached_ != nullptr) static_cast<void>(CloseHandle(static_cast<HANDLE>(reached_)));
    if (readback_ != nullptr) readback_->Release();
    if (target_ != nullptr) target_->Release();
    if (views_ != nullptr) views_->Release();
    if (fence_ != nullptr) fence_->Release();
    if (commands_ != nullptr) commands_->Release();
    if (allocator_ != nullptr) allocator_->Release();
    if (queue_ != nullptr) queue_->Release();
    if (device_ != nullptr) device_->Release();
    if (adapter_ != nullptr) adapter_->Release();
    if (factory_ != nullptr) factory_->Release();
    reached_ = nullptr;
    readback_ = nullptr;
    target_ = nullptr;
    views_ = nullptr;
    fence_ = nullptr;
    commands_ = nullptr;
    allocator_ = nullptr;
    queue_ = nullptr;
    device_ = nullptr;
    adapter_ = nullptr;
    factory_ = nullptr;
}

}  // namespace av
