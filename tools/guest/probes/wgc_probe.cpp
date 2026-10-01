// Windows.Graphics.Capture probe: how many frames a capture delivers, when, and
// whether they hold what was on screen.
//
//   wgc_probe.exe <mode> [--seconds S] [--render-ms M] [--swap-buffers N]
//                        [--pool-buffers N] [--out FILE] [--thumbs FILE]
//
// Modes:
//   window, monitor          the probe's own window, repainted with GDI once a
//                            DWM frame, captured as a window or as the monitor
//   window-d3d, monitor-d3d  the same window drawn by a D3D11 swapchain
//                            presenting at sync interval 1, as a game does;
//                            --swap-buffers 1 makes it a blt-model swapchain,
//                            --sync-interval 0 presents uncapped
//   title:TEXT               the first visible window whose title holds TEXT
//
// Every line is `key value...`: `frame` once per captured frame (its
// SystemRelativeTime, which is QPC in 100 ns, and when it arrived), then
// `capture`, `intervals` and `pixels` for the run, then `WGC PASS|FAIL`.
// --thumbs writes each captured frame as a 96x54 luminance thumbnail, one
// after the other: what the window showed frame by frame, to see whether it
// ever steps back to an older frame.
// tools/frame_pacing.py reads the frames to line captures up with its ETW
// trace. Each paint and each captured frame also calls D3DKMTGetScanLine, which
// dxgkrnl's profiler keyword logs with the calling thread: a marker on the
// ETW timeline.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwmapi.h>
#include <roapi.h>
#include <timeapi.h>
#include <winstring.h>
// llvm-mingw's windows.foundation.h specializes IReference for boolean and for
// BYTE, which are one type there (unsigned char); the probe uses neither.
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <vector>

using namespace ABI::Windows::Graphics::Capture;
using namespace ABI::Windows::Graphics::DirectX::Direct3D11;
namespace dx = ABI::Windows::Graphics::DirectX;
using ABI::Windows::Foundation::IClosable;
using ABI::Windows::Graphics::SizeInt32;
using DxgiAccess = ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess;

typedef UINT D3DKMT_HANDLE;
struct D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME { WCHAR DeviceName[32]; D3DKMT_HANDLE hAdapter; LUID AdapterLuid; UINT VidPnSourceId; };
struct D3DKMT_GETSCANLINE { D3DKMT_HANDLE hAdapter; UINT VidPnSourceId; BOOLEAN InVerticalBlank; UINT ScanLine; };
extern "C" {
LONG WINAPI D3DKMTOpenAdapterFromGdiDisplayName(D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME *);
LONG WINAPI D3DKMTGetScanLine(D3DKMT_GETSCANLINE *);
}

namespace {

FILE *out = stdout;
#define LOG(...) do { std::fprintf(out, __VA_ARGS__); std::fflush(out); } while (0)
#define CHECK(expr) do { HRESULT hr_ = (expr); if (FAILED(hr_)) { LOG("fail %s hr=0x%08lx\n", #expr, (unsigned long)hr_); return false; } } while (0)

template <class T> void release(T *&object)
{
    if (object) object->Release();
    object = nullptr;
}

struct Options {
    const wchar_t *mode = L"monitor-d3d";
    double seconds = 8.0;
    double render_ms = 0.0;   // busy time before each present, as a game renders
    UINT swap_buffers = 2;
    UINT sync_interval = 1;   // 0 presents uncapped, as a game with vsync off
    int pool_buffers = 3;
    double min_update_ms = -1.0;   // GraphicsCaptureSession.MinUpdateInterval; below 0, DWM's 16 ms
    const wchar_t *thumbs = nullptr;
};

// A marker on the ETW timeline: dxgkrnl's profiler keyword logs each
// DxgkGetScanLine with the thread that called it.
class Marker {
public:
    Marker()
    {
        MONITORINFOEXW info = {};
        info.cbSize = sizeof info;
        GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &info);
        D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open = {};
        wcscpy(open.DeviceName, info.szDevice);
        if (D3DKMTOpenAdapterFromGdiDisplayName(&open) == 0) {
            line_.hAdapter = open.hAdapter;
            line_.VidPnSourceId = open.VidPnSourceId;
        }
    }
    void mark() const
    {
        if (line_.hAdapter == 0) return;
        D3DKMT_GETSCANLINE line = line_;
        D3DKMTGetScanLine(&line);
    }

private:
    D3DKMT_GETSCANLINE line_ = {};
};

