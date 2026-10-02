#include "executor/shim/loader_host_apis.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"

#include <string.h>
#include <vulkan/vulkan.h>

namespace av {

LoaderHostApis::LoaderHostApis()
    : instance_(nullptr), uuids_{}, sub_apis_{}, vulkan_kinds_{}, opencl_kinds_{}, opencl_devices_{},
      gl_displays_{}, d3d_runtime_(), d3d11_devices_{}, d3d12_devices_{}, d3d9_devices_{}, surfaces_{}, extensions_{}, count_(0u),
      opened_(false)
      {
          AV_SCOPE("LoaderHostApis::LoaderHostApis");
      }

LoaderHostApis::~LoaderHostApis()
{
    AV_SCOPE("LoaderHostApis::~LoaderHostApis");
    close();
}

void LoaderHostApis::open()
{
    AV_SCOPE("LoaderHostApis::open");
    if (opened_) return;
    opened_ = true;
    enumerate();
}

void LoaderHostApis::enumerate()
{
    AV_SCOPE("LoaderHostApis::enumerate");
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "av-executor";
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
        VkPhysicalDeviceDrmPropertiesEXT node = {};
        node.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
        VkPhysicalDeviceIDProperties identity = {};
        identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        identity.pNext = &node;
        VkPhysicalDeviceProperties2 device = {};
        device.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        device.pNext = &identity;
        vkGetPhysicalDeviceProperties2(devices[i], &device);

        uuids_[i] = DeviceUuid(identity.deviceUUID);
        render_minors_[i] = node.hasRender ? static_cast<int32_t>(node.renderMinor) : -1;
        sub_apis_[i] = AV_EXECUTOR_SUB_API_VULKAN;
        vulkan_kinds_[i] = kinds_of(devices[i]);
        opencl_kinds_[i] = 0u;
        if (opencl_devices_[i].find(uuids_[i].raw())) {
            sub_apis_[i] |= AV_EXECUTOR_SUB_API_OPENCL;
            /* A host pointer, on every device that runs OpenCL at all:
             * CL_MEM_USE_HOST_PTR is core rather than an extension, so a
             * buffer can always be made over the context's blob arena
             * (feature 052, T033). A dma-buf is still not offered - that
             * would be clCreateBufferWithProperties with an fd, which each
             * platform answers differently. */
            opencl_kinds_[i] = AV_BLOB_HOST_POINTER;
        }
        if (gl_displays_[i].open(uuids_[i].raw())) sub_apis_[i] |= AV_EXECUTOR_SUB_API_OPENGL;
    }
    /* After the loop: DXVK makes a Vulkan instance of its own, and asking it
     * about each GPU in turn is one factory and one enumeration. */
    if (d3d_runtime_.open()) {
        for (uint32_t i = 0u; i < found; i++)
            if (d3d11_devices_[i].open(d3d_runtime_, uuids_[i].raw())) {
                sub_apis_[i] |= AV_EXECUTOR_SUB_API_D3D;
                /* Direct3D 12 rides on the same sub-API: a session speaks
                 * whichever the guest's runtime asks for. */
                if (d3d12_devices_[i].open(d3d_runtime_, uuids_[i].raw())) d3d11_devices_[i].pair(&d3d12_devices_[i]);
                /* So does Direct3D 9, where DXVK's d3d9 is there too. */
                static_cast<void>(d3d9_devices_[i].open(d3d_runtime_, uuids_[i].raw()));
            }
    }
    count_ = found;
}

uint32_t LoaderHostApis::kinds_of(void *device)
{
    AV_SCOPE("LoaderHostApis::kinds_of");
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
    uint32_t kinds = 0u;
    for (uint32_t n = 0u; n < available; n++) {
        if (strcmp(extensions_[n].extensionName, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME) == 0)
            kinds |= AV_BLOB_HOST_POINTER;
        /* Both are offered where both are there, because which one works is
         * not a property of the extension list: a driver may advertise the
         * host-pointer import and still refuse every userptr whose pages are
         * file-backed, which guest RAM always is. The device tries the
         * dma-buf first and falls back (docs/testing.md). */
        if (strcmp(extensions_[n].extensionName, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME) == 0)
            kinds |= AV_BLOB_DMA_BUF;
    }
    return kinds;
}

void LoaderHostApis::close()
{
    AV_SCOPE("LoaderHostApis::close");
    for (uint32_t i = 0u; i < count_; i++) {
        gl_displays_[i].close();
        d3d12_devices_[i].close();
        d3d9_devices_[i].close();
        d3d11_devices_[i].close();
        opencl_devices_[i].forget();
    }
    if (instance_ != nullptr) vkDestroyInstance(static_cast<VkInstance>(instance_), nullptr);
    instance_ = nullptr;
    opened_ = false;
    count_ = 0u;
}

uint32_t LoaderHostApis::gpu_count()
{
    AV_SCOPE("LoaderHostApis::gpu_count");
    return count_;
}

void LoaderHostApis::gpu_uuid(uint32_t index, uint8_t *out)
{
    AV_SCOPE("LoaderHostApis::gpu_uuid");
    if (index >= count_ || out == nullptr) return;
    memcpy(out, uuids_[index].raw(), DeviceUuid::bytes);
}

int32_t LoaderHostApis::render_minor(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::render_minor");
    return index < count_ ? render_minors_[index] : -1;
}

uint32_t LoaderHostApis::sub_apis(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::sub_apis");
    return index < count_ ? sub_apis_[index] : 0u;
}

uint32_t LoaderHostApis::import_kinds(uint32_t index, uint32_t sub_api)
{
    AV_SCOPE("LoaderHostApis::import_kinds");
    if (index >= count_) return 0u;
    if (sub_api == AV_EXECUTOR_SUB_API_VULKAN) return vulkan_kinds_[index];
    if (sub_api == AV_EXECUTOR_SUB_API_OPENCL) return opencl_kinds_[index];
    /* OpenGL takes a host pointer as its context's region, not as a GL object
     * (feature 050), so any GPU it runs on takes one. */
    if (sub_api == AV_EXECUTOR_SUB_API_OPENGL && gl_displays_[index].open()) return AV_BLOB_HOST_POINTER;
    /* Direct3D takes its blob arena as a region, as OpenGL does. */
    if (sub_api == AV_EXECUTOR_SUB_API_D3D && d3d11_devices_[index].opened()) return AV_BLOB_HOST_POINTER;
    return 0u;
}

const GlDisplay *LoaderHostApis::gl_display(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::gl_display");
    return index < count_ && gl_displays_[index].open() ? &gl_displays_[index] : nullptr;
}

const OpenClDevice *LoaderHostApis::opencl_device(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::opencl_device");
    return index < count_ && opencl_devices_[index].device() != nullptr ? &opencl_devices_[index] : nullptr;
}

D3d11HostDevice *LoaderHostApis::d3d11_device(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::d3d11_device");
    return index < count_ && d3d11_devices_[index].opened() ? &d3d11_devices_[index] : nullptr;
}

D3d9HostDevice *LoaderHostApis::d3d9_device(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::d3d9_device");
    return index < count_ && d3d9_devices_[index].opened() ? &d3d9_devices_[index] : nullptr;
}

D3d12HostDevice *LoaderHostApis::d3d12_device(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::d3d12_device");
    return index < count_ && d3d12_devices_[index].opened() ? &d3d12_devices_[index] : nullptr;
}

SharedSurfaces *LoaderHostApis::surfaces(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::surfaces");
    return index < count_ ? &surfaces_[index] : nullptr;
}

}  // namespace av
