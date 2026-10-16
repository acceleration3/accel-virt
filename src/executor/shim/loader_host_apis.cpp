#include "executor/shim/loader_host_apis.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"
#include "executor/shim/physical_device_identity.hpp"

#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

namespace av {

LoaderHostApis::LoaderHostApis()
    : instance_(nullptr), uuids_{}, sub_apis_{}, opencl_devices_{},
      gl_displays_{}, d3d_runtime_(), d3d11_devices_{}, d3d12_devices_{}, d3d9_devices_{}, heap_(), surfaces_{}, share_sink_(stderr),
      share_lines_(share_sink_, "virtio-gpu-av: shares: "), shares_{}, count_(0u), opened_(false)
{
    AV_SCOPE("LoaderHostApis::LoaderHostApis");
    for (ObjectStorage<HostShares> &shares : shares_) static_cast<void>(shares.construct(heap_, share_lines_));
    for (ObjectStorage<SharedSurfaces> &surfaces : surfaces_) static_cast<void>(surfaces.construct(heap_));
    for (uint32_t i = 0u; i < HostGpuSet::max_gpus; i++)
        static_cast<void>(exporters_[i].construct(compositors_[i]));
}

LoaderHostApis::~LoaderHostApis()
{
    AV_SCOPE("LoaderHostApis::~LoaderHostApis");
    close();
    for (ObjectStorage<SharedSurfaces> &surfaces : surfaces_) surfaces.destroy();
    for (ObjectStorage<HostShares> &shares : shares_) shares.destroy();
    for (ObjectStorage<CompositorMemory> &exporter : exporters_) exporter.destroy();
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
        const PhysicalDeviceIdentity identity(handle, devices[i]);
        uuids_[i] = identity.uuid();
        render_minors_[i] = identity.render_minor();
        sub_apis_[i] = AV_EXECUTOR_SUB_API_VULKAN;
        if (opencl_devices_[i].find(uuids_[i].raw())) sub_apis_[i] |= AV_EXECUTOR_SUB_API_OPENCL;
        /* Every GPU's one source of the memory a guest maps
         * (docs/host-memory.md): its compositing context, which also owns
         * what the APIs hand each other. Opened now, and allocating nothing
         * until a guest asks. */
        if (compositors_[i].open(uuids_[i].raw()))
            static_cast<void>(sources_[i].construct(exporters_[i].object(), heap_));
        if (gl_displays_[i].open(uuids_[i].raw())) sub_apis_[i] |= AV_EXECUTOR_SUB_API_OPENGL;
    }
    /* After the loop: DXVK makes a Vulkan instance of its own, and asking it
     * about each GPU in turn is one factory and one enumeration. */
    if (d3d_runtime_.open()) {
        for (uint32_t i = 0u; i < found; i++)
            if (d3d11_devices_[i].open(d3d_runtime_, uuids_[i].raw(), shares_[i].object())) {
                sub_apis_[i] |= AV_EXECUTOR_SUB_API_D3D;
                if (compositors_[i].opened()) d3d11_devices_[i].use_compositor(&compositors_[i]);
                /* Direct3D 12 rides on the same sub-API: a session speaks
                 * whichever the guest's runtime asks for. */
                if (d3d12_devices_[i].open(d3d_runtime_, uuids_[i].raw())) d3d11_devices_[i].pair(&d3d12_devices_[i]);
                /* So does Direct3D 9, where DXVK's d3d9 is there too. */
                static_cast<void>(d3d9_devices_[i].open(d3d_runtime_, uuids_[i].raw()));
            }
    }
    count_ = found;
}

void LoaderHostApis::close()
{
    AV_SCOPE("LoaderHostApis::close");
    for (uint32_t i = 0u; i < count_; i++) {
        gl_displays_[i].close();
        d3d12_devices_[i].close();
        d3d9_devices_[i].close();
        d3d11_devices_[i].close();
        d3d11_devices_[i].use_compositor(nullptr);
        sources_[i].destroy();
        compositors_[i].close();
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

HostMemorySource *LoaderHostApis::memory_source(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::memory_source");
    return index < count_ && sources_[index].constructed() ? &sources_[index].object() : nullptr;
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
    return index < count_ && d3d11_devices_[index].placed() ? &d3d11_devices_[index] : nullptr;
}

D3d9HostDevice *LoaderHostApis::d3d9_device(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::d3d9_device");
    return index < count_ && d3d9_devices_[index].opened() ? &d3d9_devices_[index] : nullptr;
}

D3d12HostDevice *LoaderHostApis::d3d12_device(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::d3d12_device");
    return index < count_ && d3d12_devices_[index].placed() ? &d3d12_devices_[index] : nullptr;
}

SharedSurfaces *LoaderHostApis::surfaces(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::surfaces");
    return index < count_ ? &surfaces_[index].object() : nullptr;
}

HostShares *LoaderHostApis::shares(uint32_t index)
{
    AV_SCOPE("LoaderHostApis::shares");
    return index < count_ ? &shares_[index].object() : nullptr;
}

}  // namespace av
