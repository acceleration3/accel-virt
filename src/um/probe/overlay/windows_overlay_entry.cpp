#include "um/probe/overlay/windows_overlay_entry.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

namespace {

constexpr wchar_t window_class[] = L"avoverlayprobe";
constexpr float   full = 255.0F;

LRESULT CALLBACK overlay_window_procedure(HWND window, UINT message, WPARAM wide, LPARAM lengthy)
{
    AV_SCOPE("overlay_window_procedure");
    return DefWindowProcW(window, message, wide, lengthy);
}

}  // namespace

WindowsOverlayEntry::WindowsOverlayEntry()
    : instance_(GetModuleHandleW(nullptr)), atom_(0u), window_(nullptr), device_(nullptr), context_(nullptr),
      factory_(nullptr), output_(nullptr), swapchain_(nullptr), target_(nullptr)
{
    AV_SCOPE("WindowsOverlayEntry::WindowsOverlayEntry");
}

WindowsOverlayEntry::~WindowsOverlayEntry()
{
    AV_SCOPE("WindowsOverlayEntry::~WindowsOverlayEntry");
    release();
}

int32_t WindowsOverlayEntry::create_device()
{
    AV_SCOPE("WindowsOverlayEntry::create_device");
    D3D_FEATURE_LEVEL made = D3D_FEATURE_LEVEL_11_0;
    HRESULT asked = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                      nullptr, 0u, D3D11_SDK_VERSION, &device_, &made, &context_);
    if (asked < 0) return asked;
    IDXGIDevice *dxgi = nullptr;
    asked = device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgi));
    if (asked < 0) return asked;
    IDXGIAdapter *adapter = nullptr;
    asked = dxgi->GetAdapter(&adapter);
    dxgi->Release();
    if (asked < 0) return asked;
    asked = adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void **>(&factory_));
    IDXGIOutput *output = nullptr;
    if (asked >= 0) asked = adapter->EnumOutputs(0u, &output);
    adapter->Release();
    if (asked < 0) return asked;
    asked = output->QueryInterface(__uuidof(IDXGIOutput3), reinterpret_cast<void **>(&output_));
    output->Release();
    if (asked < 0) return asked;
    WNDCLASSEXW description = {};
    description.cbSize = sizeof description;
    description.lpfnWndProc = &overlay_window_procedure;
    description.hInstance = static_cast<HINSTANCE>(instance_);
    description.lpszClassName = window_class;
    atom_ = RegisterClassExW(&description);
    return atom_ != 0u ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

int32_t WindowsOverlayEntry::supports_overlays(bool &supported)
{
    AV_SCOPE("WindowsOverlayEntry::supports_overlays");
    supported = output_->SupportsOverlays() != FALSE;
    return S_OK;
}

int32_t WindowsOverlayEntry::overlay_support(uint32_t &flags)
{
    AV_SCOPE("WindowsOverlayEntry::overlay_support");
    UINT support = 0u;
    const HRESULT asked = output_->CheckOverlaySupport(DXGI_FORMAT_B8G8R8A8_UNORM, device_, &support);
    flags = support;
    return asked;
}

int32_t WindowsOverlayEntry::create_swap_chain(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    AV_SCOPE("WindowsOverlayEntry::create_swap_chain");
    drop_swap_chain();
    if (width == 0u || height == 0u) {
        DXGI_OUTPUT_DESC described = {};
        const HRESULT asked = output_->GetDesc(&described);
        if (asked < 0) return asked;
        x = described.DesktopCoordinates.left;
        y = described.DesktopCoordinates.top;
        width = static_cast<uint32_t>(described.DesktopCoordinates.right - described.DesktopCoordinates.left);
        height = static_cast<uint32_t>(described.DesktopCoordinates.bottom - described.DesktopCoordinates.top);
    }
    window_ = CreateWindowExW(WS_EX_TOPMOST, window_class, L"avoverlayprobe", WS_POPUP | WS_VISIBLE, x, y,
                              static_cast<int>(width), static_cast<int>(height), nullptr, nullptr,
                              static_cast<HINSTANCE>(instance_), nullptr);
    if (window_ == nullptr) return HRESULT_FROM_WIN32(GetLastError());
    pump();
    DXGI_SWAP_CHAIN_DESC1 described = {};
    described.Width = width;
    described.Height = height;
    described.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    described.SampleDesc.Count = 1u;
    described.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    described.BufferCount = 2u;
    described.Scaling = DXGI_SCALING_STRETCH;
    described.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    described.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    HRESULT asked = factory_->CreateSwapChainForHwnd(device_, window_, &described, nullptr, nullptr, &swapchain_);
    if (asked < 0) return asked;
    ID3D11Texture2D *back = nullptr;
    asked = swapchain_->GetBuffer(0u, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&back));
    if (asked < 0) return asked;
    asked = device_->CreateRenderTargetView(back, nullptr, &target_);
    back->Release();
    return asked;
}

/* FLIP_DISCARD has one view of buffer 0 that always names the back buffer. */
int32_t WindowsOverlayEntry::present(const uint8_t *colour)
{
    AV_SCOPE("WindowsOverlayEntry::present");
    const float rgba[4] = {static_cast<float>(colour[0]) / full, static_cast<float>(colour[1]) / full,
                           static_cast<float>(colour[2]) / full, static_cast<float>(colour[3]) / full};
    context_->OMSetRenderTargets(1u, &target_, nullptr);
    context_->ClearRenderTargetView(target_, rgba);
    const HRESULT presented = swapchain_->Present(1u, 0u);
    pump();
    return presented;
}

int32_t WindowsOverlayEntry::composition(uint32_t &mode)
{
    AV_SCOPE("WindowsOverlayEntry::composition");
    IDXGISwapChainMedia *media = nullptr;
    HRESULT asked = swapchain_->QueryInterface(__uuidof(IDXGISwapChainMedia), reinterpret_cast<void **>(&media));
    if (asked < 0) return asked;
    DXGI_FRAME_STATISTICS_MEDIA statistics = {};
    asked = media->GetFrameStatisticsMedia(&statistics);
    media->Release();
    mode = static_cast<uint32_t>(statistics.CompositionMode);
    return asked;
}

void WindowsOverlayEntry::pump()
{
    AV_SCOPE("WindowsOverlayEntry::pump");
    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE) != FALSE) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void WindowsOverlayEntry::drop_swap_chain()
{
    AV_SCOPE("WindowsOverlayEntry::drop_swap_chain");
    if (context_ != nullptr) context_->OMSetRenderTargets(0u, nullptr, nullptr);
    if (target_ != nullptr) target_->Release();
    if (swapchain_ != nullptr) swapchain_->Release();
    if (context_ != nullptr) context_->Flush();
    if (window_ != nullptr) DestroyWindow(window_);
    target_ = nullptr;
    swapchain_ = nullptr;
    window_ = nullptr;
}

void WindowsOverlayEntry::release()
{
    AV_SCOPE("WindowsOverlayEntry::release");
    drop_swap_chain();
    if (output_ != nullptr) output_->Release();
    if (factory_ != nullptr) factory_->Release();
    if (context_ != nullptr) context_->Release();
    if (device_ != nullptr) device_->Release();
    output_ = nullptr;
    factory_ = nullptr;
    context_ = nullptr;
    device_ = nullptr;
}

}  // namespace av
