#include <cstdio>
#include <cwchar>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>

int wmain(int argc, wchar_t **argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    const bool shared = argc > 1 && wcscmp(argv[1], L"shared") == 0;
    const bool formatted = argc > 1 && wcscmp(argv[1], L"pixelformat") == 0;
    const int  seconds = argc > 2 ? _wtoi(argv[2]) : 5;
    WNDCLASSW type = {};
    type.lpfnWndProc = DefWindowProcW;
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"d3d11_flip";
    RegisterClassW(&type);
    HWND window = CreateWindowW(L"d3d11_flip", L"d3d11_flip", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 700, 100, 656, 399,
                                nullptr, nullptr, type.hInstance, nullptr);
    if (formatted) {
        HDC dc = GetDC(window);
        PIXELFORMATDESCRIPTOR wanted = {};
        wanted.nSize = sizeof wanted;
        wanted.nVersion = 1;
        wanted.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        wanted.iPixelType = PFD_TYPE_RGBA;
        wanted.cColorBits = 32;
        const int format = ChoosePixelFormat(dc, &wanted);
        wprintf(L"pixel format %d set %d\n", format, SetPixelFormat(dc, format, &wanted) ? 1 : 0);
        ReleaseDC(window, dc);
    }
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
    HRESULT made = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                     &device, nullptr, &context);
    wprintf(L"device 0x%08lx\n", static_cast<unsigned long>(made));
    if (FAILED(made)) return 1;
    IDXGIDevice *dxgi = nullptr;
    IDXGIAdapter *adapter = nullptr;
    IDXGIFactory2 *factory = nullptr;
    device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgi));
    dxgi->GetAdapter(&adapter);
    adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void **>(&factory));
    RECT area = {};
    GetClientRect(window, &area);
    DXGI_SWAP_CHAIN_DESC1 described = {};
    described.Width = area.right;
    described.Height = area.bottom;
    described.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    described.SampleDesc = {1, 0};
    described.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    described.BufferCount = 2;
    described.Scaling = DXGI_SCALING_STRETCH;
    described.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1 *chain = nullptr;
    made = factory->CreateSwapChainForHwnd(device, window, &described, nullptr, nullptr, &chain);
    wprintf(L"chain 0x%08lx\n", static_cast<unsigned long>(made));
    if (FAILED(made)) return 2;
    ID3D11Texture2D *slot = nullptr;
    ID3D11RenderTargetView *slot_view = nullptr;
    if (shared) {
        D3D11_TEXTURE2D_DESC texture = {};
        texture.Width = described.Width;
        texture.Height = described.Height;
        texture.MipLevels = 1;
        texture.ArraySize = 1;
        texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture.SampleDesc = {1, 0};
        texture.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        texture.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
        made = device->CreateTexture2D(&texture, nullptr, &slot);
        wprintf(L"shared texture 0x%08lx\n", static_cast<unsigned long>(made));
        if (FAILED(made)) return 3;
        device->CreateRenderTargetView(slot, nullptr, &slot_view);
    }
    const ULONGLONG until = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000u;
    int frames = 0;
    while (GetTickCount64() < until) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
        ID3D11Texture2D *back = nullptr;
        chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&back));
        if (shared) {
            const float green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
            context->ClearRenderTargetView(slot_view, green);
            context->CopyResource(back, slot);
        } else {
            ID3D11RenderTargetView *view = nullptr;
            device->CreateRenderTargetView(back, nullptr, &view);
            const float blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
            context->ClearRenderTargetView(view, blue);
            view->Release();
        }
        back->Release();
        chain->Present(1, 0);
        frames++;
    }
    wprintf(L"presents %d\n", frames);
    return 0;
}
