// The guest half of docs/external-export.md, as a program a mirror copies:
// Direct3D 11 textures made to be read by a process outside QEMU, and the
// shared id avgpu minted for each, learnt in user mode with no driver call of
// this project's own - only D3DKMT, which gdi32 exports to every process.
//
//   shared_id_probe.exe [count] [wait]
//
// For each of `count` (default 3) B8G8R8A8 textures made with
// D3D11_RESOURCE_MISC_SHARED it prints the legacy shared handle and the id,
// and a frame copied into it, counted, with its event query waited for; with
// `wait` it keeps them shared until Enter is pressed. A
// process outside the guest exports the id over the device's export socket.
//
// The id is in the allocation's private driver data, which the miniport wrote
// back at DxgkDdiCreateAllocation and dxgkrnl hands every process that opens
// the resource: 56 bytes of avgpu's allocation descriptor, then a 40-byte
// record whose magic is "AVS\0" (0x00535641), version 1, and the id at byte 8.
// Opening the resource by its shared handle in this process reads them.
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// d3dkmthk.h's, which MinGW does not ship; the layouts are the WDK's.
typedef UINT D3DKMT_HANDLE;
typedef LONG NTSTATUS_VALUE;

struct D3DKMT_OPENADAPTERFROMLUID {
    LUID          AdapterLuid;
    D3DKMT_HANDLE hAdapter;
};
struct D3DKMT_CLOSEADAPTER {
    D3DKMT_HANDLE hAdapter;
};
struct D3DKMT_CREATEDEVICE {
    union {
        D3DKMT_HANDLE hAdapter;
        void         *pAdapter;
    };
    UINT          Flags;
    D3DKMT_HANDLE hDevice;
    void         *pCommandBuffer;
    UINT          CommandBufferSize;
    void         *pAllocationList;
    UINT          AllocationListSize;
    void         *pPatchLocationList;
    UINT          PatchLocationListSize;
};
struct D3DKMT_DESTROYDEVICE {
    D3DKMT_HANDLE hDevice;
};
struct D3DKMT_QUERYRESOURCEINFO {
    D3DKMT_HANDLE hDevice;
    D3DKMT_HANDLE hGlobalShare;
    void         *pPrivateRuntimeData;
    UINT          PrivateRuntimeDataSize;
    UINT          TotalPrivateDriverDataSize;
    UINT          ResourcePrivateDriverDataSize;
    UINT          NumAllocations;
};
struct D3DDDI_OPENALLOCATIONINFO {
    D3DKMT_HANDLE hAllocation;
    const void   *pPrivateDriverData;
    UINT          PrivateDriverDataSize;
};
struct D3DKMT_OPENRESOURCE {
    D3DKMT_HANDLE              hDevice;
    D3DKMT_HANDLE              hGlobalShare;
    UINT                       NumAllocations;
    D3DDDI_OPENALLOCATIONINFO *pOpenAllocationInfo;
    void                      *pPrivateRuntimeData;
    UINT                       PrivateRuntimeDataSize;
    void                      *pResourcePrivateDriverData;
    UINT                       ResourcePrivateDriverDataSize;
    void                      *pTotalPrivateDriverDataBuffer;
    UINT                       TotalPrivateDriverDataBufferSize;
    D3DKMT_HANDLE              hResource;
};
struct D3DKMT_DESTROYALLOCATION {
    D3DKMT_HANDLE        hDevice;
    D3DKMT_HANDLE        hResource;
    const D3DKMT_HANDLE *phAllocationList;
    UINT                 AllocationCount;
};

