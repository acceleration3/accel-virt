#include "device/shim/loader_host_apis.hpp"

#include "device/abi/rvk_executor.h"

#include <CL/cl.h>
#include <CL/cl_ext.h>
#include <string.h>
#include <vulkan/vulkan.h>

namespace av {

LoaderHostApis::LoaderHostApis()
    : instance_(nullptr), uuids_{}, sub_apis_{}, vulkan_kinds_{}, opencl_kinds_{}, extensions_{}, count_(0u),
      opened_(false) {}

LoaderHostApis::~LoaderHostApis() { close(); }

void LoaderHostApis::open()
{
    if (opened_) return;
    opened_ = true;
    enumerate();
}

void LoaderHostApis::enumerate()
{
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "rvk-executor";
    /* Spelled out rather than VK_API_VERSION_1_1, which is a macro full of
     * C casts this build refuses. 1.1 is what VkPhysicalDeviceIDProperties
     * needs, and nothing here needs more. */
    application.apiVersion = (1u << 22) | (1u << 12);
    VkInstanceCreateInfo instance = {};
    instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance.pApplicationInfo = &application;

    VkInstance handle = VK_NULL_HANDLE;
    /* A host with no GPU, no driver or no loader at all answers here, and that
     * is an answer: the library then serves nothing and the device describes
     * no capset rather than one it cannot honour. */
    if (vkCreateInstance(&instance, nullptr, &handle) != VK_SUCCESS) return;
    instance_ = handle;

    VkPhysicalDevice devices[HostGpuSet::max_gpus];
    uint32_t found = HostGpuSet::max_gpus;
    if (vkEnumeratePhysicalDevices(handle, &found, devices) < 0) return;
    if (found > HostGpuSet::max_gpus) found = HostGpuSet::max_gpus;

    for (uint32_t i = 0u; i < found; i++) {
        VkPhysicalDeviceIDProperties identity = {};
        identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        VkPhysicalDeviceProperties2 device = {};
        device.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        device.pNext = &identity;
        vkGetPhysicalDeviceProperties2(devices[i], &device);

        memcpy(uuids_[i], identity.deviceUUID, uuid_bytes);
        sub_apis_[i] = RVK_EXECUTOR_SUB_API_VULKAN;
        vulkan_kinds_[i] = kinds_of(devices[i]);
        opencl_kinds_[i] = 0u;
        /* OpenCL is reported only where a device on this host says it is the
         * same GPU, rather than because the host has OpenCL at all. */
        if (serves_opencl(uuids_[i])) sub_apis_[i] |= RVK_EXECUTOR_SUB_API_OPENCL;
    }
    count_ = found;
}

uint32_t LoaderHostApis::kinds_of(void *device)
{
    auto *const physical = static_cast<VkPhysicalDevice>(device);

    /* Asked in one call, because vkEnumerateDeviceExtensionProperties has no
     * offset: a second call with a smaller count re-reads the list from the
     * start rather than continuing it, so anything beyond the first batch
     * would never be seen. A driver with more extensions than this reports
     * VK_INCOMPLETE and the ones past the end are not considered - the answer
     * is then "cannot import", which costs a copy and never a wrong mapping. */
    uint32_t available = max_extensions;
    if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &available, extensions_) < 0)
        return 0u;
    if (available > max_extensions) available = max_extensions;
    for (uint32_t n = 0u; n < available; n++) {
        if (strcmp(extensions_[n].extensionName, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME) == 0)
            return RVK_BLOB_HOST_POINTER;
    }
    return 0u;
}

void LoaderHostApis::close()
{
    if (instance_ != nullptr) vkDestroyInstance(static_cast<VkInstance>(instance_), nullptr);
    instance_ = nullptr;
    opened_ = false;
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
    if (index >= count_) return 0u;
    if (sub_api == RVK_EXECUTOR_SUB_API_VULKAN) return vulkan_kinds_[index];
    if (sub_api == RVK_EXECUTOR_SUB_API_OPENCL) return opencl_kinds_[index];
    return 0u;
}

bool LoaderHostApis::serves_opencl(const uint8_t *uuid) const
{
    cl_platform_id platforms[max_platforms];
    cl_uint        found = 0u;
    if (clGetPlatformIDs(max_platforms, platforms, &found) != CL_SUCCESS) return false;
    if (found > max_platforms) found = max_platforms;

    for (cl_uint p = 0u; p < found; p++) {
        cl_device_id cl_devices[max_gpu_devices];
        cl_uint      device_count = 0u;
        if (clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, max_gpu_devices, cl_devices,
                           &device_count) != CL_SUCCESS)
            continue;
        if (device_count > max_gpu_devices) device_count = max_gpu_devices;
        for (cl_uint d = 0u; d < device_count; d++) {
            uint8_t device_uuid[uuid_bytes] = {};
            /* A device whose driver does not carry cl_khr_device_uuid cannot
             * be matched to a Vulkan GPU, so it is not claimed for one. */
            if (clGetDeviceInfo(cl_devices[d], CL_DEVICE_UUID_KHR, uuid_bytes, device_uuid,
                                nullptr) != CL_SUCCESS)
                continue;
            if (memcmp(device_uuid, uuid, uuid_bytes) == 0) return true;
        }
    }
    return false;
}

}  // namespace av
