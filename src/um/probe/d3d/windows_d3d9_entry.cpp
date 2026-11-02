#include "um/probe/d3d/windows_d3d9_entry.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t channels = 4u;

}

WindowsD3d9Entry::WindowsD3d9Entry()
    : d3d9_(nullptr), device_(nullptr), readback_(nullptr), width_(0u), height_(0u)
{
    AV_SCOPE("WindowsD3d9Entry::WindowsD3d9Entry");
}

WindowsD3d9Entry::~WindowsD3d9Entry()
{
    AV_SCOPE("WindowsD3d9Entry::~WindowsD3d9Entry");
    release();
}

int32_t WindowsD3d9Entry::create_device(void *window, uint32_t width, uint32_t height)
{
    AV_SCOPE("WindowsD3d9Entry::create_device");
    d3d9_ = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3d9_ == nullptr) return static_cast<int32_t>(D3DERR_NOTAVAILABLE);
    D3DPRESENT_PARAMETERS asked = {};
    asked.BackBufferWidth = width;
    asked.BackBufferHeight = height;
    asked.BackBufferFormat = D3DFMT_X8R8G8B8;
    asked.BackBufferCount = 1u;
    asked.SwapEffect = D3DSWAPEFFECT_DISCARD;
    asked.hDeviceWindow = static_cast<HWND>(window);
    asked.Windowed = TRUE;
    const HRESULT created =
        d3d9_->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, static_cast<HWND>(window),
                            D3DCREATE_HARDWARE_VERTEXPROCESSING, &asked, &device_);
    if (created < 0) return created;
    width_ = width;
    height_ = height;
    return created;
}

int32_t WindowsD3d9Entry::adapter_ids(uint32_t &vendor, uint32_t &device)
{
    AV_SCOPE("WindowsD3d9Entry::adapter_ids");
    D3DADAPTER_IDENTIFIER9 identified = {};
    const HRESULT          asked = d3d9_->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0u, &identified);
    if (asked < 0) return asked;
    vendor = identified.VendorId;
    device = identified.DeviceId;
    return asked;
}

int32_t WindowsD3d9Entry::clear(const uint8_t *colour)
{
    AV_SCOPE("WindowsD3d9Entry::clear");
    const D3DCOLOR asked = D3DCOLOR_ARGB(colour[3], colour[0], colour[1], colour[2]);
    HRESULT        cleared = device_->Clear(0u, nullptr, D3DCLEAR_TARGET, asked, 1.0F, 0u);
    if (cleared < 0) return cleared;
    cleared = device_->BeginScene();
    if (cleared < 0) return cleared;
    return device_->EndScene();
}

int32_t WindowsD3d9Entry::read_back(uint8_t *pixels, uint32_t bytes)
{
    AV_SCOPE("WindowsD3d9Entry::read_back");
    IDirect3DSurface9 *target = nullptr;
    HRESULT            asked = device_->GetRenderTarget(0u, &target);
    if (asked < 0) return asked;
    if (readback_ == nullptr)
        asked = device_->CreateOffscreenPlainSurface(width_, height_, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                     &readback_, nullptr);
    if (asked >= 0) asked = device_->GetRenderTargetData(target, readback_);
    target->Release();
    if (asked < 0) return asked;
    D3DLOCKED_RECT locked = {};
    asked = readback_->LockRect(&locked, nullptr, D3DLOCK_READONLY);
    if (asked < 0) return asked;
    const uint32_t row = width_ * channels;
    for (uint32_t line = 0u; line < height_ && (line + 1u) * row <= bytes; line++) {
        const auto *const source =
            static_cast<const uint8_t *>(locked.pBits) + static_cast<uint32_t>(locked.Pitch) * line;
        for (uint32_t byte = 0u; byte < row; byte++) pixels[line * row + byte] = source[byte];
    }
    static_cast<void>(readback_->UnlockRect());
    return asked;
}

int32_t WindowsD3d9Entry::present()
{
    AV_SCOPE("WindowsD3d9Entry::present");
    return device_->Present(nullptr, nullptr, nullptr, nullptr);
}

void WindowsD3d9Entry::release()
{
    AV_SCOPE("WindowsD3d9Entry::release");
    if (readback_ != nullptr) readback_->Release();
    if (device_ != nullptr) device_->Release();
    if (d3d9_ != nullptr) d3d9_->Release();
    readback_ = nullptr;
    device_ = nullptr;
    d3d9_ = nullptr;
}

}