Marker *marker = nullptr;

// The probe's window and what animates it. (x, y) of a GDI frame holds
// R = (x + 8 * step) & 255, G = y & 255, B = 128, so a stride, scale or channel
// fault shows along a captured row; a D3D frame clears to R = 8 * step.
class ProbeWindow {
public:
    static constexpr int width = 512;
    static constexpr int height = 256;

    bool open(bool d3d, const Options &options)
    {
        d3d_ = d3d;
        options_ = options;
        self_ = this;
        CreateThread(nullptr, 0, &ProbeWindow::window_thread, this, 0, nullptr);
        for (int i = 0; i < 100 && !hwnd_; i++) Sleep(20);
        Sleep(300);
        return hwnd_ != nullptr;
    }
    void close()
    {
        animating_ = 0;
        if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }
    HWND hwnd() const { return hwnd_; }
    bool d3d() const { return d3d_; }
    LONG painted() const { return painted_; }
    // Where the client area starts inside a window capture, which includes
    // the frame DWM draws around it.
    POINT client_offset() const
    {
        RECT frame = {};
        DwmGetWindowAttribute(hwnd_, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof frame);
        POINT origin = {0, 0};
        ClientToScreen(hwnd_, &origin);
        return POINT{origin.x - frame.left, origin.y - frame.top};
    }

private:
    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_PAINT && self_ && !self_->d3d_) {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            self_->paint(dc);
            EndPaint(hwnd, &paint);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    static DWORD WINAPI window_thread(void *context)
    {
        auto *self = static_cast<ProbeWindow *>(context);
        WNDCLASSW type = {};
        type.lpfnWndProc = &ProbeWindow::procedure;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"rvkwgcprobe";
        RegisterClassW(&type);
        RECT r = {0, 0, width, height};
        AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_TOPMOST);
        self->hwnd_ = CreateWindowExW(WS_EX_TOPMOST, L"rvkwgcprobe", L"rvk wgc probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      100, 100, r.right - r.left, r.bottom - r.top, nullptr, nullptr, type.hInstance,
                                      nullptr);
        CreateThread(nullptr, 0, &ProbeWindow::animate_thread, self, 0, nullptr);
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return 0;
    }
    static DWORD WINAPI animate_thread(void *context)
    {
        auto *self = static_cast<ProbeWindow *>(context);
        return self->d3d_ ? self->animate_d3d() : self->animate_gdi();
    }
    void paint(HDC dc)
    {
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                pixels_[y * width + x] = ((unsigned)((x + 8 * step_) & 255) << 16) | ((unsigned)(y & 255) << 8) | 128u;
        BITMAPINFO bitmap = {};
        bitmap.bmiHeader.biSize = sizeof bitmap.bmiHeader;
        bitmap.bmiHeader.biWidth = width;
        bitmap.bmiHeader.biHeight = -height;
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        bitmap.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(dc, 0, 0, width, height, 0, 0, 0, height, pixels_, &bitmap, DIB_RGB_COLORS);
    }
    // One step painted, then DWM waited for: every step is on screen for
    // exactly one composition, so a step no capture shows was dropped.
    DWORD animate_gdi()
    {
        while (animating_) {
            InterlockedIncrement(&step_);
            HDC dc = GetDC(hwnd_);
            paint(dc);
            ReleaseDC(hwnd_, dc);
            marker->mark();
            InterlockedIncrement(&painted_);
            DwmFlush();
        }
        return 0;
    }
    DWORD animate_d3d()
    {
        IDXGIFactory2 *factory = nullptr;
        IDXGIAdapter1 *adapter = nullptr;
        ID3D11Device *device = nullptr;
        ID3D11DeviceContext *context = nullptr;
        IDXGISwapChain1 *chain = nullptr;
        D3D_FEATURE_LEVEL level;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void **)&factory)) ||
            factory->EnumAdapters1(0, &adapter) != S_OK ||
            FAILED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                     &device, &level, &context)))
            return 1;
        DXGI_SWAP_CHAIN_DESC1 description = {};
        description.Width = width;
        description.Height = height;
        description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.BufferCount = options_.swap_buffers;
        description.SwapEffect = options_.swap_buffers == 1 ? DXGI_SWAP_EFFECT_DISCARD : DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (FAILED(factory->CreateSwapChainForHwnd(device, hwnd_, &description, nullptr, nullptr, &chain))) return 1;
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        while (animating_) {
            InterlockedIncrement(&step_);
            ID3D11Texture2D *buffer = nullptr;
            ID3D11RenderTargetView *view = nullptr;
            chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&buffer);
            device->CreateRenderTargetView(buffer, nullptr, &view);
            float colour[4] = {((8 * step_) & 255) / 255.0f, 0.25f, 0.5f, 1};
            context->ClearRenderTargetView(view, colour);
            release(view);
            release(buffer);
            context->Flush();
            if (options_.render_ms > 0) {
                LARGE_INTEGER start, now;
                QueryPerformanceCounter(&start);
                do QueryPerformanceCounter(&now);
                while (1000.0 * (now.QuadPart - start.QuadPart) / frequency.QuadPart < options_.render_ms);
            }
            chain->Present(options_.sync_interval, 0);
            marker->mark();
            InterlockedIncrement(&painted_);
        }
        release(chain);
        release(context);
        release(device);
        release(adapter);
        release(factory);
        return 0;
    }

    static ProbeWindow *self_;
    HWND hwnd_ = nullptr;
    bool d3d_ = false;
    Options options_;
    volatile LONG step_ = 0;
    volatile LONG painted_ = 0;
    volatile LONG animating_ = 1;
    unsigned pixels_[width * height] = {};
};

