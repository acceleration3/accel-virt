// d3d9_dirty_present.exe: a windowed Direct3D 9 swap chain presented the way
// Affinity's canvas and Half-Life 2's window are (docs/testing.md, "Probes
// from an application's calls"), and every frame it shows read back off the
// desktop and judged against the frame that was presented.
//
// Affinity (traced at its DDI: avumd-calls, Oct 2026) keeps one X8R8G8B8 back
// buffer it never locks, under D3DSWAPEFFECT_COPY, and redraws only what
// changed: a Clear of one rectangle, a few draws under small viewports, and a
// Present every few milliseconds whose dirty region is those rectangles. What
// it shows is therefore the back buffer as every present before left it. The
// `partial` mode does the same with a "handle" that moves across the window
// each frame, as a dragged gradient's does, and a bar code of the frame's
// number in the corner. The `full` mode clears and redraws the whole frame
// under D3DSWAPEFFECT_DISCARD, as a game does.
//
// What the window shows is read three ways, each a step further along the
// path to the user's eye:
//   - through DWM on request (PrintWindow with PW_RENDERFULLCONTENT), the
//     default: the window as DWM would render it now;
//   - off the screen (--screen: a BitBlt from the desktop's DC, which has DWM
//     compose that region): the composed desktop;
//   - as the compositor composes it, frame by frame (--wgc: a
//     Windows.Graphics.Capture of the window on a thread of its own): the
//     frames a window mirror and the scanout take, at DWM's own pace, with
//     nothing waiting for anything.
// Each picture's bar code is decoded into the frame number shown, and the
// probe fails when
//   - the number shown goes backwards: a frame presented earlier shows after a
//     later one did ("past frames");
//   - the frame shown holds a handle other than its own: the handle of an
//     earlier frame is still there, or the present mixed two frames ("past
//     rectangles").
// The lag between the frame presented and the frame shown is reported, not
// judged: composition is allowed to be behind, never to go back.
//
//   d3d9_dirty_present.exe [partial|full] [--ex] [--frames N] [--interval-ms N]
//                          [--width W --height H] [--source-rect] [--settle-ms N]
//                          [--screen] [--wgc] [--read-every N] [--rect-order N]
//                          [--settle-capture-ms N] [--no-read] [--verbose]
//
// --ex           makes the device through Direct3D9Ex, as a few applications do.
// --source-rect  presents the dirty rectangles' bound as pSourceRect/pDestRect
//                rather than as a dirty region.
// --rect-order N the order of the three dirty rectangles - the old handle's,
//                the new handle's and the bar code's: 0 old,new,code (the
//                default); 1 new,old,code; 2 code,old,new; 3 code,new,old.
// --read-every N reads after every Nth frame only, so presents outrun the
//                reads between, as an application's do.
// --settle-capture-ms N
//                waits N ms after a compositor frame arrives before reading it,
//                and reads it twice: a frame that reads differently the second
//                time was still being written when it arrived - a tear of the
//                capture's own, not of the window.
// --no-read      only presents, for a trace of the calls alone.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwmapi.h>
#include <roapi.h>
#include <winstring.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
// llvm-mingw's windows.foundation.h specializes IReference for boolean and for
// BYTE, which are one type there (unsigned char); the probe uses neither.
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace {

namespace wgc = ABI::Windows::Graphics::Capture;
namespace wgd3d = ABI::Windows::Graphics::DirectX::Direct3D11;
using DxgiAccess = ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess;

template <typename T> T lesser(T a, T b) { return a < b ? a : b; }
template <typename T> T greater(T a, T b) { return a > b ? a : b; }
template <class T> void release(T *&object)
{
    if (object != nullptr) object->Release();
    object = nullptr;
}

enum class Mode { partial, full };

struct Options {
    Mode     mode = Mode::partial;
    bool     ex = false;
    bool     source_rect = false;
    bool     read = true;
    bool     screen = false;
    bool     wgc = false;
    bool     verbose = false;
    uint32_t read_every = 1u;
    uint32_t rect_order = 0u;
    uint32_t settle_capture_ms = 0u;
    uint32_t frames = 600u;
    uint32_t interval_ms = 4u;
    uint32_t settle_ms = 400u;
    uint32_t width = 1280u;
    uint32_t height = 800u;
};

/* The picture: a grey background, a handle that walks a diagonal as a drag
 * does, and a bar code of the frame number: `code_bits` cells of `cell` pixels
 * along the top-left edge, a white guard cell at either end. */
constexpr uint32_t handle_size = 24u;
constexpr uint32_t cell = 8u;
constexpr uint32_t code_bits = 16u;
constexpr uint32_t code_cells = code_bits + 2u;
constexpr int32_t  code_x = 8, code_y = 8;
/* A second copy of the bar code along the bottom edge: a frame whose two
 * codes differ was composed from a surface only partly written. */
int32_t bottom_code_y(const Options &options) { return static_cast<int32_t>(options.height) - 8 - static_cast<int32_t>(cell); }
constexpr D3DCOLOR background = D3DCOLOR_XRGB(96, 96, 96);
constexpr D3DCOLOR handle_colour = D3DCOLOR_XRGB(255, 160, 0);
constexpr D3DCOLOR full_background = D3DCOLOR_XRGB(20, 40, 90);

struct Vertex {
    float x, y, z, rhw;
    DWORD colour;
};

RECT handle_rect(const Options &options, uint32_t frame)
{
    /* Back and forth across the window, never under the bar code, a new
     * place every frame so no two nearby frames share one. */
    const uint32_t span_x = options.width - handle_size - 40u;
    const uint32_t span_y = options.height - handle_size - 80u;   /* clear of the bottom bar code */
    const uint32_t step = 7u;
    const uint32_t tx = (frame * step) % (2u * span_x);
    const uint32_t ty = (frame * (step + 4u)) % (2u * span_y);
    const int32_t  x = 20 + static_cast<int32_t>(tx < span_x ? tx : 2u * span_x - tx);
    const int32_t  y = 50 + static_cast<int32_t>(ty < span_y ? ty : 2u * span_y - ty);
    return RECT{x, y, x + static_cast<int32_t>(handle_size), y + static_cast<int32_t>(handle_size)};
}

RECT code_rect_at(int32_t y)
{
    return RECT{code_x, y, code_x + static_cast<int32_t>(code_cells * cell), y + static_cast<int32_t>(cell)};
}

bool overlap(const RECT &a, const RECT &b)
{
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

RECT bound(const RECT *rectangles, uint32_t count)
{
    RECT all = rectangles[0];
    for (uint32_t index = 1; index < count; index++) {
        all.left = lesser(all.left, rectangles[index].left);
        all.top = lesser(all.top, rectangles[index].top);
        all.right = greater(all.right, rectangles[index].right);
        all.bottom = greater(all.bottom, rectangles[index].bottom);
    }
    return all;
}

/* A dirty region of up to four rectangles as RGNDATA lays it out. */
struct Region {
    RGNDATAHEADER header;
    RECT          rectangles[4];
};

void make_region(Region &region, const RECT *rectangles, uint32_t count)
{
    memset(&region, 0, sizeof region);
    region.header.dwSize = sizeof region.header;
    region.header.iType = RDH_RECTANGLES;
    region.header.nCount = count;
    region.header.nRgnSize = count * sizeof(RECT);
    region.header.rcBound = bound(rectangles, count);
    for (uint32_t index = 0; index < count; index++) region.rectangles[index] = rectangles[index];
}

class Picture {
public:
    Picture(IDirect3DDevice9 *device, const Options &options) : device_(device), options_(options) {}

    void draw_code(uint32_t frame)
    {
        draw_code_at(frame, code_y);
        draw_code_at(frame, bottom_code_y(options_));
    }

    void draw_code_at(uint32_t frame, int32_t y)
    {
        D3DRECT cells[code_cells];
        for (uint32_t index = 0; index < code_cells; index++) {
            const int32_t x = code_x + static_cast<int32_t>(index * cell);
            cells[index] = D3DRECT{x, y, x + static_cast<int32_t>(cell), y + static_cast<int32_t>(cell)};
        }
        /* Guards, then the bits most significant first. */
        device_->Clear(1u, &cells[0], D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0u);
        device_->Clear(1u, &cells[code_cells - 1u], D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0u);
        for (uint32_t bit = 0; bit < code_bits; bit++) {
            const bool one = ((frame >> (code_bits - 1u - bit)) & 1u) != 0u;
            device_->Clear(1u, &cells[1u + bit], D3DCLEAR_TARGET,
                           one ? D3DCOLOR_XRGB(0, 0, 0) : D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0u);
        }
    }

    void erase(const RECT &rectangle, D3DCOLOR colour)
    {
        const D3DRECT area = {rectangle.left, rectangle.top, rectangle.right, rectangle.bottom};
        device_->Clear(1u, &area, D3DCLEAR_TARGET, colour, 1.0f, 0u);
    }

    /* The handle drawn as Affinity draws its small things: a viewport of its
     * rectangle and a pretransformed quad. */
    void draw_handle(const RECT &rectangle)
    {
        D3DVIEWPORT9 viewport = {};
        viewport.X = static_cast<DWORD>(rectangle.left);
        viewport.Y = static_cast<DWORD>(rectangle.top);
        viewport.Width = static_cast<DWORD>(rectangle.right - rectangle.left);
        viewport.Height = static_cast<DWORD>(rectangle.bottom - rectangle.top);
        viewport.MinZ = 0.0f;
        viewport.MaxZ = 1.0f;
        device_->SetViewport(&viewport);
        const float  left = static_cast<float>(rectangle.left) - 0.5f;
        const float  top = static_cast<float>(rectangle.top) - 0.5f;
        const float  right = static_cast<float>(rectangle.right) - 0.5f;
        const float  bottom = static_cast<float>(rectangle.bottom) - 0.5f;
        const Vertex quad[4] = {{left, top, 0.0f, 1.0f, handle_colour},
                                {right, top, 0.0f, 1.0f, handle_colour},
                                {left, bottom, 0.0f, 1.0f, handle_colour},
                                {right, bottom, 0.0f, 1.0f, handle_colour}};
        device_->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2u, quad, sizeof(Vertex));
        D3DVIEWPORT9 whole = {0u, 0u, options_.width, options_.height, 0.0f, 1.0f};
        device_->SetViewport(&whole);
    }

private:
    IDirect3DDevice9 *device_;
    const Options    &options_;
};

/* A picture of the window's client area, BGRX rows top-down, however it was
 * taken; `origin` is where the client's top-left is in it. */
class Pixels {
public:
    Pixels(uint32_t width, uint32_t height) : width_(width), height_(height), pixels_(new uint32_t[size_t{width} * height]) {}
    ~Pixels() { delete[] pixels_; }
    Pixels(const Pixels &) = delete;
    Pixels &operator=(const Pixels &) = delete;

    uint32_t  width() const { return width_; }
    uint32_t  height() const { return height_; }
    uint32_t *rows() { return pixels_; }
    void      place(int32_t x, int32_t y) { origin_x_ = x; origin_y_ = y; }

    uint32_t pixel(int32_t x, int32_t y) const
    {
        x += origin_x_;
        y += origin_y_;
        if (x < 0 || y < 0 || x >= static_cast<int32_t>(width_) || y >= static_cast<int32_t>(height_)) return 0xFF00FFu;
        return pixels_[static_cast<size_t>(y) * width_ + x] & 0xFFFFFFu;
    }

    /* The frame number the bar code shows, or -1 when its guards are not
     * both white (a frame mid-change, or not ours). */
    int64_t code() const { return code_at(code_y); }

    int64_t code_at(int32_t top) const
    {
        const int32_t y = top + static_cast<int32_t>(cell / 2u);
        auto cell_at = [&](uint32_t index) {
            return pixel(code_x + static_cast<int32_t>(index * cell + cell / 2u), y);
        };
        if (!white(cell_at(0u)) || !white(cell_at(code_cells - 1u))) return -1;
        uint32_t value = 0u;
        for (uint32_t bit = 0; bit < code_bits; bit++) {
            const uint32_t sample = cell_at(1u + bit);
            if (!white(sample) && !black(sample)) return -1;
            value = (value << 1u) | (black(sample) ? 1u : 0u);
        }
        return value;
    }

    /* How many of the rectangle's sampled pixels are the handle's colour. */
    uint32_t handle_pixels(const RECT &rectangle) const
    {
        uint32_t count = 0u;
        for (int32_t y = rectangle.top + 2; y < rectangle.bottom - 2; y += 4)
            for (int32_t x = rectangle.left + 2; x < rectangle.right - 2; x += 4)
                if (orange(pixel(x, y))) count++;
        return count;
    }

    static uint32_t samples(const RECT &rectangle)
    {
        uint32_t count = 0u;
        for (int32_t y = rectangle.top + 2; y < rectangle.bottom - 2; y += 4)
            for (int32_t x = rectangle.left + 2; x < rectangle.right - 2; x += 4) count++;
        return count;
    }

    bool save(const wchar_t *path) const
    {
        FILE *file = _wfopen(path, L"wb");
        if (file == nullptr) return false;
        BITMAPFILEHEADER file_header = {};
        BITMAPINFOHEADER info = {};
        const uint32_t   bytes = width_ * height_ * 4u;
        file_header.bfType = 0x4D42u;
        file_header.bfOffBits = sizeof file_header + sizeof info;
        file_header.bfSize = file_header.bfOffBits + bytes;
        info.biSize = sizeof info;
        info.biWidth = static_cast<LONG>(width_);
        info.biHeight = -static_cast<LONG>(height_);
        info.biPlanes = 1;
        info.biBitCount = 32;
        info.biCompression = BI_RGB;
        fwrite(&file_header, sizeof file_header, 1u, file);
        fwrite(&info, sizeof info, 1u, file);
        fwrite(pixels_, bytes, 1u, file);
        fclose(file);
        return true;
    }

private:
    static uint32_t channel(uint32_t bgr, uint32_t shift) { return (bgr >> shift) & 0xFFu; }
    static bool white(uint32_t bgr) { return channel(bgr, 16u) > 200u && channel(bgr, 8u) > 200u && channel(bgr, 0u) > 200u; }
    static bool black(uint32_t bgr) { return channel(bgr, 16u) < 60u && channel(bgr, 8u) < 60u && channel(bgr, 0u) < 60u; }
    static bool orange(uint32_t bgr) { return channel(bgr, 16u) > 200u && channel(bgr, 8u) > 110u && channel(bgr, 8u) < 210u && channel(bgr, 0u) < 70u; }

    uint32_t  width_, height_;
    int32_t   origin_x_ = 0, origin_y_ = 0;
    uint32_t *pixels_;
};

/* The window's client area as the desktop composes it: through DWM on
 * request (PrintWindow with PW_RENDERFULLCONTENT renders the window as it
 * shows it, whichever way its frames arrive), or off the screen. */
class WindowReader {
public:
    WindowReader(HWND window, uint32_t width, uint32_t height, bool screen)
        : window_(window), screen_(screen), pixels_(width, height)
    {
    }

    bool read()
    {
        const HDC memory_dc = CreateCompatibleDC(nullptr);
        if (memory_dc == nullptr) return false;
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof info.bmiHeader;
        info.bmiHeader.biWidth = static_cast<LONG>(pixels_.width());
        info.bmiHeader.biHeight = -static_cast<LONG>(pixels_.height());   /* top-down */
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void         *bits = nullptr;
        const HBITMAP bitmap = CreateDIBSection(memory_dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0u);
        bool          read_back = false;
        if (bitmap != nullptr) {
            const HGDIOBJ was = SelectObject(memory_dc, bitmap);
            if (screen_) {
                POINT origin = {0, 0};
                ClientToScreen(window_, &origin);
                const HDC screen_dc = GetDC(nullptr);
                read_back = screen_dc != nullptr &&
                            BitBlt(memory_dc, 0, 0, static_cast<int>(pixels_.width()), static_cast<int>(pixels_.height()),
                                   screen_dc, origin.x, origin.y, SRCCOPY) != 0;
                if (screen_dc != nullptr) ReleaseDC(nullptr, screen_dc);
            } else {
                read_back = PrintWindow(window_, memory_dc, PW_CLIENTONLY | PW_RENDERFULLCONTENT) != 0;
            }
            if (read_back) memcpy(pixels_.rows(), bits, size_t{pixels_.width()} * pixels_.height() * 4u);
            SelectObject(memory_dc, was);
            DeleteObject(bitmap);
        }
        DeleteDC(memory_dc);
        return read_back;
    }

    const Pixels &pixels() const { return pixels_; }

private:
    HWND   window_;
    bool   screen_;
    Pixels pixels_;
};

class Judge {
public:
    Judge(const Options &options, const wchar_t *who) : options_(options), who_(who) {}

    /* `shown` is what the window shows after frame `presented` was presented. */
    void judge(const Pixels &picture, int64_t shown, uint32_t presented, bool final)
    {
        reads_++;
        if (shown < 0) {
            unreadable_++;
            return;
        }
        if (shown > presented) {
            fail(L"frame %lld shown after frame %u was presented: not a frame of ours", shown, presented);
            return;
        }
        const uint32_t lag = presented - static_cast<uint32_t>(shown);
        if (lag > max_lag_) max_lag_ = lag;
        const int64_t bottom = picture.code_at(bottom_code_y(options_));
        if (bottom >= 0 && bottom != shown) {
            fail(L"torn: the top of the window shows frame %lld, the bottom frame %lld (frame %u presented)", shown, bottom,
                 presented);
            torn_++;
            save_once(picture, L"torn");
            return;
        }
        if (shown < last_shown_) {
            fail(L"the window went back from frame %lld to frame %lld (frame %u presented)", last_shown_, shown, presented);
            backwards_++;
            save_once(picture, L"went-back");
        }
        if (shown != last_shown_) distinct_++;
        last_shown_ = shown;
        if (final && shown != presented) fail(L"after settling the window shows frame %lld, not the last frame %u", shown, presented);
        judge_handles(picture, static_cast<uint32_t>(shown));
    }

    void judge_handles(const Pixels &picture, uint32_t shown)
    {
        const RECT own = handle_rect(options_, shown);
        const uint32_t own_samples = Pixels::samples(own);
        const uint32_t own_found = picture.handle_pixels(own);
        if (own_found < own_samples) {
            fail(L"frame %u shows %u of %u of its own handle's pixels", shown, own_found, own_samples);
            save_once(picture, L"own-handle");
        }
        /* The handles of the frames before: those not under this frame's must be
         * gone. The first frame of the picture has nothing before it. */
        const uint32_t back = lesser(shown, 24u);
        for (uint32_t earlier = 1u; earlier <= back; earlier++) {
            const RECT old = handle_rect(options_, shown - earlier);
            if (overlap(old, own)) continue;
            const uint32_t stale = picture.handle_pixels(old);
            if (stale != 0u) {
                fail(L"frame %u still shows the handle of frame %u (%u pixels)", shown, shown - earlier, stale);
                stale_++;
                save_once(picture, L"stale-handle");
                break;
            }
        }
    }

    void report(uint32_t presented)
    {
        if (reads_ != 0u && unreadable_ == reads_) fail(L"no read of the window showed a bar code: nothing was judged");
        wprintf(L"%ls: presented %u frames, read the window %u times (%u unreadable, %u distinct frames), shown at most "
                L"%u frames behind\n",
                who_, presented, reads_, unreadable_, distinct_, max_lag_);
        wprintf(L"%ls: went back %u times, torn %u times, stale handles %u times\n", who_, backwards_, torn_, stale_);
    }

    bool     failed() const { return failures_ != 0u; }
    uint32_t failures() const { return failures_; }
    uint32_t reads() const { return reads_; }

private:
    void fail(const wchar_t *format, ...)
    {
        failures_++;
        if (failures_ > 40u) return;   /* enough said */
        va_list arguments;
        va_start(arguments, format);
        wprintf(L"FAIL %ls: ", who_);
        vwprintf(format, arguments);
        wprintf(L"\n");
        va_end(arguments);
    }

    void save_once(const Pixels &picture, const wchar_t *what)
    {
        if (saved_ >= 3u) return;
        wchar_t path[MAX_PATH];
        swprintf(path, MAX_PATH, L"C:\\ProgramData\\av\\d3d9-dirty-present-%ls-%ls-%u.bmp", who_, what, saved_);
        if (picture.save(path)) wprintf(L"note saved %ls\n", path);
        saved_++;
    }

    const Options &options_;
    const wchar_t *who_;
    int64_t  last_shown_ = -1;
    uint32_t reads_ = 0u, unreadable_ = 0u, distinct_ = 0u, max_lag_ = 0u, backwards_ = 0u, torn_ = 0u, stale_ = 0u,
             failures_ = 0u, saved_ = 0u;
};

template <class T> HRESULT activation_factory(const wchar_t *runtime_class, T **factory)
{
    HSTRING name = nullptr;
    WindowsCreateString(runtime_class, static_cast<UINT32>(wcslen(runtime_class)), &name);
    const HRESULT made = RoGetActivationFactory(name, __uuidof(T), reinterpret_cast<void **>(factory));
    WindowsDeleteString(name);
    return made;
}

/* The window as the compositor composes it, frame by frame
 * (Windows.Graphics.Capture, as a window mirror takes it), on a thread of its
 * own: every frame copied to a staging texture, read, and judged against the
 * frame presented at the time. The capture is of the whole window, DWM's
 * frame included; the client's offset in it is DWM's extended frame bounds
 * against the client's origin. */
class CompositorWatcher {
public:
    CompositorWatcher(HWND window, const Options &options, const volatile LONG &presented)
        : window_(window), options_(options), presented_(presented), judge_(options, L"compositor")
    {
    }

    bool start()
    {
        thread_ = CreateThread(nullptr, 0, &CompositorWatcher::run, this, 0, nullptr);
        if (thread_ == nullptr) return false;
        /* Started, or failed to: either way the frame loop may begin. */
        while (!started_ && !stopped_) Sleep(5);
        return started_;
    }

    void stop()
    {
        stopping_ = 1;
        if (thread_ != nullptr) WaitForSingleObject(thread_, 5000);
        if (thread_ != nullptr) CloseHandle(thread_);
        thread_ = nullptr;
    }

    Judge &judge() { return judge_; }
    uint32_t frames() const { return frames_; }
    uint32_t rereads_differed() const { return rereads_differed_; }

private:
    static DWORD WINAPI run(void *context)
    {
        auto *self = static_cast<CompositorWatcher *>(context);
        const bool ran = self->capture();
        if (!ran) wprintf(L"compositor: the capture did not run\n");
        self->stopped_ = 1;
        return ran ? 0u : 1u;
    }

#define WATCH(expr) do { const HRESULT hr_ = (expr); if (FAILED(hr_)) { wprintf(L"compositor: %hs 0x%08lx\n", #expr, static_cast<unsigned long>(hr_)); return false; } } while (0)

    bool capture()
    {
        WATCH(RoInitialize(RO_INIT_MULTITHREADED));
        IDXGIFactory1 *dxgi = nullptr;
        IDXGIAdapter1 *adapter = nullptr;
        WATCH(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&dxgi)));
        WATCH(dxgi->EnumAdapters1(0, &adapter));
        ID3D11Device        *device = nullptr;
        ID3D11DeviceContext *context = nullptr;
        D3D_FEATURE_LEVEL    level;
        WATCH(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                D3D11_SDK_VERSION, &device, &level, &context));
        IDXGIDevice        *dxgi_device = nullptr;
        IInspectable       *inspectable = nullptr;
        wgd3d::IDirect3DDevice *winrt_device = nullptr;
        WATCH(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgi_device)));
        WATCH(CreateDirect3D11DeviceFromDXGIDevice(dxgi_device, &inspectable));
        WATCH(inspectable->QueryInterface(__uuidof(wgd3d::IDirect3DDevice), reinterpret_cast<void **>(&winrt_device)));
        release(inspectable);
        release(dxgi_device);

        ::IGraphicsCaptureItemInterop *interop = nullptr;
        wgc::IGraphicsCaptureItem        *item = nullptr;
        WATCH(activation_factory(RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem, &interop));
        WATCH(interop->CreateForWindow(window_, __uuidof(wgc::IGraphicsCaptureItem), reinterpret_cast<void **>(&item)));
        ABI::Windows::Graphics::SizeInt32 size;
        WATCH(item->get_Size(&size));
        wgc::IDirect3D11CaptureFramePoolStatics2 *pools = nullptr;
        wgc::IDirect3D11CaptureFramePool         *pool = nullptr;
        wgc::IGraphicsCaptureSession             *session = nullptr;
        WATCH(activation_factory(RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool, &pools));
        WATCH(pools->CreateFreeThreaded(winrt_device,
                                        ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized, 3,
                                        size, &pool));
        WATCH(pool->CreateCaptureSession(item, &session));
        /* Without the yellow capture border Windows 11 draws otherwise. */
        wgc::IGraphicsCaptureSession3 *session3 = nullptr;
        if (SUCCEEDED(session->QueryInterface(__uuidof(wgc::IGraphicsCaptureSession3), reinterpret_cast<void **>(&session3)))) {
            session3->put_IsBorderRequired(false);
            release(session3);
        }
        wgc::IGraphicsCaptureSession2 *session2 = nullptr;
        if (SUCCEEDED(session->QueryInterface(__uuidof(wgc::IGraphicsCaptureSession2), reinterpret_cast<void **>(&session2)))) {
            session2->put_IsCursorCaptureEnabled(false);
            release(session2);
        }
        WATCH(session->StartCapture());
        wprintf(L"compositor: capturing the window, %dx%d\n", size.Width, size.Height);

        RECT frame = {};
        DwmGetWindowAttribute(window_, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof frame);
        POINT origin = {0, 0};
        ClientToScreen(window_, &origin);
        const int32_t client_x = origin.x - frame.left, client_y = origin.y - frame.top;

        ID3D11Texture2D *staging = nullptr;
        Pixels          *picture = nullptr;
        started_ = 1;
        while (!stopping_) {
            wgc::IDirect3D11CaptureFrame *captured = nullptr;
            if (FAILED(pool->TryGetNextFrame(&captured))) break;
            if (captured == nullptr) {
                Sleep(1);
                continue;
            }
            /* Which frame had been presented when this one was composed: read
             * before the frame is, so a frame shown can never be a frame not yet
             * presented by this reckoning except by being truly ahead. */
            const uint32_t presented = static_cast<uint32_t>(presented_);
            ID3D11Texture2D *texture = texture_of(captured);
            if (texture != nullptr) {
                D3D11_TEXTURE2D_DESC description;
                texture->GetDesc(&description);
                if (picture == nullptr || picture->width() != description.Width || picture->height() != description.Height) {
                    release(staging);
                    delete picture;
                    picture = new Pixels(description.Width, description.Height);
                    picture->place(client_x, client_y);
                    D3D11_TEXTURE2D_DESC copy = description;
                    copy.Usage = D3D11_USAGE_STAGING;
                    copy.BindFlags = 0;
                    copy.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                    copy.MiscFlags = 0;
                    device->CreateTexture2D(&copy, nullptr, &staging);
                }
                if (options_.settle_capture_ms != 0u) Sleep(options_.settle_capture_ms);
                if (staging != nullptr && read_into(context, staging, texture, description, *picture)) {
                    frames_++;
                    const int64_t code = picture->code();
                    const int64_t bottom = picture->code_at(bottom_code_y(options_));
                    /* Read again: a frame that changed under the first read was
                     * still being written by the compositor when it arrived. */
                    if (options_.settle_capture_ms != 0u && read_into(context, staging, texture, description, *picture) &&
                        (picture->code() != code || picture->code_at(bottom_code_y(options_)) != bottom)) {
                        rereads_differed_++;
                        wprintf(L"compositor: a frame read %lld/%lld then %lld/%lld: written while read\n", code, bottom,
                                picture->code(), picture->code_at(bottom_code_y(options_)));
                    }
                    if (options_.verbose) wprintf(L"compositor: frame shows %lld, %u presented\n", code, presented);
                    judge_.judge(*picture, picture->code(), presented, false);
                }
                release(texture);
            }
            ABI::Windows::Foundation::IClosable *closable = nullptr;
            if (SUCCEEDED(captured->QueryInterface(__uuidof(ABI::Windows::Foundation::IClosable), reinterpret_cast<void **>(&closable)))) {
                closable->Close();
                release(closable);
            }
            release(captured);
        }
        if (picture != nullptr) picture->save(L"C:\\ProgramData\\av\\d3d9-dirty-present-compositor-last.bmp");
        delete picture;
        release(staging);
        ABI::Windows::Foundation::IClosable *closable = nullptr;
        if (SUCCEEDED(session->QueryInterface(__uuidof(ABI::Windows::Foundation::IClosable), reinterpret_cast<void **>(&closable)))) closable->Close();
        release(closable);
        if (SUCCEEDED(pool->QueryInterface(__uuidof(ABI::Windows::Foundation::IClosable), reinterpret_cast<void **>(&closable)))) closable->Close();
        release(closable);
        release(session);
        release(pool);
        release(pools);
        release(item);
        release(interop);
        release(winrt_device);
        release(context);
        release(device);
        release(adapter);
        release(dxgi);
        return true;
    }
