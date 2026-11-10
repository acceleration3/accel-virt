#include "executor/shim/d3d_runtime.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/env_setting.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/physical_device_identity.hpp"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

namespace av {

namespace {

constexpr size_t path_bytes = 4096u;

}

void *D3dRuntime::open_library(const char *name) const
{
    AV_SCOPE("D3dRuntime::open_library");
    const char *const directory = EnvSetting("AV_D3D_PATH").text();
    if (directory == nullptr || directory[0] == '\0') return dlopen(name, RTLD_NOW | RTLD_LOCAL);
    char path[path_bytes];
    const int written = snprintf(path, sizeof(path), "%s/%s", directory, name);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(path)) return nullptr;
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

D3dRuntime::D3dRuntime()
    : dxgi_(nullptr), d3d11_(nullptr), d3d12core_(nullptr), d3d12_(nullptr), d3d9_(nullptr), create_factory_(nullptr),
      create_device11_(nullptr), get_interface12_(nullptr), create9_(nullptr),
      open_dmabuf_heap_(nullptr),
      serialize_root_(nullptr), export_image_(nullptr), export_fence_(nullptr), abandon_waits_(nullptr), set_indirect_table_(nullptr),
      take_memory11_(nullptr), take_memory9_(nullptr)
{
    AV_SCOPE("D3dRuntime::D3dRuntime");
}

D3dRuntime::~D3dRuntime()
{
    AV_SCOPE("D3dRuntime::~D3dRuntime");
    close();
}

bool D3dRuntime::open()
{
    AV_SCOPE("D3dRuntime::open");
    if (d3d11_ != nullptr) return true;
    setenv("DXVK_WSI_DRIVER", "SDL3", 0);
    if (EnvSetting("WAYLAND_DISPLAY").text() == nullptr && EnvSetting("DISPLAY").text() == nullptr)
        setenv("SDL_VIDEO_DRIVER", "offscreen", 0);
    dxgi_ = open_library("libdxvk_dxgi.so");
    d3d11_ = open_library("libdxvk_d3d11.so");
    if (dxgi_ == nullptr || d3d11_ == nullptr) {
        close();
        return false;
    }
    create_factory_ = reinterpret_cast<CreateFactory>(dlsym(dxgi_, "CreateDXGIFactory1"));
    create_device11_ = reinterpret_cast<CreateDevice11>(dlsym(d3d11_, "D3D11CreateDevice"));
    take_memory11_ = reinterpret_cast<TakeOutOfMemory>(dlsym(d3d11_, "dxvk_take_out_of_memory"));
    if (create_factory_ == nullptr || create_device11_ == nullptr) {
        close();
        return false;
    }
    d3d12core_ = open_library("libvkd3d-proton-d3d12core.so");
    if (d3d12core_ != nullptr) d3d12_ = open_library("libvkd3d-proton-d3d12.so");
    if (d3d12_ != nullptr) {
        get_interface12_ = reinterpret_cast<GetInterface12>(dlsym(d3d12_, "D3D12GetInterface"));
        open_dmabuf_heap_ =
            reinterpret_cast<OpenDmabufHeap>(dlsym(d3d12core_, "vkd3d_proton_open_heap_from_dmabuf"));
        serialize_root_ = reinterpret_cast<SerializeRoot>(dlsym(d3d12_, "D3D12SerializeVersionedRootSignature"));
        export_image_ = reinterpret_cast<ExportImage>(dlsym(d3d12core_, "vkd3d_proton_export_image"));
        export_fence_ = reinterpret_cast<ExportFence>(dlsym(d3d12core_, "vkd3d_proton_export_fence"));
        abandon_waits_ = reinterpret_cast<AbandonWaits>(dlsym(d3d12core_, "vkd3d_proton_abandon_waits"));
        set_indirect_table_ =
            reinterpret_cast<SetIndirectTable>(dlsym(d3d12core_, "vkd3d_proton_set_indirect_va_table"));
    }
    d3d9_ = open_library("libdxvk_d3d9.so");
    if (d3d9_ != nullptr) create9_ = reinterpret_cast<Create9>(dlsym(d3d9_, "Direct3DCreate9"));
    if (d3d9_ != nullptr) take_memory9_ = reinterpret_cast<TakeOutOfMemory>(dlsym(d3d9_, "dxvk_take_out_of_memory"));
    return true;
}

void D3dRuntime::close()
{
    AV_SCOPE("D3dRuntime::close");
    create_factory_ = nullptr;
    create_device11_ = nullptr;
    get_interface12_ = nullptr;
    create9_ = nullptr;
    take_memory11_ = nullptr;
    take_memory9_ = nullptr;
    open_dmabuf_heap_ = nullptr;
    serialize_root_ = nullptr;
    export_image_ = nullptr;
    export_fence_ = nullptr;
    abandon_waits_ = nullptr;
    set_indirect_table_ = nullptr;
    dxgi_ = nullptr;
    d3d11_ = nullptr;
    d3d12core_ = nullptr;
    d3d12_ = nullptr;
    d3d9_ = nullptr;
}

void D3dRuntime::give_vram_gate(const AvVramGate &gate) const
{
    AV_SCOPE("D3dRuntime::give_vram_gate");
    void *const libraries[] = {d3d11_, d3d9_};
    for (void *library : libraries) {
        if (library == nullptr) continue;
        const SetVramGate set = reinterpret_cast<SetVramGate>(dlsym(library, "dxvk_proton_set_vram_gate"));
        if (set != nullptr) set(&gate);
    }
    if (d3d12core_ == nullptr) return;
    const SetVramGate set12 = reinterpret_cast<SetVramGate>(dlsym(d3d12core_, "vkd3d_proton_set_vram_gate"));
    if (set12 != nullptr) set12(&gate);
}

IDirect3D9 *D3dRuntime::create_d3d9() const
{
    AV_SCOPE("D3dRuntime::create_d3d9");
    return create9_ != nullptr ? create9_(D3D_SDK_VERSION) : nullptr;
}

IDXGIAdapter1 *D3dRuntime::adapter_for(const uint8_t *uuid) const
{
    AV_SCOPE("D3dRuntime::adapter_for");
    if (create_factory_ == nullptr || uuid == nullptr) return nullptr;
    IDXGIFactory1 *factory = nullptr;
    const GUID factory_id = __uuidof(IDXGIFactory1);
    if (FAILED(create_factory_(&factory_id, reinterpret_cast<void **>(&factory)))) return nullptr;
    IDXGIAdapter1 *found = nullptr;
    IDXGIAdapter1 *adapter = nullptr;
    for (UINT index = 0u; found == nullptr && factory->EnumAdapters1(index, &adapter) == S_OK; index++) {
        IDXGIVkInteropAdapter *interop = nullptr;
        if (SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIVkInteropAdapter), reinterpret_cast<void **>(&interop)))) {
            VkInstance instance = VK_NULL_HANDLE;
            VkPhysicalDevice physical = VK_NULL_HANDLE;
            interop->GetVulkanHandles(&instance, &physical);
            if (PhysicalDeviceIdentity(instance, physical).is(uuid)) {
                found = adapter;
                adapter = nullptr;
            }
            interop->Release();
        }
        if (adapter != nullptr) adapter->Release();
    }
    factory->Release();
    return found;
}

