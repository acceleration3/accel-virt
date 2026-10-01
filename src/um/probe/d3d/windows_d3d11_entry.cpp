#include "um/probe/d3d/windows_d3d11_entry.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t channels = 4u;
constexpr float    full = 255.0F;

}  // namespace

WindowsD3d11Entry::WindowsD3d11Entry()
    : swapchain_(nullptr), device_(nullptr), context_(nullptr), back_(nullptr), target_(nullptr),
      staging_(nullptr), width_(0u), height_(0u)
{
    AV_SCOPE("WindowsD3d11Entry::WindowsD3d11Entry");
}

WindowsD3d11Entry::~WindowsD3d11Entry()
{
    AV_SCOPE("WindowsD3d11Entry::~WindowsD3d11Entry");
    release();
}

int32_t WindowsD3d11Entry::create_device(void *window, uint32_t width, uint32_t height, uint32_t &feature_level)
{
    AV_SCOPE("WindowsD3d11Entry::create_device");
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
    D3D_FEATURE_LEVEL made = D3D_FEATURE_LEVEL_11_0;
    const HRESULT     created =
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0u, nullptr, 0u,
                                      D3D11_SDK_VERSION, &described, &swapchain_, &device_, &made, &context_);
    if (created < 0) return created;
    width_ = width;
    height_ = height;
    feature_level = static_cast<uint32_t>(made);
    return created;
}

int32_t WindowsD3d11Entry::adapter_luid(uint64_t &luid)
{
    AV_SCOPE("WindowsD3d11Entry::adapter_luid");
    IDXGIDevice *dxgi = nullptr;
    HRESULT      asked = device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgi));
    if (asked < 0) return asked;
    IDXGIAdapter *adapter = nullptr;
    asked = dxgi->GetAdapter(&adapter);
    dxgi->Release();
    if (asked < 0) return asked;
    DXGI_ADAPTER_DESC described = {};
    asked = adapter->GetDesc(&described);
    adapter->Release();
    if (asked < 0) return asked;
    luid = (static_cast<uint64_t>(static_cast<uint32_t>(described.AdapterLuid.HighPart)) << 32) |
           described.AdapterLuid.LowPart;
    return asked;
}

int32_t WindowsD3d11Entry::create_render_target()
{
    AV_SCOPE("WindowsD3d11Entry::create_render_target");
    HRESULT asked = swapchain_->GetBuffer(0u, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&back_));
    if (asked < 0) return asked;
    asked = device_->CreateRenderTargetView(back_, nullptr, &target_);
    if (asked < 0) return asked;
    D3D11_TEXTURE2D_DESC described = {};
    described.Width = width_;
    described.Height = height_;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    described.SampleDesc.Count = 1u;
    described.Usage = D3D11_USAGE_STAGING;
    described.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    return device_->CreateTexture2D(&described, nullptr, &staging_);
}

void WindowsD3d11Entry::clear(const uint8_t *colour)
{
    AV_SCOPE("WindowsD3d11Entry::clear");
    const float asked[channels] = {static_cast<float>(colour[0]) / full, static_cast<float>(colour[1]) / full,
                                   static_cast<float>(colour[2]) / full, static_cast<float>(colour[3]) / full};
    context_->ClearRenderTargetView(target_, asked);
}

int32_t WindowsD3d11Entry::read_back(uint8_t *pixels, uint32_t bytes)
{
    AV_SCOPE("WindowsD3d11Entry::read_back");
    context_->CopyResource(staging_, back_);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT            asked = context_->Map(staging_, 0u, D3D11_MAP_READ, 0u, &mapped);
    if (asked < 0) return asked;
    const uint32_t row = width_ * channels;
    for (uint32_t line = 0u; line < height_ && (line + 1u) * row <= bytes; line++) {
        const auto *const source = static_cast<const uint8_t *>(mapped.pData) + line * mapped.RowPitch;
        for (uint32_t byte = 0u; byte < row; byte++) pixels[line * row + byte] = source[byte];
    }
    context_->Unmap(staging_, 0u);
    return asked;
}

/* AV_PROBE_HOLD=<frames> keeps the window presenting, a colour a frame at
 * vsync, so the frames can be watched on the guest's screen. */
int32_t WindowsD3d11Entry::present()
{
    AV_SCOPE("WindowsD3d11Entry::present");
    const HRESULT first = swapchain_->Present(0u, 0u);
    wchar_t value[16] = {};
    const DWORD length = GetEnvironmentVariableW(L"AV_PROBE_HOLD", value, 16u);
    uint32_t frames = 0u;
    for (DWORD index = 0u; index < length && value[index] >= L'0' && value[index] <= L'9'; index++)
        frames = frames * 10u + static_cast<uint32_t>(value[index] - L'0');
    for (uint32_t frame = 0u; frame < frames; frame++) {
        const float phase = static_cast<float>(frame % 120u) / 120.0F;
        const float cycling[channels] = {phase, 1.0F - phase, 0.5F, 1.0F};
        context_->ClearRenderTargetView(target_, cycling);
        static_cast<void>(swapchain_->Present(1u, 0u));
    }
    return first;
}

void WindowsD3d11Entry::release()
{
    AV_SCOPE("WindowsD3d11Entry::release");
    if (staging_ != nullptr) staging_->Release();
    if (target_ != nullptr) target_->Release();
    if (back_ != nullptr) back_->Release();
    if (context_ != nullptr) context_->Release();
    if (swapchain_ != nullptr) swapchain_->Release();
    if (device_ != nullptr) device_->Release();
    staging_ = nullptr;
    target_ = nullptr;
    back_ = nullptr;
    context_ = nullptr;
    swapchain_ = nullptr;
    device_ = nullptr;
}

}  // namespace av
