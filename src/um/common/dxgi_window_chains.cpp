#include "um/common/dxgi_window_chains.hpp"

#include "core/log/log_line.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/kmt.hpp"
#include "core/log/log_sink.hpp"
#include "icd/common/slot_format.hpp"
#include "icd/common/thread_lock.hpp"
#include "proto/resources/shared_allocation_data.hpp"
#include "um/common/shared_rows_scope.hpp"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>

namespace av {

namespace {

using CreateDevice = HRESULT(WINAPI *)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *,
                                       UINT, UINT, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
using CreateFactory = HRESULT(WINAPI *)(REFIID, void **);
using Compile = HRESULT(WINAPI *)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR,
                                  UINT, UINT, ID3DBlob **, ID3DBlob **);

/* Three vertices covering the target: a triangle twice its size, with no
 * vertex buffer behind it. */
constexpr char corners_source[] = "float4 main(uint id : SV_VertexID) : SV_Position {"
                                  " float2 at = float2((id << 1) & 2, id & 2);"
                                  " return float4(at * float2(2, -2) + float2(-1, 1), 0, 1); }";
/* A slot's texel of the row as far from the bottom as this pixel is from the
 * top: OpenGL's rows start at the bottom. */
constexpr char flipped_source[] = "Texture2D slot : register(t0);"
                                  "float4 main(float4 at : SV_Position) : SV_Target {"
                                  " uint width, height; slot.GetDimensions(width, height);"
                                  " return slot.Load(int3(int(at.x), int(height) - 1 - int(at.y), 0)); }";

/* What the first chain opened and presented are logged once under. */
constexpr char chain_open_key[] = "chain open";
constexpr char chain_presented_key[] = "chain presented";
constexpr uint32_t buffer_count = 2u;

}  // namespace

DxgiWindowChains::DxgiWindowChains(BlockSource &memory, Kmt &kmt, icd::ThreadLock &lock, LogSink &log)
    : kmt_(kmt), lock_(lock), lines_(memory, log, "av: "), d3d11_(nullptr), dxgi_(nullptr), create_device_(nullptr),
      create_factory_(nullptr), compiler_(nullptr), compile_(nullptr), load_refused_(false),
      devices_(), chains_(memory), windows_(memory, Pool::paged)
{
    AV_SCOPE("DxgiWindowChains::DxgiWindowChains");
}

DxgiWindowChains::~DxgiWindowChains()
{
    AV_SCOPE("DxgiWindowChains::~DxgiWindowChains");
    windows_.for_each([this](uint64_t, Chain *&chain) { drop_chain(*chain); });
    windows_.clear();
    chains_.destroy_all();
    for (Device &device : devices_) drop_device(device);
    if (d3d11_ != nullptr) FreeLibrary(static_cast<HMODULE>(d3d11_));
    if (dxgi_ != nullptr) FreeLibrary(static_cast<HMODULE>(dxgi_));
    if (compiler_ != nullptr) FreeLibrary(static_cast<HMODULE>(compiler_));
}

bool DxgiWindowChains::open(void *window, uint64_t luid, uint32_t width, uint32_t height, icd::SlotLayout layout,
                            uint32_t slots, uint64_t *ids, uint64_t *bytes)
{
    AV_SCOPE("DxgiWindowChains::open");
    if (window == nullptr || luid == 0u || width == 0u || height == 0u || slots == 0u || slots > max_slots)
        return false;
    lock_.acquire();
    Chain *chain = chain_of(window);
    const uint32_t device = device_for(luid);
    if (chain != nullptr && chain->device != device) {
        forget_chain(window);
        chain = nullptr;
    }
    if (chain == nullptr && device != max_devices) chain = new_chain(window);
    bool made = device != max_devices && chain != nullptr;
    if (made && (chain->chain.get() == nullptr || chain->width != width || chain->height != height ||
                 chain->count != slots || chain->layout != layout)) {
        chain->window = window;
        chain->device = device;
        chain->layout = layout;
        chain->format = icd::SlotFormat(layout).dxgi();
        made = make_chain(*chain, devices_[device], width, height) && make_slots(*chain, devices_[device], slots);
        if (!made) forget_chain(window);
    }
    if (made) {
        for (uint32_t slot = 0u; slot < slots; slot++) {
            ids[slot] = chain->ids[slot];
            bytes[slot] = chain->bytes[slot];
        }
        if (lines_.first(chain_open_key, 0))
            lines_.note_hex("a window's chain is open; its first slot is shared as", ids[0], 16u);
    }
    lock_.release();
    return made;
}