int32_t D3dRuntime::create_device11(IDXGIAdapter1 *adapter, ID3D11Device *&device, ID3D11DeviceContext *&context,
                                    uint32_t &feature_level) const
{
    AV_SCOPE("D3dRuntime::create_device11");
    device = nullptr;
    context = nullptr;
    feature_level = 0u;
    if (create_device11_ == nullptr || adapter == nullptr) return E_FAIL;
    const uint32_t levels[] = {D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
                               D3D_FEATURE_LEVEL_11_0};
    return create_device11_(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0u, levels,
                            static_cast<uint32_t>(sizeof(levels) / sizeof(levels[0])), D3D11_SDK_VERSION, &device,
                            &feature_level, &context);
}

uint32_t D3dRuntime::vulkan_index(const uint8_t *uuid) const
{
    AV_SCOPE("D3dRuntime::vulkan_index");
    constexpr uint32_t most_devices = 16u;
    constexpr uint32_t vulkan_1_1 = (1u << 22u) | (1u << 12u);
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.apiVersion = vulkan_1_1;
    VkInstanceCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create.pApplicationInfo = &application;
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&create, nullptr, &instance) != VK_SUCCESS) return ~0u;
    VkPhysicalDevice devices[most_devices] = {};
    uint32_t count = most_devices;
    uint32_t found = ~0u;
    if (vkEnumeratePhysicalDevices(instance, &count, devices) >= 0) {
        for (uint32_t index = 0u; index < count && found == ~0u; index++)
            if (PhysicalDeviceIdentity(instance, devices[index]).is(uuid)) found = index;
    }
    vkDestroyInstance(instance, nullptr);
    return found;
}

