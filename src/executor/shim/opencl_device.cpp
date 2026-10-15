#include "executor/shim/opencl_device.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/opencl/cl_platform_choice.hpp"
#include "proto/context/device_uuid.hpp"

#include <CL/cl.h>
#include <CL/cl_ext.h>
#include <stdlib.h>

namespace av {

OpenClDevice::OpenClDevice() : platform_(nullptr), device_(nullptr)
{
    AV_SCOPE("OpenClDevice::OpenClDevice");
}

bool OpenClDevice::find(const uint8_t *uuid)
{
    AV_SCOPE("OpenClDevice::find");
    forget();
    if (uuid == nullptr) return false;
    /* Mesa's rusticl offers no device of a driver it was not told to enable,
     * and a desktop's QEMU is started with nothing telling it: on a host
     * whose only OpenCL is Mesa's, the guest saw a platform with no GPU. The
     * hardware drivers the host's GPUs run on are enabled before the first
     * call loads it, unless whoever started QEMU chose otherwise. */
    setenv("RUSTICL_ENABLE", rusticl_drivers, 0);
    cl_platform_id platforms[max_platforms];
    cl_uint        found = 0u;
    if (clGetPlatformIDs(max_platforms, platforms, &found) != CL_SUCCESS) return false;
    if (found > max_platforms) found = max_platforms;

    /* Every platform is asked, not just the first that has the GPU: which
     * one runs it is ClPlatformChoice's to say. */
    ClPlatformChoice choice;
    for (cl_uint p = 0u; p < found; p++) {
        cl_device_id devices[max_gpu_devices];
        cl_uint      device_count = 0u;
        if (clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, max_gpu_devices, devices, &device_count) !=
            CL_SUCCESS)
            continue;
        if (device_count > max_gpu_devices) device_count = max_gpu_devices;
        char name[ClPlatformChoice::name_bytes] = {};
        if (clGetPlatformInfo(platforms[p], CL_PLATFORM_NAME, sizeof name, name, nullptr) != CL_SUCCESS) name[0] = '\0';
        name[sizeof name - 1u] = '\0';
        for (cl_uint d = 0u; d < device_count; d++) {
            uint8_t device_uuid[DeviceUuid::bytes] = {};
            /* A device whose driver does not carry cl_khr_device_uuid cannot
             * be matched to a Vulkan GPU, so it is not claimed for one. */
            if (clGetDeviceInfo(devices[d], CL_DEVICE_UUID_KHR, DeviceUuid::bytes, device_uuid, nullptr) != CL_SUCCESS)
                continue;
            if (!DeviceUuid(device_uuid).equals(uuid)) continue;
            choice.consider(platforms[p], devices[d], name);
        }
    }
    platform_ = choice.platform();
    device_ = choice.device();
    return device_ != nullptr;
}

void OpenClDevice::forget()
{
    AV_SCOPE("OpenClDevice::forget");
    platform_ = nullptr;
    device_ = nullptr;
}

void *OpenClDevice::platform() const
{
    AV_SCOPE("OpenClDevice::platform");
    return platform_;
}

void *OpenClDevice::device() const
{
    AV_SCOPE("OpenClDevice::device");
    return device_;
}

}  // namespace av