/* The copy or draw runs on the host's queue after the frame the slot holds,
 * which the caller waited for; the query after the present ends once it has.
 * A top-down slot is the back buffer's format and size, so it is copied as it
 * is. */
bool DxgiWindowChains::present(void *window, uint32_t slot)
{
    AV_SCOPE("DxgiWindowChains::present");
    lock_.acquire();
    Chain *const chain = chain_of(window);
    if (chain == nullptr || chain->chain.get() == nullptr || slot >= chain->count) {
        lock_.release();
        return false;
    }
    ID3D11DeviceContext *const    context = devices_[chain->device].context.get();
    ComReference<ID3D11Texture2D> back;
    bool presented = SUCCEEDED(
        chain->chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(back.receive())));
    if (presented) {
        if (chain->layout == icd::SlotLayout::rgba_bottom_up)
            draw_flipped(*chain, devices_[chain->device], back.get(), slot);
        else
            context->CopyResource(back.get(), chain->slots[slot].get());
        const HRESULT shown = chain->chain->Present(0, 0);
        presented = SUCCEEDED(shown);
        if (lines_.first(chain_presented_key, 0))
            lines_.note_hex("a window's chain presented its first frame, answering", static_cast<uint32_t>(shown), 16u);
        context->End(chain->copied.get());
        context->Flush();
        uint32_t looks = 0u;
        while (context->GetData(chain->copied.get(), nullptr, 0u, 0u) == S_FALSE && looks < copy_looks) {
            kmt_.sleep_briefly();
            looks++;
        }
    }
    lock_.release();
    return presented;
}

void DxgiWindowChains::close(void *window)
{
    AV_SCOPE("DxgiWindowChains::close");
    lock_.acquire();
    forget_chain(window);
    lock_.release();
}

bool DxgiWindowChains::loaded()
{
    AV_SCOPE("DxgiWindowChains::loaded");
    if (create_device_ != nullptr && create_factory_ != nullptr && compile_ != nullptr) return true;
    if (load_refused_) return false;
    d3d11_ = LoadLibraryA("d3d11.dll");
    dxgi_ = LoadLibraryA("dxgi.dll");
    compiler_ = LoadLibraryA("d3dcompiler_47.dll");
    if (compiler_ != nullptr)
        compile_ = reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(compiler_), "D3DCompile"));
    if (d3d11_ != nullptr)
        create_device_ = reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(d3d11_), "D3D11CreateDevice"));
    if (dxgi_ != nullptr)
        create_factory_ = reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(dxgi_), "CreateDXGIFactory1"));
    if (create_device_ != nullptr && create_factory_ != nullptr && compile_ != nullptr) return true;
    load_refused_ = true;
    refuse("d3d11.dll, dxgi.dll or d3dcompiler_47.dll did not load");
    return false;
}

uint32_t DxgiWindowChains::device_for(uint64_t luid)
{
    AV_SCOPE("DxgiWindowChains::device_for");
    uint32_t vacant = max_devices;
    for (uint32_t index = 0u; index < max_devices; index++) {
        if (devices_[index].device.get() != nullptr && devices_[index].luid == luid) return index;
        if (devices_[index].device.get() == nullptr && vacant == max_devices) vacant = index;
    }
    if (vacant == max_devices || !loaded()) return max_devices;
    if (make_device(luid, devices_[vacant])) return vacant;
    drop_device(devices_[vacant]);
    return max_devices;
}