ProbeWindow *ProbeWindow::self_ = nullptr;

template <class T> HRESULT activation_factory(const wchar_t *runtime_class, T **factory)
{
    HSTRING name = nullptr;
    WindowsCreateString(runtime_class, (UINT32)wcslen(runtime_class), &name);
    HRESULT hr = RoGetActivationFactory(name, __uuidof(T), (void **)factory);
    WindowsDeleteString(name);
    return hr;
}

// The texture behind a captured frame, and a CPU copy of all or part of it.
class FrameReader {
public:
    FrameReader(ID3D11Device *device, ID3D11DeviceContext *context) : device_(device), context_(context) {}
    ~FrameReader()
    {
        release(row_);
        release(whole_);
    }

    static constexpr unsigned thumb_width = 96, thumb_height = 54;

    // The frame's luminance on a 96x54 grid, into `thumb`; false when it
    // cannot be read.
    bool thumbnail(IDirect3D11CaptureFrame *frame, unsigned char *thumb)
    {
        ID3D11Texture2D *texture = texture_of(frame);
        if (!texture) return false;
        D3D11_TEXTURE2D_DESC description;
        texture->GetDesc(&description);
        if (whole_) {
            D3D11_TEXTURE2D_DESC held;
            whole_->GetDesc(&held);
            if (held.Width != description.Width || held.Height != description.Height) release(whole_);
        }
        if (!whole_) {
            D3D11_TEXTURE2D_DESC staging = description;
            staging.Usage = D3D11_USAGE_STAGING;
            staging.BindFlags = 0;
            staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            staging.MiscFlags = 0;
            device_->CreateTexture2D(&staging, nullptr, &whole_);
        }
        bool read = false;
        D3D11_MAPPED_SUBRESOURCE map;
        if (whole_) {
            context_->CopyResource(whole_, texture);
            if (SUCCEEDED(context_->Map(whole_, 0, D3D11_MAP_READ, 0, &map))) {
                const auto *bytes = static_cast<const unsigned char *>(map.pData);
                for (unsigned ty = 0; ty < thumb_height; ty++)
                    for (unsigned tx = 0; tx < thumb_width; tx++) {
                        const unsigned x = (2 * tx + 1) * description.Width / (2 * thumb_width);
                        const unsigned y = (2 * ty + 1) * description.Height / (2 * thumb_height);
                        const unsigned char *p = bytes + (size_t)y * map.RowPitch + (size_t)x * 4;
                        thumb[ty * thumb_width + tx] = (unsigned char)((p[0] + 2 * p[1] + p[2]) / 4);
                    }
                context_->Unmap(whole_, 0);
                read = true;
            }
        }
        release(texture);
        return read;
    }

