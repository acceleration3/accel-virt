// d3d11_flip.exe: a window shown through a DXGI flip chain of R8G8B8A8 buffers,
// as the OpenGL and Vulkan drivers' window chains are: "direct" clears the back
// buffer blue; "shared" clears a texture shared by NT handle green and copies
// it into the back buffer, as a chain's slot is; "pixelformat" is "direct" on a
// window an OpenGL pixel format was set on first, as an OpenGL window is; "blt"
// is "direct" through a blt-model chain (DXGI_SWAP_EFFECT_DISCARD), whose
// presents go through the window's redirection surface; "bltout" is "blt" on
// the adapter that has an output, where the present needs no cross-adapter copy.
// Presents for a few seconds.
//
//   d3d11_flip.exe direct|shared|pixelformat|blt [seconds]
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
    const bool on_output = argc > 1 && wcscmp(argv[1], L"bltout") == 0;
    const bool blt = on_output || (argc > 1 && wcscmp(argv[1], L"blt") == 0);
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
    IDXGIAdapter1 *chosen = nullptr;
    if (on_output) {
        IDXGIFactory1 *enumerating = nullptr;
        CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&enumerating));
        for (UINT index = 0; enumerating != nullptr && chosen == nullptr; index++) {
            IDXGIAdapter1 *candidate = nullptr;
            if (FAILED(enumerating->EnumAdapters1(index, &candidate))) break;
            IDXGIOutput *output = nullptr;
            if (SUCCEEDED(candidate->EnumOutputs(0, &output))) {
                output->Release();
                chosen = candidate;
                wprintf(L"adapter %u has the output\n", index);
            } else {
                candidate->Release();
            }
        }
        if (enumerating != nullptr) enumerating->Release();
    }
    HRESULT made = D3D11CreateDevice(chosen, chosen != nullptr ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                                     nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context);
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
    described.BufferCount = blt ? 1 : 2;
    described.Scaling = DXGI_SCALING_STRETCH;
    described.SwapEffect = blt ? DXGI_SWAP_EFFECT_DISCARD : DXGI_SWAP_EFFECT_FLIP_DISCARD;
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
        if (FAILED(chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&back))) || back == nullptr) {
            wprintf(L"no back buffer, device removed 0x%08lx\n",
                    static_cast<unsigned long>(device->GetDeviceRemovedReason()));
            return 4;
        }
        if (shared) {
            const float green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
            context->ClearRenderTargetView(slot_view, green);
            context->CopyResource(back, slot);
        } else {
            ID3D11RenderTargetView *view = nullptr;
            const HRESULT viewed = device->CreateRenderTargetView(back, nullptr, &view);
            if (FAILED(viewed) || view == nullptr) {
                wprintf(L"no view 0x%08lx after %d, device removed 0x%08lx\n", static_cast<unsigned long>(viewed),
                        frames, static_cast<unsigned long>(device->GetDeviceRemovedReason()));
                return 6;
            }
            const float blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
            context->ClearRenderTargetView(view, blue);
            view->Release();
        }
        back->Release();
        const HRESULT presented = chain->Present(1, 0);
        if (FAILED(presented)) {
            wprintf(L"present 0x%08lx after %d, device removed 0x%08lx\n", static_cast<unsigned long>(presented), frames,
                    static_cast<unsigned long>(device->GetDeviceRemovedReason()));
            return 5;
        }
        frames++;
    }
    wprintf(L"presents %d\n", frames);
    return 0;
}
