// Cross-adapter sharing between two avgpu adapters, as Affinity does it
// (docs/host-memory.md, "Application memory"): row-major memory made on one
// adapter and opened on another, and what each step answers.
//
//   cross_adapter_probe.exe
//
// With two or more hardware adapters (the guest's two avgpu functions), for
// every ordered pair (A, B):
//   1. Direct3D 12: a heap of 1 MiB flagged SHARED | SHARED_CROSS_ADAPTER on A,
//      a shared handle made of it (ID3D12Device::CreateSharedHandle), and the
//      heap opened on B (OpenSharedHandle) with a buffer placed in it.
//   2. Direct3D 11: a 1249x805 B8G8R8A8 texture of row-major layout, shared
//      (ID3D11Device3 / D3D11_TEXTURE_LAYOUT_ROW_MAJOR with a shared NT
//      handle), made on A and opened on B (ID3D11Device1::OpenSharedResource1).
// Each step prints its HRESULT, so the miniport's log line for a refused
// allocation ("avgpu: refused GDI surface: ... width .. format .. type ..",
// lsw-avgpu.log) and the driver's ("a cross-adapter resource: ... misc ..")
// can be read beside the probe's: the former settles which format space
// D3DKMDT_GDISURFACEDATA.Format is in, the latter what misc flags the runtime
// gave the creation. The pixels are not compared: once both opens succeed, the
// colour is checked by drawing on one adapter and reading on the other.
#include <windows.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

void say(const char *what, HRESULT result)
{
    std::printf("  %-52s 0x%08lx%s\n", what, static_cast<unsigned long>(result), SUCCEEDED(result) ? "" : "  FAILED");
}

bool heap_on_both(IDXGIAdapter1 *a, IDXGIAdapter1 *b)
{
    ID3D12Device *device_a = nullptr;
    ID3D12Device *device_b = nullptr;
    HRESULT       result = D3D12CreateDevice(a, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_a));
    say("D3D12CreateDevice (creator)", result);
    if (FAILED(result)) return false;
    result = D3D12CreateDevice(b, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_b));
    say("D3D12CreateDevice (opener)", result);
    if (FAILED(result)) {
        device_a->Release();
        return false;
    }
    D3D12_HEAP_DESC desc = {};
    desc.SizeInBytes = 1u << 20u;
    desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    desc.Flags = D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER | D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    ID3D12Heap *heap = nullptr;
    result = device_a->CreateHeap(&desc, IID_PPV_ARGS(&heap));
    say("CreateHeap SHARED_CROSS_ADAPTER", result);
    bool done = false;
    if (SUCCEEDED(result)) {
        HANDLE shared = nullptr;
        result = device_a->CreateSharedHandle(heap, nullptr, GENERIC_ALL, nullptr, &shared);
        say("CreateSharedHandle", result);
        if (SUCCEEDED(result)) {
            ID3D12Heap *opened = nullptr;
            result = device_b->OpenSharedHandle(shared, IID_PPV_ARGS(&opened));
            say("OpenSharedHandle on the other adapter", result);
            if (SUCCEEDED(result)) {
                D3D12_RESOURCE_DESC buffer = {};
                buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                buffer.Width = 1u << 20u;
                buffer.Height = 1u;
                buffer.DepthOrArraySize = 1u;
                buffer.MipLevels = 1u;
                buffer.SampleDesc.Count = 1u;
                buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                buffer.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
                ID3D12Resource *placed = nullptr;
                result = device_b->CreatePlacedResource(opened, 0u, &buffer, D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                        IID_PPV_ARGS(&placed));
                say("CreatePlacedResource in the opened heap", result);
                done = SUCCEEDED(result);
                if (placed != nullptr) placed->Release();
                opened->Release();
            }
            CloseHandle(shared);
        }
        heap->Release();
    }
    device_b->Release();
    device_a->Release();
    return done;
}