#undef WATCH

    static bool read_into(ID3D11DeviceContext *context, ID3D11Texture2D *staging, ID3D11Texture2D *texture,
                          const D3D11_TEXTURE2D_DESC &description, Pixels &picture)
    {
        D3D11_MAPPED_SUBRESOURCE map;
        context->CopyResource(staging, texture);
        if (FAILED(context->Map(staging, 0, D3D11_MAP_READ, 0, &map))) return false;
        for (uint32_t y = 0; y < description.Height; y++)
            memcpy(picture.rows() + size_t{y} * description.Width,
                   static_cast<const uint8_t *>(map.pData) + size_t{y} * map.RowPitch, size_t{description.Width} * 4u);
        context->Unmap(staging, 0);
        return true;
    }

    static ID3D11Texture2D *texture_of(wgc::IDirect3D11CaptureFrame *frame)
    {
        wgd3d::IDirect3DSurface *surface = nullptr;
        DxgiAccess              *access = nullptr;
        ID3D11Texture2D         *texture = nullptr;
        if (SUCCEEDED(frame->get_Surface(&surface)) &&
            SUCCEEDED(surface->QueryInterface(__uuidof(DxgiAccess), reinterpret_cast<void **>(&access))))
            access->GetInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture));
        release(access);
        release(surface);
        return texture;
    }

    HWND                 window_;
    const Options       &options_;
    const volatile LONG &presented_;
    Judge                judge_;
    HANDLE               thread_ = nullptr;
    volatile LONG        started_ = 0, stopped_ = 0, stopping_ = 0;
    uint32_t             frames_ = 0u;
    uint32_t             rereads_differed_ = 0u;
};

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

