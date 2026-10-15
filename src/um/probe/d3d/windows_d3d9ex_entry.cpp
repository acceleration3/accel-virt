#include "um/probe/d3d/windows_d3d9ex_entry.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* A dirty region of one rectangle, as RGNDATA lays it out: the header, then
 * the rectangles. */
struct OneRectangleRegion {
    RGNDATAHEADER header;
    RECT          rectangle;
};

}  // namespace

WindowsD3d9ExEntry::WindowsD3d9ExEntry() : d3d9_(nullptr), device_(nullptr), chain_(nullptr)
{
    AV_SCOPE("WindowsD3d9ExEntry::WindowsD3d9ExEntry");
}

WindowsD3d9ExEntry::~WindowsD3d9ExEntry()
{
    AV_SCOPE("WindowsD3d9ExEntry::~WindowsD3d9ExEntry");
    release();
}

int32_t WindowsD3d9ExEntry::create_device(void *window)
{
    AV_SCOPE("WindowsD3d9ExEntry::create_device");
    const HRESULT made = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9_);
    if (made < 0) return made;
    D3DPRESENT_PARAMETERS asked = {};
    asked.BackBufferWidth = 1u;
    asked.BackBufferHeight = 1u;
    asked.BackBufferFormat = D3DFMT_X8R8G8B8;
    asked.BackBufferCount = 1u;
    asked.SwapEffect = D3DSWAPEFFECT_DISCARD;
    asked.hDeviceWindow = static_cast<HWND>(window);
    asked.Windowed = TRUE;
    return d3d9_->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, static_cast<HWND>(window),
                                 D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED |
                                     D3DCREATE_FPU_PRESERVE,
                                 &asked, nullptr, &device_);
}

int32_t WindowsD3d9ExEntry::create_swap_chain(void *window, uint32_t width, uint32_t height)
{
    AV_SCOPE("WindowsD3d9ExEntry::create_swap_chain");
    D3DPRESENT_PARAMETERS asked = {};
    asked.BackBufferWidth = width;
    asked.BackBufferHeight = height;
    asked.BackBufferFormat = D3DFMT_X8R8G8B8;
    asked.BackBufferCount = 1u;
    asked.SwapEffect = D3DSWAPEFFECT_COPY;
    asked.hDeviceWindow = static_cast<HWND>(window);
    asked.Windowed = TRUE;
    return device_->CreateAdditionalSwapChain(&asked, &chain_);
}

int32_t WindowsD3d9ExEntry::draw(const uint8_t *colour)
{
    AV_SCOPE("WindowsD3d9ExEntry::draw");
    IDirect3DSurface9 *buffer = nullptr;
    HRESULT            drawn = chain_->GetBackBuffer(0u, D3DBACKBUFFER_TYPE_MONO, &buffer);
    if (drawn < 0) return drawn;
    drawn = device_->SetRenderTarget(0u, buffer);
    buffer->Release();
    if (drawn >= 0) drawn = device_->BeginScene();
    if (drawn < 0) return drawn;
    drawn = device_->Clear(0u, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(colour[3], colour[0], colour[1], colour[2]),
                           1.0F, 0u);
    const HRESULT ended = device_->EndScene();
    return drawn < 0 ? drawn : ended;
}

int32_t WindowsD3d9ExEntry::present(uint32_t width, uint32_t height)
{
    AV_SCOPE("WindowsD3d9ExEntry::present");
    OneRectangleRegion dirty = {};
    dirty.rectangle = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    dirty.header.dwSize = sizeof dirty.header;
    dirty.header.iType = RDH_RECTANGLES;
    dirty.header.nCount = 1u;
    dirty.header.nRgnSize = sizeof dirty.rectangle;
    dirty.header.rcBound = dirty.rectangle;
    return chain_->Present(nullptr, nullptr, nullptr, reinterpret_cast<const RGNDATA *>(&dirty), 0u);
}

int32_t WindowsD3d9ExEntry::wait_for_blank()
{
    AV_SCOPE("WindowsD3d9ExEntry::wait_for_blank");
    return device_->WaitForVBlank(0u);
}

void WindowsD3d9ExEntry::release()
{
    AV_SCOPE("WindowsD3d9ExEntry::release");
    if (chain_ != nullptr) chain_->Release();
    if (device_ != nullptr) device_->Release();
    if (d3d9_ != nullptr) d3d9_->Release();
    chain_ = nullptr;
    device_ = nullptr;
    d3d9_ = nullptr;
}

}  // namespace av
