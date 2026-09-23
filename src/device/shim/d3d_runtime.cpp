#include "device/shim/d3d_runtime.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

namespace av {

namespace {

constexpr size_t path_bytes = 4096u;

}  // namespace

/* The file `name` in RVK_D3D_PATH, or the bare name for the loader's own
 * search when that is unset. */
void *D3dRuntime::open_library(const char *name) const
{
    AV_SCOPE("D3dRuntime::open_library");
    const char *const directory = getenv("RVK_D3D_PATH");
    if (directory == nullptr || directory[0] == '\0') return dlopen(name, RTLD_NOW | RTLD_LOCAL);
    char path[path_bytes];
    const int written = snprintf(path, sizeof(path), "%s/%s", directory, name);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(path)) return nullptr;
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

D3dRuntime::D3dRuntime()
    : dxgi_(nullptr), d3d11_(nullptr), d3d12_(nullptr), create_factory_(nullptr), create_device11_(nullptr),
      create_device12_(nullptr)
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
    /* SDL3's driver is the one that loads with no display: the device never
     * presents through DXVK, it only needs a factory. */
    setenv("DXVK_WSI_DRIVER", "SDL3", 0);
    dxgi_ = open_library("libdxvk_dxgi.so");
    d3d11_ = open_library("libdxvk_d3d11.so");
    if (dxgi_ == nullptr || d3d11_ == nullptr) {
        close();
        return false;
    }
    create_factory_ = reinterpret_cast<CreateFactory>(dlsym(dxgi_, "CreateDXGIFactory1"));
    create_device11_ = reinterpret_cast<CreateDevice11>(dlsym(d3d11_, "D3D11CreateDevice"));
    if (create_factory_ == nullptr || create_device11_ == nullptr) {
        close();
        return false;
    }
    d3d12_ = open_library("libvkd3d-proton-d3d12.so");
    if (d3d12_ != nullptr)
        create_device12_ = reinterpret_cast<CreateDevice12>(dlsym(d3d12_, "D3D12CreateDevice"));
    return true;
}

void D3dRuntime::close()
{
    AV_SCOPE("D3dRuntime::close");
    /* Never unloaded: DXVK and vkd3d-proton keep threads of their own that
     * outlive any one device, and unloading code a thread is still in is a
     * crash at exit rather than a leak. */
    create_factory_ = nullptr;
    create_device11_ = nullptr;
    create_device12_ = nullptr;
    dxgi_ = nullptr;
    d3d11_ = nullptr;
    d3d12_ = nullptr;
}

bool D3dRuntime::has_d3d11() const
{
    AV_SCOPE("D3dRuntime::has_d3d11");
    return create_device11_ != nullptr;
}

bool D3dRuntime::has_d3d12() const
{
    AV_SCOPE("D3dRuntime::has_d3d12");
    return create_device12_ != nullptr;
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
            VkPhysicalDeviceIDProperties identity = {};
            identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
            VkPhysicalDeviceProperties2 properties = {};
            properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties.pNext = &identity;
            /* Through DXVK's own instance: a physical device is only
             * meaningful to the instance that enumerated it. */
            auto query = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
                vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2"));
            if (query != nullptr) {
                query(physical, &properties);
                if (memcmp(identity.deviceUUID, uuid, uuid_bytes) == 0) {
                    found = adapter;
                    adapter = nullptr;
                }
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

int32_t D3dRuntime::create_device12(const uint8_t *uuid, void *&device) const
{
    AV_SCOPE("D3dRuntime::create_device12");
    device = nullptr;
    if (create_device12_ == nullptr || uuid == nullptr) return E_FAIL;
    const GUID device_id = __uuidof(ID3D12Device);
    return create_device12_(nullptr, D3D_FEATURE_LEVEL_12_0, &device_id, &device);
}

}  // namespace av