bool DxgiWindowChains::make_device(uint64_t luid, Device &made)
{
    AV_SCOPE("DxgiWindowChains::make_device");
    made.luid = luid;
    if (FAILED(reinterpret_cast<CreateFactory>(create_factory_)(__uuidof(IDXGIFactory2),
                                                               reinterpret_cast<void **>(made.factory.receive())))) {
        refuse("no DXGI factory");
        return false;
    }
    ComReference<IDXGIAdapter1> adapter;
    bool                        found = false;
    for (UINT index = 0u; !found && made.factory->EnumAdapters1(index, adapter.receive()) != DXGI_ERROR_NOT_FOUND;
         index++) {
        DXGI_ADAPTER_DESC1 described = {};
        if (FAILED(adapter->GetDesc1(&described))) continue;
        const uint64_t adapter_luid = (static_cast<uint64_t>(static_cast<uint32_t>(described.AdapterLuid.HighPart))
                                       << 32u) |
                                      described.AdapterLuid.LowPart;
        found = adapter_luid == luid;
    }
    if (!found) {
        refuse("DXGI lists no adapter of the context's LUID");
        return false;
    }
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    const HRESULT           created = reinterpret_cast<CreateDevice>(create_device_)(
        adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0u, levels, 2u, D3D11_SDK_VERSION, made.device.receive(),
        nullptr, made.context.receive());
    if (FAILED(created)) {
        refuse("Direct3D 11 made no device on the context's adapter");
        return false;
    }
    return make_shaders(made) && kernel_device(luid, made);
}

bool DxgiWindowChains::kernel_device(uint64_t luid, Device &made)
{
    AV_SCOPE("DxgiWindowChains::kernel_device");
    AvgpuAdapters avgpu(kmt_);
    if (avgpu.learn() != Status::ok) return false;
    made.kmt_adapter = avgpu.take(avgpu.index_of(luid));
    if (made.kmt_adapter == 0u || kmt_.create_device(made.kmt_adapter, made.kmt_device) != Status::ok) {
        refuse("D3DKMT opens no device on the context's adapter");
        return false;
    }
    return true;
}

bool DxgiWindowChains::make_shaders(Device &made)
{
    AV_SCOPE("DxgiWindowChains::make_shaders");
    const Compile          compile = reinterpret_cast<Compile>(compile_);
    ComReference<ID3DBlob> corners, flipped;
    const bool             compiled =
        SUCCEEDED(compile(corners_source, sizeof corners_source - 1u, "corners", nullptr, nullptr, "main", "vs_4_0",
                          0u, 0u, corners.receive(), nullptr)) &&
        SUCCEEDED(compile(flipped_source, sizeof flipped_source - 1u, "flipped", nullptr, nullptr, "main", "ps_4_0",
                          0u, 0u, flipped.receive(), nullptr));
    D3D11_RASTERIZER_DESC rasterized = {};
    rasterized.FillMode = D3D11_FILL_SOLID;
    rasterized.CullMode = D3D11_CULL_NONE;
    rasterized.DepthClipEnable = TRUE;
    if (compiled &&
        SUCCEEDED(made.device->CreateVertexShader(corners->GetBufferPointer(), corners->GetBufferSize(), nullptr,
                                                  made.corners.receive())) &&
        SUCCEEDED(made.device->CreatePixelShader(flipped->GetBufferPointer(), flipped->GetBufferSize(), nullptr,
                                                 made.flipped.receive())) &&
        SUCCEEDED(made.device->CreateRasterizerState(&rasterized, made.unculled.receive())))
        return true;
    refuse("the shaders that turn a frame the right way up were not made");
    return false;
}

void DxgiWindowChains::draw_flipped(Chain &chain, Device &device, ID3D11Texture2D *back, uint32_t slot)
{
    AV_SCOPE("DxgiWindowChains::draw_flipped");
    ComReference<ID3D11RenderTargetView> target;
    if (FAILED(device.device->CreateRenderTargetView(back, nullptr, target.receive()))) return;
    ID3D11DeviceContext *const      context = device.context.get();
    const D3D11_VIEWPORT            whole = {0.0f, 0.0f, static_cast<float>(chain.width), static_cast<float>(chain.height),
                                             0.0f, 1.0f};
    ID3D11RenderTargetView *const   targets[1] = {target.get()};
    ID3D11ShaderResourceView *const read[1] = {chain.views[slot].get()};
    ID3D11ShaderResourceView *const none[1] = {nullptr};
    context->OMSetRenderTargets(1u, targets, nullptr);
    context->RSSetViewports(1u, &whole);
    context->RSSetState(device.unculled.get());
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(device.corners.get(), nullptr, 0u);
    context->PSSetShader(device.flipped.get(), nullptr, 0u);
    context->PSSetShaderResources(0u, 1u, read);
    context->Draw(3u, 0u);
    /* Bound as a source, a slot could not be copied into by the next frame
     * without Direct3D unbinding it first. */
    context->PSSetShaderResources(0u, 1u, none);
    context->OMSetRenderTargets(0u, nullptr, nullptr);
}