int32_t D3dRuntime::create_device12(const uint8_t *uuid, void *&device) const
{
    AV_SCOPE("D3dRuntime::create_device12");
    constexpr size_t index_bytes = 16u;
    device = nullptr;
    if (get_interface12_ == nullptr || uuid == nullptr) return E_FAIL;
    const uint32_t index = vulkan_index(uuid);
    if (index == ~0u) return E_FAIL;
    ID3D12DeviceFactory *factory = nullptr;
    const GUID factory_class = clsid_d3d12_device_factory;
    const GUID factory_id = __uuidof(ID3D12DeviceFactory);
    if (FAILED(get_interface12_(&factory_class, &factory_id, reinterpret_cast<void **>(&factory)))) return E_FAIL;
    char previous[index_bytes] = {};
    const char *const was = EnvSetting("VKD3D_VULKAN_DEVICE").text();
    if (was != nullptr) snprintf(previous, sizeof(previous), "%s", was);
    char chosen[index_bytes] = {};
    snprintf(chosen, sizeof(chosen), "%u", index);
    setenv("VKD3D_VULKAN_DEVICE", chosen, 1);
    const HRESULT made = factory->CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), &device);
    if (was != nullptr) setenv("VKD3D_VULKAN_DEVICE", previous, 1);
    else unsetenv("VKD3D_VULKAN_DEVICE");
    factory->Release();
    if (FAILED(made)) device = nullptr;
    return made;
}

int32_t D3dRuntime::open_dmabuf_heap(void *device, void *address, int fd, uint64_t size, void *&heap) const
{
    AV_SCOPE("D3dRuntime::open_dmabuf_heap");
    heap = nullptr;
    if (open_dmabuf_heap_ == nullptr || device == nullptr) return E_NOTIMPL;
    const GUID    heap_id = __uuidof(ID3D12Heap);
    const int32_t made = open_dmabuf_heap_(device, address, fd, size, &heap_id, &heap);
    if (FAILED(made)) heap = nullptr;
    return made;
}

int32_t D3dRuntime::export_image(void *resource, Vkd3dImageExport &image) const
{
    AV_SCOPE("D3dRuntime::export_image");
    if (export_image_ == nullptr || resource == nullptr) return E_NOTIMPL;
    return export_image_(resource, &image);
}

uint32_t D3dRuntime::take_out_of_memory() const
{
    AV_SCOPE("D3dRuntime::take_out_of_memory");
    return (take_memory11_ != nullptr ? take_memory11_() : 0u) + (take_memory9_ != nullptr ? take_memory9_() : 0u);
}

void D3dRuntime::abandon_waits(void *device) const
{
    AV_SCOPE("D3dRuntime::abandon_waits");
    if (abandon_waits_ != nullptr && device != nullptr) abandon_waits_(device);
}

bool D3dRuntime::translates_indirect_addresses() const
{
    AV_SCOPE("D3dRuntime::translates_indirect_addresses");
    return set_indirect_table_ != nullptr;
}

void D3dRuntime::set_indirect_va_table(void *list, uint64_t table) const
{
    AV_SCOPE("D3dRuntime::set_indirect_va_table");
    if (set_indirect_table_ != nullptr && list != nullptr) static_cast<void>(set_indirect_table_(list, table));
}

int32_t D3dRuntime::export_fence(void *fence, int &fd) const
{
    AV_SCOPE("D3dRuntime::export_fence");
    fd = -1;
    if (export_fence_ == nullptr || fence == nullptr) return E_NOTIMPL;
    const int32_t made = export_fence_(fence, &fd);
    if (FAILED(made)) fd = -1;
    return made;
}

int32_t D3dRuntime::serialize_root_signature(const void *desc, void *&blob) const
{
    AV_SCOPE("D3dRuntime::serialize_root_signature");
    blob = nullptr;
    if (serialize_root_ == nullptr || desc == nullptr) return E_NOTIMPL;
    void *errors = nullptr;
    const int32_t made = serialize_root_(desc, &blob, &errors);
    if (errors != nullptr) static_cast<IUnknown *>(errors)->Release();
    if (FAILED(made)) blob = nullptr;
    return made;
}

}