extern "C" {
NTSTATUS_VALUE WINAPI D3DKMTOpenAdapterFromLuid(D3DKMT_OPENADAPTERFROMLUID *);
NTSTATUS_VALUE WINAPI D3DKMTCloseAdapter(const D3DKMT_CLOSEADAPTER *);
NTSTATUS_VALUE WINAPI D3DKMTCreateDevice(D3DKMT_CREATEDEVICE *);
NTSTATUS_VALUE WINAPI D3DKMTDestroyDevice(const D3DKMT_DESTROYDEVICE *);
NTSTATUS_VALUE WINAPI D3DKMTQueryResourceInfo(D3DKMT_QUERYRESOURCEINFO *);
NTSTATUS_VALUE WINAPI D3DKMTOpenResource(D3DKMT_OPENRESOURCE *);
NTSTATUS_VALUE WINAPI D3DKMTDestroyAllocation(const D3DKMT_DESTROYALLOCATION *);
}

constexpr UINT     descriptor_bytes = 56u;   // avgpu's AllocationDescriptor
constexpr UINT     record_bytes = 40u;       // its SharedObjectRecord
constexpr uint32_t record_magic = 0x00535641u;
constexpr uint32_t record_version = 1u;

// A D3DKMT device on the adapter the Direct3D device runs on: what a shared
// resource is opened on.
class KmtDevice {
public:
    explicit KmtDevice(ID3D11Device *device)
    {
        IDXGIDevice  *dxgi = nullptr;
        IDXGIAdapter *adapter = nullptr;
        DXGI_ADAPTER_DESC described = {};
        if (FAILED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgi)))) return;
        const bool found = SUCCEEDED(dxgi->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetDesc(&described));
        if (adapter != nullptr) adapter->Release();
        dxgi->Release();
        D3DKMT_OPENADAPTERFROMLUID opened = {};
        opened.AdapterLuid = described.AdapterLuid;
        if (!found || D3DKMTOpenAdapterFromLuid(&opened) != 0) return;
        adapter_ = opened.hAdapter;
        D3DKMT_CREATEDEVICE created = {};
        created.hAdapter = adapter_;
        if (D3DKMTCreateDevice(&created) == 0) device_ = created.hDevice;
    }
    ~KmtDevice()
    {
        if (device_ != 0u) {
            const D3DKMT_DESTROYDEVICE destroyed = {device_};
            D3DKMTDestroyDevice(&destroyed);
        }
        if (adapter_ != 0u) {
            const D3DKMT_CLOSEADAPTER closed = {adapter_};
            D3DKMTCloseAdapter(&closed);
        }
    }
    KmtDevice(const KmtDevice &) = delete;
    KmtDevice &operator=(const KmtDevice &) = delete;

    bool usable() const { return device_ != 0u; }

    // The id avgpu minted over the resource `shared` names (a legacy shared
    // handle, IDXGIResource::GetSharedHandle); 0 when it is not avgpu's.
    uint64_t shared_id(HANDLE shared) const
    {
        const D3DKMT_HANDLE global = static_cast<D3DKMT_HANDLE>(reinterpret_cast<uintptr_t>(shared));
        D3DKMT_QUERYRESOURCEINFO sizes = {};
        sizes.hDevice = device_;
        sizes.hGlobalShare = global;
        if (D3DKMTQueryResourceInfo(&sizes) != 0 || sizes.NumAllocations != 1u ||
            sizes.TotalPrivateDriverDataSize < descriptor_bytes + record_bytes)
            return 0u;
        static unsigned char runtime[4096], resource[4096], allocations[4096];
        if (sizes.PrivateRuntimeDataSize > sizeof runtime || sizes.ResourcePrivateDriverDataSize > sizeof resource ||
            sizes.TotalPrivateDriverDataSize > sizeof allocations)
            return 0u;
        D3DDDI_OPENALLOCATIONINFO allocation = {};
        D3DKMT_OPENRESOURCE       open = {};
        open.hDevice = device_;
        open.hGlobalShare = global;
        open.NumAllocations = 1u;
        open.pOpenAllocationInfo = &allocation;
        open.pPrivateRuntimeData = runtime;
        open.PrivateRuntimeDataSize = sizes.PrivateRuntimeDataSize;
        open.pResourcePrivateDriverData = resource;
        open.ResourcePrivateDriverDataSize = sizes.ResourcePrivateDriverDataSize;
        open.pTotalPrivateDriverDataBuffer = allocations;
        open.TotalPrivateDriverDataBufferSize = sizes.TotalPrivateDriverDataSize;
        if (D3DKMTOpenResource(&open) != 0) return 0u;
        uint64_t id = 0u;
        const auto *const data = static_cast<const unsigned char *>(allocation.pPrivateDriverData);
        if (data != nullptr && allocation.PrivateDriverDataSize == descriptor_bytes + record_bytes) {
            uint32_t magic = 0u, version = 0u;
            std::memcpy(&magic, data + descriptor_bytes, sizeof magic);
            std::memcpy(&version, data + descriptor_bytes + 4u, sizeof version);
            if (magic == record_magic && version == record_version)
                std::memcpy(&id, data + descriptor_bytes + 8u, sizeof id);
        }
        // The open is this process's second hold on the resource: let go of it.
        const D3DKMT_DESTROYALLOCATION closed = {device_, open.hResource, nullptr, 0u};
        D3DKMTDestroyAllocation(&closed);
        return id;
    }

