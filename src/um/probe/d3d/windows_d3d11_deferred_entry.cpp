#include "um/probe/d3d/windows_d3d11_deferred_entry.hpp"

#include "core/trace/function_scope.hpp"

/* psapi.h is not in the pinned SDK subset; PROCESS_MEMORY_COUNTERS_EX and the
 * call as that header declares them (SDK 10.0.26100, um/psapi.h). */
extern "C" {
struct AvProcessMemoryCountersEx {
    DWORD  cb;
    DWORD  PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivateUsage;
};
BOOL WINAPI K32GetProcessMemoryInfo(HANDLE process, AvProcessMemoryCountersEx *counters, DWORD bytes);
}

namespace av {

namespace {

constexpr HRESULT invalid_argument = static_cast<HRESULT>(0x80070057u);

}  // namespace

WindowsD3d11DeferredEntry::WindowsD3d11DeferredEntry()
    : swapchain_(nullptr), device_(nullptr), immediate_(nullptr), constants_{}, constant_count_(0u),
      geometry_(nullptr), readback_(nullptr), deferred_{}, lists_{}, context_count_(0u)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::WindowsD3d11DeferredEntry");
}

WindowsD3d11DeferredEntry::~WindowsD3d11DeferredEntry()
{
    AV_SCOPE("WindowsD3d11DeferredEntry::~WindowsD3d11DeferredEntry");
    release();
}

int32_t WindowsD3d11DeferredEntry::create_device(void *window, uint32_t width, uint32_t height)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::create_device");
    DXGI_SWAP_CHAIN_DESC described = {};
    described.BufferDesc.Width = width;
    described.BufferDesc.Height = height;
    described.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    described.SampleDesc.Count = 1u;
    described.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    described.BufferCount = 2u;
    described.OutputWindow = static_cast<HWND>(window);
    described.Windowed = TRUE;
    described.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    return D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0u, nullptr, 0u,
                                         D3D11_SDK_VERSION, &described, &swapchain_, &device_, nullptr, &immediate_);
}

int32_t WindowsD3d11DeferredEntry::dynamic_buffer(uint32_t bytes, uint32_t bind, ID3D11Buffer **made)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::dynamic_buffer");
    D3D11_BUFFER_DESC described = {};
    described.ByteWidth = bytes;
    described.Usage = D3D11_USAGE_DYNAMIC;
    described.BindFlags = bind;
    described.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return device_->CreateBuffer(&described, nullptr, made);
}

int32_t WindowsD3d11DeferredEntry::create_buffers(const uint32_t *sizes, uint32_t count, uint32_t geometry_bytes,
                                                  uint32_t readback_bytes)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::create_buffers");
    if (count > most_constants) return invalid_argument;
    for (; constant_count_ < count; constant_count_++) {
        const HRESULT made = dynamic_buffer(sizes[constant_count_], D3D11_BIND_CONSTANT_BUFFER,
                                            &constants_[constant_count_]);
        if (made < 0) return made;
    }
    const HRESULT made = dynamic_buffer(geometry_bytes, D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER,
                                        &geometry_);
    if (made < 0) return made;
    D3D11_BUFFER_DESC described = {};
    described.ByteWidth = readback_bytes;
    described.Usage = D3D11_USAGE_STAGING;
    described.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    return device_->CreateBuffer(&described, nullptr, &readback_);
}

int32_t WindowsD3d11DeferredEntry::create_contexts(uint32_t count)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::create_contexts");
    if (count > most_contexts) return invalid_argument;
    for (; context_count_ < count; context_count_++) {
        const HRESULT made = device_->CreateDeferredContext(0u, &deferred_[context_count_]);
        if (made < 0) return made;
    }
    return S_OK;
}

int32_t WindowsD3d11DeferredEntry::write(uint32_t context, ID3D11Buffer *buffer, D3D11_MAP type, uint32_t offset,
                                         uint32_t bytes, uint8_t value)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::write");
    if (context >= context_count_) return invalid_argument;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT            asked = deferred_[context]->Map(buffer, 0u, type, 0u, &mapped);
    if (asked < 0) return asked;
    auto *const at = static_cast<uint8_t *>(mapped.pData) + offset;
    for (uint32_t byte = 0u; byte < bytes; byte++) at[byte] = value;
    deferred_[context]->Unmap(buffer, 0u);
    return asked;
}