DxgiWindowChains::Chain *DxgiWindowChains::chain_of(void *window)
{
    AV_SCOPE("DxgiWindowChains::chain_of");
    Chain *const *const chain = windows_.find(reinterpret_cast<uintptr_t>(window));
    return chain != nullptr ? *chain : nullptr;
}

DxgiWindowChains::Chain *DxgiWindowChains::new_chain(void *window)
{
    AV_SCOPE("DxgiWindowChains::new_chain");
    Chain   *chain = nullptr;
    uint32_t slot = 0u;
    if (windows_.reserve(windows_.count() + 1u) != Status::ok || chains_.create(chain) != Status::ok) {
        refuse("no memory for another window's chain");
        return nullptr;
    }
    /* Room was made above, so the insert cannot fail. */
    static_cast<void>(windows_.insert(reinterpret_cast<uintptr_t>(window), chain, slot));
    return chain;
}

void DxgiWindowChains::forget_chain(void *window)
{
    AV_SCOPE("DxgiWindowChains::forget_chain");
    Chain *chain = nullptr;
    if (!windows_.take(reinterpret_cast<uintptr_t>(window), chain)) return;
    drop_chain(*chain);
    static_cast<void>(chains_.destroy(chain));
}

/* A chain of this window is made again at a new size: its slots go first, so
 * nothing holds its buffers when they are resized. */
bool DxgiWindowChains::make_chain(Chain &chain, Device &device, uint32_t width, uint32_t height)
{
    AV_SCOPE("DxgiWindowChains::make_chain");
    drop_slots(chain);
    if (chain.chain.get() != nullptr) {
        if (FAILED(chain.chain->ResizeBuffers(0u, width, height, static_cast<DXGI_FORMAT>(chain.format), 0u))) {
            refuse("the window's chain would not resize");
            return false;
        }
    } else {
        DXGI_SWAP_CHAIN_DESC1 described = {};
        described.Width = width;
        described.Height = height;
        described.Format = static_cast<DXGI_FORMAT>(chain.format);
        described.SampleDesc = {1u, 0u};
        described.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        described.BufferCount = buffer_count;
        described.Scaling = DXGI_SCALING_STRETCH;
        described.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        described.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        HWND const window = static_cast<HWND>(chain.window);
        if (FAILED(device.factory->CreateSwapChainForHwnd(device.device.get(), window, &described, nullptr, nullptr,
                                                          chain.chain.receive()))) {
            refuse("DXGI made no flip chain on the window");
            return false;
        }
        static_cast<void>(device.factory->MakeWindowAssociation(window, DXGI_MWA_NO_WINDOW_CHANGES));
    }
    const D3D11_QUERY_DESC query = {D3D11_QUERY_EVENT, 0u};
    if (chain.copied.get() == nullptr && FAILED(device.device->CreateQuery(&query, chain.copied.receive())))
        return false;
    chain.width = width;
    chain.height = height;
    return true;
}