private:
    D3DKMT_HANDLE adapter_ = 0u;
    D3DKMT_HANDLE device_ = 0u;
};

}  // namespace

int wmain(int argc, wchar_t **argv)
{
    const int count = argc > 1 ? _wtoi(argv[1]) : 3;
    ID3D11Device        *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                 nullptr, 0u, D3D11_SDK_VERSION, &device, nullptr, &context))) {
        std::printf("FAIL no Direct3D 11 device\n");
        return 1;
    }
    KmtDevice kmt(device);
    if (!kmt.usable()) {
        std::printf("FAIL no D3DKMT device on the adapter\n");
        return 1;
    }
    D3D11_TEXTURE2D_DESC described = {};
    described.Width = 256u;
    described.Height = 128u;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    described.SampleDesc.Count = 1u;
    described.Usage = D3D11_USAGE_DEFAULT;
    described.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    described.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    D3D11_TEXTURE2D_DESC frame_described = described;
    frame_described.MiscFlags = 0u;
    ID3D11Texture2D *frame = nullptr;
    if (FAILED(device->CreateTexture2D(&frame_described, nullptr, &frame))) return 1;
    const D3D11_QUERY_DESC event = {D3D11_QUERY_EVENT, 0u};
    int failures = 0;
    for (int slot = 0; slot < count; slot++) {
        ID3D11Texture2D *texture = nullptr;
        IDXGIResource   *resource = nullptr;
        HANDLE           shared = nullptr;
        if (FAILED(device->CreateTexture2D(&described, nullptr, &texture)) ||
            FAILED(texture->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void **>(&resource))) ||
            FAILED(resource->GetSharedHandle(&shared))) {
            std::printf("FAIL slot %d: no shared texture\n", slot);
            failures++;
            continue;
        }
        const uint64_t id = kmt.shared_id(shared);
        // A frame into the slot: one copy counted, flushed, and its event
        // query waited for - the host has then run the copy (writes 1).
        ID3D11Query *done = nullptr;
        context->CopyResource(texture, frame);
        device->CreateQuery(&event, &done);
        context->End(done);
        context->Flush();
        BOOL finished = FALSE;
        while (done != nullptr && context->GetData(done, &finished, sizeof finished, 0u) == S_FALSE) Sleep(0);
        std::printf("%s slot %d: shared handle %p id 0x%016llx copies 1\n", id != 0u ? "ok" : "FAIL", slot, shared,
                    static_cast<unsigned long long>(id));
        failures += id != 0u ? 0 : 1;
        if (done != nullptr) done->Release();
        resource->Release();
        // Kept, so the ids printed stay shared while the program waits: a
        // consumer outside the guest can export them until Enter is pressed.
        (void)texture;
    }
    if (count > 0 && argc > 2) std::getchar();
    frame->Release();
    context->Release();
    device->Release();
    return failures == 0 ? 0 : 1;
}