    static ID3D11Texture2D *texture_of(IDirect3D11CaptureFrame *frame)
    {
        IDirect3DSurface *surface = nullptr;
        DxgiAccess *access = nullptr;
        ID3D11Texture2D *texture = nullptr;
        if (SUCCEEDED(frame->get_Surface(&surface)) &&
            SUCCEEDED(surface->QueryInterface(__uuidof(DxgiAccess), (void **)&access)))
            access->GetInterface(__uuidof(ID3D11Texture2D), (void **)&texture);
        release(access);
        release(surface);
        return texture;
    }

    // The animation step a window capture shows: R at the client's first
    // column is 8 * step. -1 when the frame cannot be read.
    int step(IDirect3D11CaptureFrame *frame, POINT client)
    {
        ID3D11Texture2D *texture = texture_of(frame);
        if (!texture) return -1;
        D3D11_TEXTURE2D_DESC description;
        texture->GetDesc(&description);
        if (!row_) {
            D3D11_TEXTURE2D_DESC staging = description;
            staging.Width = 4;
            staging.Height = 1;
            staging.Usage = D3D11_USAGE_STAGING;
            staging.BindFlags = 0;
            staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            staging.MiscFlags = 0;
            device_->CreateTexture2D(&staging, nullptr, &row_);
        }
        int seen = -1;
        D3D11_BOX box = {(UINT)client.x, (UINT)client.y + 100, 0, (UINT)client.x + 4, (UINT)client.y + 101, 1};
        if (row_ && box.right <= description.Width && box.bottom <= description.Height) {
            context_->CopySubresourceRegion(row_, 0, 0, 0, 0, texture, 0, &box);
            D3D11_MAPPED_SUBRESOURCE map;
            if (SUCCEEDED(context_->Map(row_, 0, D3D11_MAP_READ, 0, &map))) {
                seen = static_cast<const unsigned char *>(map.pData)[2] / 8;
                context_->Unmap(row_, 0);
            }
        }
        release(texture);
        return seen;
    }

    // Whether the frame holds the window's gradient (a GDI window capture) or
    // is mostly not black (anything else).
    bool holds_content(IDirect3D11CaptureFrame *frame, const ProbeWindow *window)
    {
        ID3D11Texture2D *texture = texture_of(frame);
        if (!texture) return false;
        D3D11_TEXTURE2D_DESC description;
        texture->GetDesc(&description);
        D3D11_TEXTURE2D_DESC staging = description;
        staging.Usage = D3D11_USAGE_STAGING;
        staging.BindFlags = 0;
        staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        staging.MiscFlags = 0;
        ID3D11Texture2D *copy = nullptr;
        bool held = false;
        D3D11_MAPPED_SUBRESOURCE map;
        if (SUCCEEDED(device_->CreateTexture2D(&staging, nullptr, &copy))) {
            context_->CopyResource(copy, texture);
            if (SUCCEEDED(context_->Map(copy, 0, D3D11_MAP_READ, 0, &map))) {
                const auto *bytes = static_cast<const unsigned char *>(map.pData);
                auto at = [&](unsigned x, unsigned y) { return bytes + (size_t)y * map.RowPitch + (size_t)x * 4; };
                if (window && !window->d3d()) {
                    POINT client = window->client_offset();
                    unsigned y = 100, bad = 0;
                    for (unsigned x = 0; x + 1 < (unsigned)ProbeWindow::width && client.x + x + 1 < description.Width; x++) {
                        const unsigned char *p = at(client.x + x, client.y + y), *q = at(client.x + x + 1, client.y + y);
                        if (p[1] != y || p[0] != 128 || (unsigned char)(q[2] - p[2]) != 1) bad++;
                    }
                    held = bad == 0;
                } else {
                    unsigned lit = 0, samples = 0;
                    for (unsigned y = 0; y < description.Height; y += 37)
                        for (unsigned x = 0; x < description.Width; x += 37, samples++)
                            if (at(x, y)[0] | at(x, y)[1] | at(x, y)[2]) lit++;
                    held = lit * 10 > samples;
                }
                context_->Unmap(copy, 0);
            }
        }
        release(copy);
        release(texture);
        return held;
    }

private:
    ID3D11Device *device_;
    ID3D11DeviceContext *context_;
    ID3D11Texture2D *row_ = nullptr;
    ID3D11Texture2D *whole_ = nullptr;
};