int32_t WindowsD3d11DeferredEntry::write_constants(uint32_t context, uint32_t buffer, uint8_t value)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::write_constants");
    if (buffer >= constant_count_) return invalid_argument;
    D3D11_BUFFER_DESC described = {};
    constants_[buffer]->GetDesc(&described);
    return write(context, constants_[buffer], D3D11_MAP_WRITE_DISCARD, 0u, described.ByteWidth, value);
}

int32_t WindowsD3d11DeferredEntry::discard_geometry(uint32_t context, uint32_t bytes, uint8_t value)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::discard_geometry");
    return write(context, geometry_, D3D11_MAP_WRITE_DISCARD, 0u, bytes, value);
}

int32_t WindowsD3d11DeferredEntry::append_geometry(uint32_t context, uint32_t offset, uint32_t bytes, uint8_t value)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::append_geometry");
    return write(context, geometry_, D3D11_MAP_WRITE_NO_OVERWRITE, offset, bytes, value);
}

int32_t WindowsD3d11DeferredEntry::finish(uint32_t context)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::finish");
    if (context >= context_count_ || lists_[context] != nullptr) return invalid_argument;
    return deferred_[context]->FinishCommandList(FALSE, &lists_[context]);
}

int32_t WindowsD3d11DeferredEntry::execute(uint32_t context)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::execute");
    if (context >= context_count_ || lists_[context] == nullptr) return invalid_argument;
    immediate_->ExecuteCommandList(lists_[context], FALSE);
    lists_[context]->Release();
    lists_[context] = nullptr;
    return S_OK;
}

int32_t WindowsD3d11DeferredEntry::present()
{
    AV_SCOPE("WindowsD3d11DeferredEntry::present");
    return swapchain_->Present(0u, 0u);
}

int32_t WindowsD3d11DeferredEntry::read_geometry(uint8_t *bytes, uint32_t count)
{
    AV_SCOPE("WindowsD3d11DeferredEntry::read_geometry");
    const D3D11_BOX head = {0u, 0u, 0u, count, 1u, 1u};
    immediate_->CopySubresourceRegion(readback_, 0u, 0u, 0u, 0u, geometry_, 0u, &head);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT            asked = immediate_->Map(readback_, 0u, D3D11_MAP_READ, 0u, &mapped);
    if (asked < 0) return asked;
    for (uint32_t byte = 0u; byte < count; byte++) bytes[byte] = static_cast<const uint8_t *>(mapped.pData)[byte];
    immediate_->Unmap(readback_, 0u);
    return asked;
}

uint64_t WindowsD3d11DeferredEntry::committed_bytes()
{
    AV_SCOPE("WindowsD3d11DeferredEntry::committed_bytes");
    AvProcessMemoryCountersEx counters = {};
    counters.cb = sizeof counters;
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters) == FALSE) return 0u;
    return counters.PrivateUsage;
}

void WindowsD3d11DeferredEntry::release()
{
    AV_SCOPE("WindowsD3d11DeferredEntry::release");
    for (uint32_t context = 0u; context < most_contexts; context++) {
        if (lists_[context] != nullptr) lists_[context]->Release();
        if (deferred_[context] != nullptr) deferred_[context]->Release();
        lists_[context] = nullptr;
        deferred_[context] = nullptr;
    }
    for (uint32_t buffer = 0u; buffer < most_constants; buffer++) {
        if (constants_[buffer] != nullptr) constants_[buffer]->Release();
        constants_[buffer] = nullptr;
    }
    if (readback_ != nullptr) readback_->Release();
    if (geometry_ != nullptr) geometry_->Release();
    if (immediate_ != nullptr) immediate_->Release();
    if (swapchain_ != nullptr) swapchain_->Release();
    if (device_ != nullptr) device_->Release();
    readback_ = nullptr;
    geometry_ = nullptr;
    immediate_ = nullptr;
    swapchain_ = nullptr;
    device_ = nullptr;
    constant_count_ = 0u;
    context_count_ = 0u;
}

}  // namespace av