bool texture_on_both(IDXGIAdapter1 *a, IDXGIAdapter1 *b)
{
    constexpr UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    ID3D11Device *device_a = nullptr;
    ID3D11Device *device_b = nullptr;
    HRESULT       result = D3D11CreateDevice(a, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, nullptr, 0u, D3D11_SDK_VERSION,
                                             &device_a, nullptr, nullptr);
    say("D3D11CreateDevice (creator)", result);
    if (FAILED(result)) return false;
    result = D3D11CreateDevice(b, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, nullptr, 0u, D3D11_SDK_VERSION, &device_b,
                               nullptr, nullptr);
    say("D3D11CreateDevice (opener)", result);
    if (FAILED(result)) {
        device_a->Release();
        return false;
    }
    ID3D11Device3 *creator = nullptr;
    result = device_a->QueryInterface(IID_PPV_ARGS(&creator));
    say("ID3D11Device3", result);
    bool done = false;
    if (SUCCEEDED(result)) {
        D3D11_TEXTURE2D_DESC1 desc = {};
        desc.Width = 1249u;
        desc.Height = 805u;
        desc.MipLevels = 1u;
        desc.ArraySize = 1u;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1u;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
        desc.TextureLayout = D3D11_TEXTURE_LAYOUT_ROW_MAJOR;
        ID3D11Texture2D1 *texture = nullptr;
        result = creator->CreateTexture2D1(&desc, nullptr, &texture);
        say("CreateTexture2D1 row-major shared", result);
        if (SUCCEEDED(result)) {
            IDXGIResource1 *resource = nullptr;
            result = texture->QueryInterface(IID_PPV_ARGS(&resource));
            HANDLE shared = nullptr;
            if (SUCCEEDED(result)) result = resource->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &shared);
            say("CreateSharedHandle", result);
            if (SUCCEEDED(result)) {
                ID3D11Device1 *opener = nullptr;
                result = device_b->QueryInterface(IID_PPV_ARGS(&opener));
                ID3D11Texture2D *opened = nullptr;
                if (SUCCEEDED(result)) result = opener->OpenSharedResource1(shared, IID_PPV_ARGS(&opened));
                say("OpenSharedResource1 on the other adapter", result);
                done = SUCCEEDED(result);
                if (opened != nullptr) opened->Release();
                if (opener != nullptr) opener->Release();
                CloseHandle(shared);
            }
            if (resource != nullptr) resource->Release();
            texture->Release();
        }
        creator->Release();
    }
    device_b->Release();
    device_a->Release();
    return done;
}

}  // namespace

int wmain()
{
    IDXGIFactory4 *factory = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        std::printf("no DXGI factory\n");
        return 2;
    }
    std::vector<IDXGIAdapter1 *> adapters;
    for (UINT index = 0u;; index++) {
        IDXGIAdapter1 *adapter = nullptr;
        if (factory->EnumAdapters1(index, &adapter) != S_OK) break;
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);
        std::printf("adapter %u: %ls luid %08lx:%08lx%s\n", index, desc.Description,
                    static_cast<unsigned long>(desc.AdapterLuid.HighPart),
                    static_cast<unsigned long>(desc.AdapterLuid.LowPart),
                    (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u ? " (software)" : "");
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0u) adapters.push_back(adapter);
    }
    if (adapters.size() < 2u) {
        std::printf("fewer than two hardware adapters: nothing to share between\n");
        return 2;
    }
    int failures = 0;
    for (size_t creator = 0u; creator < adapters.size(); creator++) {
        for (size_t opener = 0u; opener < adapters.size(); opener++) {
            if (creator == opener) continue;
            std::printf("creator %zu, opener %zu\n", creator, opener);
            std::printf(" Direct3D 12 heap\n");
            failures += heap_on_both(adapters[creator], adapters[opener]) ? 0 : 1;
            std::printf(" Direct3D 11 row-major texture\n");
            failures += texture_on_both(adapters[creator], adapters[opener]) ? 0 : 1;
        }
    }
    std::printf("%d step(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