struct Device {
    ID3D11Device *d3d = nullptr;
    ID3D11DeviceContext *context = nullptr;
    IDirect3DDevice *winrt = nullptr;

    bool open(IDXGIAdapter1 *adapter)
    {
        D3D_FEATURE_LEVEL level;
        CHECK(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                D3D11_SDK_VERSION, &d3d, &level, &context));
        IDXGIDevice *dxgi = nullptr;
        IInspectable *inspectable = nullptr;
        CHECK(d3d->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi));
        CHECK(CreateDirect3D11DeviceFromDXGIDevice(dxgi, &inspectable));
        CHECK(inspectable->QueryInterface(__uuidof(IDirect3DDevice), (void **)&winrt));
        release(inspectable);
        release(dxgi);
        return true;
    }
    ~Device()
    {
        release(winrt);
        release(context);
        release(d3d);
    }
};

double refresh_period_ms()
{
    DWM_TIMING_INFO timing = {};
    timing.cbSize = sizeof timing;
    if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &timing)) && timing.rateRefresh.uiNumerator)
        return 1000.0 * timing.rateRefresh.uiDenominator / timing.rateRefresh.uiNumerator;
    return 1000.0 / 60.0;
}

// Captures `item` for the run's seconds, polling the pool every millisecond
// as a capture application's frame loop would, and reports what arrived.
bool capture(IGraphicsCaptureItem *item, Device &device, const Options &options, const ProbeWindow *window,
             bool track_steps)
{
    SizeInt32 size;
    CHECK(item->get_Size(&size));
    IDirect3D11CaptureFramePoolStatics2 *pools = nullptr;
    IDirect3D11CaptureFramePool *pool = nullptr;
    IGraphicsCaptureSession *session = nullptr;
    CHECK(activation_factory(RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool, &pools));
    CHECK(pools->CreateFreeThreaded(device.winrt, dx::DirectXPixelFormat_B8G8R8A8UIntNormalized, options.pool_buffers,
                                    size, &pool));
    CHECK(pool->CreateCaptureSession(item, &session));
    if (options.min_update_ms >= 0) {
        // IGraphicsCaptureSession5 (Windows 11 24H2), which the MinGW headers do not declare yet.
        struct Session5 : IInspectable {
            virtual HRESULT STDMETHODCALLTYPE get_MinUpdateInterval(INT64 *hundred_ns) = 0;
            virtual HRESULT STDMETHODCALLTYPE put_MinUpdateInterval(INT64 hundred_ns) = 0;
        };
        const IID session5 = {0x67c0ea62, 0x1f85, 0x5061, {0x92, 0x5a, 0x23, 0x9b, 0xe0, 0xac, 0x09, 0xcb}};
        Session5 *interval = nullptr;
        HRESULT set = session->QueryInterface(session5, (void **)&interval);
        if (SUCCEEDED(set)) set = interval->put_MinUpdateInterval((INT64)(options.min_update_ms * 10000.0));
        release(interval);
        LOG("min_update_ms %.3f hr 0x%08lx\n", options.min_update_ms, (unsigned long)set);
    }
    LOG("item width %d height %d pool_buffers %d thread %lu process %lu\n", size.Width, size.Height,
        options.pool_buffers, GetCurrentThreadId(), GetCurrentProcessId());
    FrameReader reader(device.d3d, device.context);
    FILE *thumbs = options.thumbs ? _wfopen(options.thumbs, L"wb") : nullptr;
    const double period = refresh_period_ms();
    LARGE_INTEGER frequency, start, now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    CHECK(session->StartCapture());
    const LONG painted_at_start = window ? window->painted() : 0;
    std::vector<double> intervals;
    long long last = 0;
    unsigned frames = 0, checked = 0, held = 0, next_step = 0, repeated = 0, skipped = 0;
    int last_step = -1;
    for (;;) {
        QueryPerformanceCounter(&now);
        const double ms = 1000.0 * (now.QuadPart - start.QuadPart) / frequency.QuadPart;
        if (ms > options.seconds * 1000) break;
        IDirect3D11CaptureFrame *frame = nullptr;
        HRESULT hr = pool->TryGetNextFrame(&frame);
        if (FAILED(hr)) {
            LOG("fail TryGetNextFrame hr=0x%08lx\n", (unsigned long)hr);
            break;
        }
        if (!frame) {
            Sleep(1);
            continue;
        }
        marker->mark();
        frames++;
        ABI::Windows::Foundation::TimeSpan when;
        frame->get_SystemRelativeTime(&when);
        LOG("frame %lld %.3f\n", (long long)when.Duration, ms);
        if (last) intervals.push_back((when.Duration - last) / 10000.0);
        last = when.Duration;
        if (track_steps) {
            const int step = reader.step(frame, window->client_offset());
            if (step >= 0 && last_step >= 0) {
                const int moved = (step - last_step + 32) % 32;
                if (moved == 0) repeated++;
                else if (moved == 1) next_step++;
                else skipped += moved - 1;
            }
            if (step >= 0) last_step = step;
        }
        if (thumbs) {
            unsigned char thumb[FrameReader::thumb_width * FrameReader::thumb_height];
            if (reader.thumbnail(frame, thumb)) std::fwrite(thumb, sizeof thumb, 1, thumbs);
        }
        if (checked < 3) {
            checked++;
            held += reader.holds_content(frame, window) ? 1 : 0;
        }
        IClosable *closable = nullptr;
        if (SUCCEEDED(frame->QueryInterface(__uuidof(IClosable), (void **)&closable))) {
            closable->Close();
            release(closable);
        }
        release(frame);
    }
    std::sort(intervals.begin(), intervals.end());
    double sum = 0;
    unsigned late = 0;
    for (double interval : intervals) {
        sum += interval;
        late += interval > 1.5 * period ? 1 : 0;
    }
    LOG("capture frames %u seconds %.3f rate %.2f period_ms %.3f\n", frames, options.seconds, frames / options.seconds,
        period);
    if (!intervals.empty())
        LOG("intervals count %zu mean %.3f median %.3f p95 %.3f max %.3f late %u\n", intervals.size(),
            sum / intervals.size(), intervals[intervals.size() / 2], intervals[intervals.size() * 95 / 100],
            intervals.back(), late);
    if (track_steps)
        LOG("steps painted %ld next %u repeated %u never_captured %u\n", window->painted() - painted_at_start, next_step,
            repeated, skipped);
    else if (window)
        LOG("presents %ld rate %.1f\n", window->painted() - painted_at_start,
            (window->painted() - painted_at_start) / options.seconds);
    LOG("pixels checked %u held %u\n", checked, held);
    if (thumbs) std::fclose(thumbs);
    IClosable *closable = nullptr;
    if (SUCCEEDED(session->QueryInterface(__uuidof(IClosable), (void **)&closable))) closable->Close();
    release(closable);
    if (SUCCEEDED(pool->QueryInterface(__uuidof(IClosable), (void **)&closable))) closable->Close();
    release(closable);
    release(session);
    release(pool);
    release(pools);
    return frames > 0 && checked > 0 && held == checked;
}