bool DxgiWindowChains::make_slots(Chain &chain, Device &device, uint32_t count)
{
    AV_SCOPE("DxgiWindowChains::make_slots");
    D3D11_TEXTURE2D_DESC described = {};
    described.Width = chain.width;
    described.Height = chain.height;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = static_cast<DXGI_FORMAT>(chain.format);
    described.SampleDesc = {1u, 0u};
    described.Usage = D3D11_USAGE_DEFAULT;
    described.BindFlags = icd::SlotFormat(chain.layout).bind_flags();
    described.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    /* The ICD opens each slot's memory as an image of rows (ChainedImage,
     * OpenGL's memory object), so the host lays it out so. */
    const SharedRowsScope rows;
    for (uint32_t slot = 0u; slot < count; slot++) {
        if (FAILED(device.device->CreateTexture2D(&described, nullptr, chain.slots[slot].receive())) ||
            FAILED(device.device->CreateShaderResourceView(chain.slots[slot].get(), nullptr,
                                                           chain.views[slot].receive())) ||
            !identify(device, chain.slots[slot].get(), chain.ids[slot], chain.bytes[slot])) {
            refuse("a slot of the window's chain was not made shareable");
            return false;
        }
        chain.count = slot + 1u;
    }
    return true;
}

bool DxgiWindowChains::identify(Device &device, ID3D11Texture2D *texture, uint64_t &id, uint64_t &bytes)
{
    AV_SCOPE("DxgiWindowChains::identify");
    ComReference<IDXGIResource1> resource;
    if (FAILED(texture->QueryInterface(__uuidof(IDXGIResource1), reinterpret_cast<void **>(resource.receive()))))
        return false;
    HANDLE shared = nullptr;
    if (FAILED(resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
                                            &shared)))
        return false;
    SharedAllocationData carried;
    uint32_t             written = 0u;
    uint32_t             opened = 0u;
    const Status         read = kmt_.open_shared_object(device.kmt_device, reinterpret_cast<uint64_t>(shared), opened,
                                                        carried.lend(), static_cast<uint32_t>(SharedAllocationData::encoded_size), written);
    if (read == Status::ok) static_cast<void>(kmt_.destroy_shared_allocation(device.kmt_device, opened));
    static_cast<void>(kmt_.close_shared_object(reinterpret_cast<uint64_t>(shared)));
    if (read != Status::ok || carried.reread(written) != Status::ok || !carried.assigned()) return false;
    id = carried.object_id();
    bytes = carried.size();
    return true;
}

void DxgiWindowChains::drop_slots(Chain &chain)
{
    AV_SCOPE("DxgiWindowChains::drop_slots");
    for (uint32_t slot = 0u; slot < max_slots; slot++) {
        chain.views[slot].reset();
        chain.slots[slot].reset();
        chain.ids[slot] = 0u;
        chain.bytes[slot] = 0u;
    }
    chain.count = 0u;
}

/* Direct3D defers destroying what was released until its context lets go,
 * and DXGI makes a window another chain only once the last is destroyed: the
 * flush is what lets a window closed by one swapchain be opened by the next
 * (ID3D11DeviceContext::Flush, "Deferred Destruction Issues with Flip
 * Presentation Swap Chains"). */
void DxgiWindowChains::drop_chain(Chain &chain)
{
    AV_SCOPE("DxgiWindowChains::drop_chain");
    const bool held = chain.chain.get() != nullptr;
    drop_slots(chain);
    chain.copied.reset();
    chain.chain.reset();
    ID3D11DeviceContext *const context = devices_[chain.device].context.get();
    if (held && context != nullptr) {
        context->ClearState();
        context->Flush();
    }
    chain.window = nullptr;
    chain.device = 0u;
    chain.width = 0u;
    chain.height = 0u;
    chain.layout = icd::SlotLayout::rgba_bottom_up;
    chain.format = 0u;
}

void DxgiWindowChains::drop_device(Device &device)
{
    AV_SCOPE("DxgiWindowChains::drop_device");
    device.unculled.reset();
    device.flipped.reset();
    device.corners.reset();
    device.context.reset();
    device.device.reset();
    device.factory.reset();
    if (device.kmt_device != 0u) static_cast<void>(kmt_.destroy_device(device.kmt_device));
    if (device.kmt_adapter != 0u) static_cast<void>(kmt_.close_adapter(device.kmt_adapter));
    device.kmt_device = 0u;
    device.kmt_adapter = 0u;
    device.luid = 0u;
}

void DxgiWindowChains::refuse(const char *why)
{
    AV_SCOPE("DxgiWindowChains::refuse");
    LogLine(lines_)
        .text("no DXGI chain for a window: ")
        .text(why)
        .write();
}

}  // namespace av