bool parse(int argc, wchar_t **argv, Options &options)
{
    for (int index = 1; index < argc; index++) {
        const wchar_t *argument = argv[index];
        const bool     more = index + 1 < argc;
        if (wcscmp(argument, L"partial") == 0) options.mode = Mode::partial;
        else if (wcscmp(argument, L"full") == 0) options.mode = Mode::full;
        else if (wcscmp(argument, L"--ex") == 0) options.ex = true;
        else if (wcscmp(argument, L"--source-rect") == 0) options.source_rect = true;
        else if (wcscmp(argument, L"--no-read") == 0) options.read = false;
        else if (wcscmp(argument, L"--screen") == 0) options.screen = true;
        else if (wcscmp(argument, L"--wgc") == 0) options.wgc = true;
        else if (wcscmp(argument, L"--read-every") == 0 && more) options.read_every = wcstoul(argv[++index], nullptr, 10);
        else if (wcscmp(argument, L"--rect-order") == 0 && more) options.rect_order = wcstoul(argv[++index], nullptr, 10);
        else if (wcscmp(argument, L"--settle-capture-ms") == 0 && more) options.settle_capture_ms = wcstoul(argv[++index], nullptr, 10);
        else if (wcscmp(argument, L"--verbose") == 0) options.verbose = true;
        else if (wcscmp(argument, L"--frames") == 0 && more) options.frames = wcstoul(argv[++index], nullptr, 10);
        else if (wcscmp(argument, L"--interval-ms") == 0 && more) options.interval_ms = wcstoul(argv[++index], nullptr, 10);
        else if (wcscmp(argument, L"--settle-ms") == 0 && more) options.settle_ms = wcstoul(argv[++index], nullptr, 10);
        else if (wcscmp(argument, L"--width") == 0 && more) options.width = wcstoul(argv[++index], nullptr, 10);
        else if (wcscmp(argument, L"--height") == 0 && more) options.height = wcstoul(argv[++index], nullptr, 10);
        else {
            wprintf(L"unknown argument %ls\n", argument);
            return false;
        }
    }
    return options.frames > 0u && options.frames < (1u << code_bits) && options.width >= 320u && options.height >= 240u &&
           options.read_every > 0u && options.rect_order < 4u;
}