HWND titled_window = nullptr;
const wchar_t *wanted_title = nullptr;
BOOL CALLBACK find_titled(HWND hwnd, LPARAM)
{
    wchar_t title[256] = {};
    GetWindowTextW(hwnd, title, 256);
    if (IsWindowVisible(hwnd) && wcsstr(title, wanted_title)) {
        titled_window = hwnd;
        return FALSE;
    }
    return TRUE;
}

bool run(const Options &options)
{
    CHECK(RoInitialize(RO_INIT_MULTITHREADED));
    timeBeginPeriod(1);
    IGraphicsCaptureSessionStatics *statics = nullptr;
    boolean supported = false;
    CHECK(activation_factory(RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureSession, &statics));
    CHECK(statics->IsSupported(&supported));
    release(statics);
    if (!supported) {
        LOG("fail GraphicsCaptureSession.IsSupported is false\n");
        return false;
    }
    IGraphicsCaptureItemInterop *interop = nullptr;
    IDXGIFactory1 *dxgi = nullptr;
    IDXGIAdapter1 *adapter = nullptr;
    CHECK(activation_factory(RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem, &interop));
    CHECK(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&dxgi));
    CHECK(dxgi->EnumAdapters1(0, &adapter));
    DXGI_ADAPTER_DESC1 described;
    adapter->GetDesc1(&described);
    DEVMODEW mode = {};
    mode.dmSize = sizeof mode;
    EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode);
    LOG("adapter '%ls' vendor %04x device %04x display %lux%lu at %lu Hz\n", described.Description, described.VendorId,
        described.DeviceId, mode.dmPelsWidth, mode.dmPelsHeight, mode.dmDisplayFrequency);
    Device device;
    if (!device.open(adapter)) return false;

    const wchar_t *target = options.mode;
    IGraphicsCaptureItem *item = nullptr;
    ProbeWindow *window = nullptr;
    static ProbeWindow probe;
    bool track_steps = false;
    if (!wcsncmp(target, L"title:", 6)) {
        wanted_title = target + 6;
        EnumWindows(find_titled, 0);
        if (!titled_window) {
            LOG("fail no visible window titled *%ls*\n", wanted_title);
            return false;
        }
        CHECK(interop->CreateForWindow(titled_window, __uuidof(IGraphicsCaptureItem), (void **)&item));
    } else {
        const bool d3d = wcsstr(target, L"-d3d") != nullptr;
        const bool monitor = !wcsncmp(target, L"monitor", 7);
        if (!monitor && wcsncmp(target, L"window", 6)) {
            LOG("fail unknown mode %ls\n", target);
            return false;
        }
        if (!probe.open(d3d, options)) {
            LOG("fail the probe window did not open\n");
            return false;
        }
        window = &probe;
        track_steps = !monitor;
        if (monitor) {
            HMONITOR primary = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
            CHECK(interop->CreateForMonitor(primary, __uuidof(IGraphicsCaptureItem), (void **)&item));
        } else {
            CHECK(interop->CreateForWindow(probe.hwnd(), __uuidof(IGraphicsCaptureItem), (void **)&item));
        }
    }
    LOG("mode %ls seconds %.3f render_ms %.3f swap_buffers %u sync_interval %u\n", target, options.seconds,
        options.render_ms, options.swap_buffers, options.sync_interval);
    const bool passed = capture(item, device, options, window, track_steps);
    release(item);
    if (window) window->close();
    release(adapter);
    release(dxgi);
    release(interop);
    return passed;
}

