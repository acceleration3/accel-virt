#include "device/shim/loader_host_apis.hpp"

#include "device/abi/rvk_executor.h"

#include <dlfcn.h>
#include <string.h>
#include <vulkan/vulkan.h>

namespace av {

namespace {

/* The loader's own soname. Not linked, because a library that will not load
 * without a GPU cannot tell the device it has no GPU. */
constexpr const char *vulkan_loader = "libvulkan.so.1";

/* How many extension names are asked for at a time. The loader will report
 * hundreds and only one of them matters. */
constexpr uint32_t batch_size = 64u;

template <typename T>
T symbol_in(void *library, const char *name)
{
    return reinterpret_cast<T>(dlsym(library, name));
}

}  // namespace

LoaderHostApis::LoaderHostApis()
    : library_(nullptr), instance_(nullptr), uuids_{}, sub_apis_{}, vulkan_kinds_{}, count_(0u) {}

LoaderHostApis::~LoaderHostApis() { close(); }

void LoaderHostApis::open()
{
    if (library_ != nullptr) return;
    library_ = dlopen(vulkan_loader, RTLD_NOW | RTLD_LOCAL);
    if (library_ == nullptr) return;
    enumerate();
}

void LoaderHostApis::enumerate()
{
    auto *const create = symbol_in<PFN_vkCreateInstance>(library_, "vkCreateInstance");
    auto *const enumerate_devices =
        symbol_in<PFN_vkEnumeratePhysicalDevices>(library_, "vkEnumeratePhysicalDevices");
    auto *const properties2 =
        symbol_in<PFN_vkGetPhysicalDeviceProperties2>(library_, "vkGetPhysicalDeviceProperties2");
    auto *const extensions = symbol_in<PFN_vkEnumerateDeviceExtensionProperties>(
        library_, "vkEnumerateDeviceExtensionProperties");
    if (create == nullptr || enumerate_devices == nullptr || properties2 == nullptr ||
        extensions == nullptr)
        return;

    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "rvk-executor";
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo instance = {};
    instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance.pApplicationInfo = &application;

    VkInstance handle = VK_NULL_HANDLE;
    if (create(&instance, nullptr, &handle) != VK_SUCCESS) return;
    instance_ = handle;

    VkPhysicalDevice devices[HostGpuSet::max_gpus];
    uint32_t found = HostGpuSet::max_gpus;
    if (enumerate_devices(handle, &found, devices) < 0) return;
    if (found > HostGpuSet::max_gpus) found = HostGpuSet::max_gpus;

    for (uint32_t i = 0u; i < found; i++) {
        VkPhysicalDeviceIDProperties identity = {};
        identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        VkPhysicalDeviceProperties2 device = {};
        device.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        device.pNext = &identity;
        properties2(devices[i], &device);

        memcpy(uuids_[i], identity.deviceUUID, uuid_bytes);
        sub_apis_[i] = RVK_EXECUTOR_SUB_API_VULKAN;
        vulkan_kinds_[i] = kinds_of(devices[i]);
    }
    count_ = found;
}

uint32_t LoaderHostApis::kinds_of(void *device)
{
    auto *const extensions = symbol_in<PFN_vkEnumerateDeviceExtensionProperties>(
        library_, "vkEnumerateDeviceExtensionProperties");
    if (extensions == nullptr) return 0u;
    auto *const physical = static_cast<VkPhysicalDevice>(device);

    uint32_t available = 0u;
    if (extensions(physical, nullptr, &available, nullptr) != VK_SUCCESS) return 0u;
    for (uint32_t offset = 0u; offset < available; offset += batch_size) {
        VkExtensionProperties named[batch_size];
        uint32_t             batch = available - offset < batch_size ? available - offset : batch_size;
        if (extensions(physical, nullptr, &batch, named) < 0) return 0u;
        for (uint32_t n = 0u; n < batch; n++) {
            if (strcmp(named[n].extensionName, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME) == 0)
                return RVK_BLOB_HOST_POINTER;
        }
        if (batch < batch_size) break;
    }
    return 0u;
}

void LoaderHostApis::close()
{
    if (instance_ != nullptr && library_ != nullptr) {
        auto *const destroy = symbol_in<PFN_vkDestroyInstance>(library_, "vkDestroyInstance");
        if (destroy != nullptr) destroy(static_cast<VkInstance>(instance_), nullptr);
    }
    instance_ = nullptr;
    if (library_ != nullptr) dlclose(library_);
    library_ = nullptr;
    count_ = 0u;
}

uint32_t LoaderHostApis::gpu_count() { return count_; }

void LoaderHostApis::gpu_uuid(uint32_t index, uint8_t *out)
{
    if (index >= count_ || out == nullptr) return;
    memcpy(out, uuids_[index], uuid_bytes);
}

uint32_t LoaderHostApis::sub_apis(uint32_t index)
{
    return index < count_ ? sub_apis_[index] : 0u;
}

uint32_t LoaderHostApis::import_kinds(uint32_t index, uint32_t sub_api)
{
    if (index >= count_ || sub_api != RVK_EXECUTOR_SUB_API_VULKAN) return 0u;
    return vulkan_kinds_[index];
}

}  // namespace av