D3DPRESENT_PARAMETERS present_parameters(const Options &options, HWND window)
{
    D3DPRESENT_PARAMETERS present = {};
    present.BackBufferWidth = options.width;
    present.BackBufferHeight = options.height;
    present.BackBufferFormat = D3DFMT_X8R8G8B8;
    present.BackBufferCount = 1;
    present.SwapEffect = options.mode == Mode::partial ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.Windowed = TRUE;
    present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    return present;
}

HRESULT make_device(const Options &options, HWND window, IDirect3D9 *&direct3d, IDirect3DDevice9 *&device)
{
    D3DPRESENT_PARAMETERS present = present_parameters(options, window);
    const DWORD behaviour = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED;
    if (options.ex) {
        IDirect3D9Ex *ex = nullptr;
        const HRESULT made = Direct3DCreate9Ex(D3D_SDK_VERSION, &ex);
        if (FAILED(made)) return made;
        IDirect3DDevice9Ex *device_ex = nullptr;
        const HRESULT created = ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, behaviour, &present,
                                                   nullptr, &device_ex);
        direct3d = ex;
        device = device_ex;
        return created;
    }
    direct3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (direct3d == nullptr) return E_FAIL;
    return direct3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, behaviour, &present, &device);
}

}  // namespace

int wmain(int argc, wchar_t **argv)
{
    Options options;
    if (!parse(argc, argv, options)) {
        wprintf(L"d3d9_dirty_present.exe [partial|full] [--ex] [--frames N] [--interval-ms N] [--width W --height H] "
                L"[--source-rect] [--settle-ms N] [--screen] [--wgc] [--read-every N] [--rect-order N] [--no-read] "
                L"[--verbose]\n");
        return 1;
    }
    setvbuf(stdout, nullptr, _IONBF, 0);
    /* Physical pixels throughout: a screen read at the client's origin must
     * land on the client. */
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = L"d3d9_dirty_present";
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&window_class);
    RECT rectangle = {0, 0, static_cast<LONG>(options.width), static_cast<LONG>(options.height)};
    AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowW(L"d3d9_dirty_present", L"d3d9_dirty_present", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 60, 60,
                                rectangle.right - rectangle.left, rectangle.bottom - rectangle.top, nullptr, nullptr,
                                window_class.hInstance, nullptr);
    /* In front of whatever else is open: a screen read sees only this window. */
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(window);
    pump();

    /* The capture first: DWM gives a captured window a new redirection
     * surface, and a Direct3D 9 device that opened the old one at its creation
     * reports the change as a lost device. */
    volatile LONG presented_count = 0;
    CompositorWatcher watcher(window, options, presented_count);
    if (options.wgc && !watcher.start()) {
        wprintf(L"d3d9_dirty_present: FAIL (no compositor capture)\n");
        return 4;
    }
    if (options.wgc) Sleep(300);
    IDirect3D9       *direct3d = nullptr;
    IDirect3DDevice9 *device = nullptr;
    const HRESULT     made = make_device(options, window, direct3d, device);
    wprintf(L"%ls device %ls 0x%08lx, %ux%u, %u frames every %u ms, read %ls after every %u%ls\n",
            options.ex ? L"Direct3D9Ex" : L"Direct3D9",
            options.mode == Mode::partial ? L"COPY (partial redraws)" : L"DISCARD (full redraws)",
            static_cast<unsigned long>(made), options.width, options.height, options.frames, options.interval_ms,
            options.screen ? L"off the screen" : L"through DWM", options.read_every,
            options.wgc ? L", and watched through the compositor" : L"");
    if (FAILED(made)) return 2;

    Picture      picture(device, options);
    WindowReader reader(window, options.width, options.height, options.screen);
    Judge        judge(options, options.screen ? L"screen" : L"dwm");
    D3DVIEWPORT9 whole = {0u, 0u, options.width, options.height, 0.0f, 1.0f};
    device->SetViewport(&whole);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    /* Frame 0: the whole picture, presented whole. */
    device->BeginScene();
    picture.erase(RECT{0, 0, static_cast<LONG>(options.width), static_cast<LONG>(options.height)},
                  options.mode == Mode::partial ? background : full_background);
    picture.draw_code(0u);
    picture.draw_handle(handle_rect(options, 0u));
    device->EndScene();
    HRESULT shown = device->Present(nullptr, nullptr, nullptr, nullptr);
    uint32_t resets = 0u;
    for (uint32_t attempt = 0u; shown == D3DERR_DEVICELOST && attempt < 5u; attempt++) {
        resets++;
        const HRESULT state = device->TestCooperativeLevel();
        wprintf(L"device lost at frame 0 (cooperative level 0x%08lx), resetting\n", static_cast<unsigned long>(state));
        Sleep(100);
        D3DPRESENT_PARAMETERS again = present_parameters(options, window);
        const HRESULT reset = device->Reset(&again);
        wprintf(L"Reset 0x%08lx\n", static_cast<unsigned long>(reset));
        if (FAILED(reset)) return 3;
        device->SetViewport(&whole);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->BeginScene();
        picture.erase(RECT{0, 0, static_cast<LONG>(options.width), static_cast<LONG>(options.height)},
                      options.mode == Mode::partial ? background : full_background);
        picture.draw_code(0u);
        picture.draw_handle(handle_rect(options, 0u));
        device->EndScene();
        shown = device->Present(nullptr, nullptr, nullptr, nullptr);
    }
    if (FAILED(shown)) {
        wprintf(L"Present 0x%08lx at frame 0\n", static_cast<unsigned long>(shown));
        return 3;
    }
    Sleep(200);
    pump();
    if (options.read && reader.read()) judge.judge(reader.pixels(), reader.pixels().code(), 0u, false);

    uint32_t presented = 0u;
    for (uint32_t frame = 1; frame < options.frames; frame++) {
        pump();
        const RECT old = handle_rect(options, frame - 1u);
        const RECT now = handle_rect(options, frame);
        device->BeginScene();
        if (options.mode == Mode::partial) {
            picture.erase(old, background);
        } else {
            picture.erase(RECT{0, 0, static_cast<LONG>(options.width), static_cast<LONG>(options.height)}, full_background);
        }
        picture.draw_code(frame);
        picture.draw_handle(now);
        device->EndScene();
        /* Counted as presented before the present: a compositor frame that
         * shows it was composed after this, never before. */
        InterlockedExchange(&presented_count, static_cast<LONG>(frame));
        if (options.mode == Mode::partial) {
            const RECT code = code_rect_at(code_y), bottom = code_rect_at(bottom_code_y(options));
            const RECT orders[4][4] = {{old, now, code, bottom}, {now, old, code, bottom}, {code, old, now, bottom},
                                       {code, now, old, bottom}};
            const RECT *dirty = orders[options.rect_order];
            if (options.source_rect) {
                const RECT all = bound(dirty, 4u);
                shown = device->Present(&all, &all, nullptr, nullptr);
            } else {
                Region region;
                make_region(region, dirty, 4u);
                shown = device->Present(nullptr, nullptr, nullptr, reinterpret_cast<const RGNDATA *>(&region));
            }
        } else {
            shown = device->Present(nullptr, nullptr, nullptr, nullptr);
        }
        if (shown == D3DERR_DEVICELOST) {
            /* As a game does: wait for the device to be resettable, reset it
             * with the same chain, and go on; frame 0's picture is redrawn
             * whole since a reset empties the back buffer. */
            resets++;
            wprintf(L"device lost at frame %u, resetting\n", frame);
            for (uint32_t waited = 0; waited < 200u && device->TestCooperativeLevel() == D3DERR_DEVICELOST; waited++) {
                Sleep(10);
                pump();
            }
            D3DPRESENT_PARAMETERS again = present_parameters(options, window);
            const HRESULT reset = device->Reset(&again);
            if (FAILED(reset)) {
                wprintf(L"Reset 0x%08lx at frame %u\n", static_cast<unsigned long>(reset), frame);
                break;
            }
            device->SetViewport(&whole);
            device->SetRenderState(D3DRS_LIGHTING, FALSE);
            device->SetRenderState(D3DRS_ZENABLE, FALSE);
            device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            device->BeginScene();
            picture.erase(RECT{0, 0, static_cast<LONG>(options.width), static_cast<LONG>(options.height)},
                          options.mode == Mode::partial ? background : full_background);
            picture.draw_code(frame);
            picture.draw_handle(now);
            device->EndScene();
            shown = device->Present(nullptr, nullptr, nullptr, nullptr);
        }
        if (FAILED(shown)) {
            wprintf(L"Present 0x%08lx at frame %u\n", static_cast<unsigned long>(shown), frame);
            break;
        }
        presented = frame;
        if (options.read && frame % options.read_every == 0u && reader.read()) {
            const int64_t code = reader.pixels().code();
            if (options.verbose) wprintf(L"frame %u presented, window shows %lld\n", frame, code);
            judge.judge(reader.pixels(), code, frame, false);
        }
        if (options.interval_ms != 0u) Sleep(options.interval_ms);
    }
    /* Settled: the last frame, and nothing but it. */
    Sleep(options.settle_ms);
    pump();
    if (options.read && reader.read()) {
        const int64_t code = reader.pixels().code();
        wprintf(L"settled: window shows frame %lld after frame %u\n", code, presented);
        judge.judge(reader.pixels(), code, presented, true);
        reader.pixels().save(L"C:\\ProgramData\\av\\d3d9-dirty-present-final.bmp");
    }
    if (options.wgc) {
        watcher.stop();
        watcher.judge().report(presented);
        if (options.settle_capture_ms != 0u)
            wprintf(L"compositor: %u frames read differently the second time\n", watcher.rereads_differed());
    }
    if (options.read) judge.report(presented);
    if (resets != 0u) wprintf(L"the device was lost and reset %u times\n", resets);
    const uint32_t failures = judge.failures() + (options.wgc ? watcher.judge().failures() : 0u);
    wprintf(failures == 0u ? L"d3d9_dirty_present: PASS\n" : L"d3d9_dirty_present: FAIL (%u failures)\n", failures);
    device->Release();
    direct3d->Release();
    DestroyWindow(window);
    return failures != 0u ? 10 : 0;
}