bool parse(int argc, wchar_t **argv, Options &options, const wchar_t *&path)
{
    for (int i = 1; i < argc; i++) {
        const wchar_t *argument = argv[i];
        const bool valued = i + 1 < argc;
        if (!wcscmp(argument, L"--seconds") && valued) options.seconds = _wtof(argv[++i]);
        else if (!wcscmp(argument, L"--render-ms") && valued) options.render_ms = _wtof(argv[++i]);
        else if (!wcscmp(argument, L"--swap-buffers") && valued) options.swap_buffers = (UINT)_wtoi(argv[++i]);
        else if (!wcscmp(argument, L"--pool-buffers") && valued) options.pool_buffers = _wtoi(argv[++i]);
        else if (!wcscmp(argument, L"--sync-interval") && valued) options.sync_interval = (UINT)_wtoi(argv[++i]);
        else if (!wcscmp(argument, L"--min-update-ms") && valued) options.min_update_ms = _wtof(argv[++i]);
        else if (!wcscmp(argument, L"--out") && valued) path = argv[++i];
        else if (!wcscmp(argument, L"--thumbs") && valued) options.thumbs = argv[++i];
        else if (argument[0] != L'-') options.mode = argument;
        else return false;
    }
    return options.seconds > 0 && options.swap_buffers >= 1 && options.pool_buffers >= 1;
}

}  // namespace

int wmain(int argc, wchar_t **argv)
{
    Options options;
    const wchar_t *path = nullptr;
    if (!parse(argc, argv, options, path)) {
        std::fprintf(stderr, "usage: wgc_probe <window|monitor|window-d3d|monitor-d3d|title:TEXT> [--seconds S] "
                             "[--render-ms M] [--swap-buffers N] [--pool-buffers N] [--sync-interval N] [--min-update-ms M] [--out FILE] "
                             "[--thumbs FILE]\n");
        return 2;
    }
    if (path) {
        FILE *file = _wfopen(path, L"w");
        if (file) out = file;
    }
    Marker markers;
    marker = &markers;
    LARGE_INTEGER frequency, now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    LOG("qpc now %lld frequency %lld\n", (long long)now.QuadPart, (long long)frequency.QuadPart);
    const bool passed = run(options);
    LOG("WGC %s\n", passed ? "PASS" : "FAIL");
    if (out != stdout) std::fclose(out);
    return passed ? 0 : 1;
}
