// d3d9_probe.exe: a Direct3D 9 device made the way a Source engine game makes
// one, on whatever d3d9.dll routes the adapter to (D3D9On12, on an adapter
// whose UserModeDriverName names no Direct3D 9 driver), then frames cleared
// to a known colour and presented.
//
// D3D9On12 answers a failure inside device creation with the bare HRESULT: it
// throws a _com_error at the call that failed and catches it at the DDI. A
// vectored handler sees that throw first, and writes its HRESULT and the
// return addresses under it as module+offset, which the module's public PDB
// names.
//
//   d3d9_probe.exe [--fullscreen] [--frames N] [--width W --height H]
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

namespace {

constexpr DWORD msvc_cxx_exception = 0xE06D7363u;
constexpr ULONG frames_captured = 24u;

void write_frame(void *address)
{
    HMODULE module = nullptr;
    wchar_t name[MAX_PATH] = L"?";
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCWSTR>(address), &module)) {
        GetModuleFileNameW(module, name, MAX_PATH);
        const wchar_t *slash = wcsrchr(name, L'\\');
        wprintf(L"  %ls+0x%lx\n", slash != nullptr ? slash + 1 : name,
                static_cast<unsigned long>(reinterpret_cast<ULONG_PTR>(address) - reinterpret_cast<ULONG_PTR>(module)));
        return;
    }
    wprintf(L"  %p\n", address);
}

LONG CALLBACK first_chance(EXCEPTION_POINTERS *pointers)
{
    const EXCEPTION_RECORD &record = *pointers->ExceptionRecord;
    if (record.ExceptionCode != msvc_cxx_exception) {
        if (record.ExceptionCode >= 0xC0000000u)
            wprintf(L"exception 0x%08lx at %p\n", record.ExceptionCode, record.ExceptionAddress);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    // _CxxThrowException's parameters: magic, the thrown object, its throw
    // info. A _com_error is a vtable pointer and then the HRESULT.
    long hresult = 0;
    if (record.NumberParameters >= 2u && record.ExceptionInformation[1] != 0u)
        hresult = reinterpret_cast<const long *>(record.ExceptionInformation[1])[1];
    wprintf(L"C++ throw, HRESULT 0x%08lx (if a _com_error)\n", static_cast<unsigned long>(hresult));
    void *frames[frames_captured] = {};
    const USHORT captured = RtlCaptureStackBackTrace(0, frames_captured, frames, nullptr);
    for (USHORT index = 0; index < captured; index++) write_frame(frames[index]);
    fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void pump()
{
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

}  // namespace

int wmain(int argc, wchar_t **argv)
{
    bool fullscreen = false;
    UINT frames = 120u, width = 1280u, height = 720u;
    for (int index = 1; index < argc; index++) {
        if (wcscmp(argv[index], L"--fullscreen") == 0) fullscreen = true;
        else if (wcscmp(argv[index], L"--frames") == 0 && index + 1 < argc) frames = wcstoul(argv[++index], nullptr, 10);
        else if (wcscmp(argv[index], L"--width") == 0 && index + 1 < argc) width = wcstoul(argv[++index], nullptr, 10);
        else if (wcscmp(argv[index], L"--height") == 0 && index + 1 < argc) height = wcstoul(argv[++index], nullptr, 10);
    }
    AddVectoredExceptionHandler(1, first_chance);

    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = L"d3d9_probe";
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&window_class);
    RECT rectangle = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowW(L"d3d9_probe", L"d3d9_probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100,
                                rectangle.right - rectangle.left, rectangle.bottom - rectangle.top, nullptr, nullptr,
                                window_class.hInstance, nullptr);
    pump();

    IDirect3D9 *direct3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (direct3d == nullptr) {
        wprintf(L"Direct3DCreate9 failed\n");
        return 1;
    }
    const UINT adapters = direct3d->GetAdapterCount();
    wprintf(L"adapters %u\n", adapters);
    for (UINT adapter = 0; adapter < adapters; adapter++) {
        D3DADAPTER_IDENTIFIER9 identifier = {};
        const HRESULT named = direct3d->GetAdapterIdentifier(adapter, 0, &identifier);
        D3DDISPLAYMODE mode = {};
        direct3d->GetAdapterDisplayMode(adapter, &mode);
        wprintf(L"adapter %u: 0x%08lx %hs [%hs] %04lx:%04lx mode %ux%u@%u fmt %u\n", adapter,
                static_cast<unsigned long>(named), identifier.Description, identifier.Driver, identifier.VendorId,
                identifier.DeviceId, mode.Width, mode.Height, mode.RefreshRate, static_cast<unsigned>(mode.Format));
    }
    D3DCAPS9 caps = {};
    const HRESULT capped = direct3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
    wprintf(L"GetDeviceCaps 0x%08lx vs %lx ps %lx\n", static_cast<unsigned long>(capped),
            static_cast<unsigned long>(caps.VertexShaderVersion), static_cast<unsigned long>(caps.PixelShaderVersion));
    fflush(stdout);

    D3DPRESENT_PARAMETERS present = {};
    present.BackBufferWidth = width;
    present.BackBufferHeight = height;
    present.BackBufferFormat = D3DFMT_X8R8G8B8;
    present.BackBufferCount = 1;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.Windowed = fullscreen ? FALSE : TRUE;
    present.EnableAutoDepthStencil = TRUE;
    present.AutoDepthStencilFormat = D3DFMT_D24S8;
    present.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    if (fullscreen) present.FullScreen_RefreshRateInHz = 60;
    IDirect3DDevice9 *device = nullptr;
    const HRESULT made = direct3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                                D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &present,
                                                &device);
    wprintf(L"CreateDevice 0x%08lx\n", static_cast<unsigned long>(made));
    fflush(stdout);
    if (FAILED(made)) {
        direct3d->Release();
        return 2;
    }
    const D3DCOLOR colours[] = {D3DCOLOR_XRGB(200, 40, 40), D3DCOLOR_XRGB(40, 200, 40), D3DCOLOR_XRGB(40, 40, 200)};
    HRESULT shown = S_OK;
    for (UINT frame = 0; frame < frames; frame++) {
        pump();
        device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, colours[(frame / 60u) % 3u], 1.0f, 0);
        device->BeginScene();
        device->EndScene();
        shown = device->Present(nullptr, nullptr, nullptr, nullptr);
        if (FAILED(shown)) {
            wprintf(L"Present 0x%08lx at frame %u\n", static_cast<unsigned long>(shown), frame);
            break;
        }
    }
    wprintf(L"presented; last 0x%08lx\n", static_cast<unsigned long>(shown));
    device->Release();
    direct3d->Release();
    DestroyWindow(window);
    return 0;
}
