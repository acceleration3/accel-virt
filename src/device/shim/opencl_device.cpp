#include "device/shim/opencl_device.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/context/device_uuid.hpp"

#include <CL/cl.h>
#include <CL/cl_ext.h>

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
    cl_platform_id platforms[max_platforms];
    cl_uint        found = 0u;
    if (clGetPlatformIDs(max_platforms, platforms, &found) != CL_SUCCESS) return false;
    if (found > max_platforms) found = max_platforms;

    for (cl_uint p = 0u; p < found && device_ == nullptr; p++) {
        cl_device_id devices[max_gpu_devices];
        cl_uint      device_count = 0u;
        if (clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, max_gpu_devices, devices, &device_count) !=
            CL_SUCCESS)
            continue;
        if (device_count > max_gpu_devices) device_count = max_gpu_devices;
        for (cl_uint d = 0u; d < device_count && device_ == nullptr; d++) {
            uint8_t device_uuid[DeviceUuid::bytes] = {};
            /* A device whose driver does not carry cl_khr_device_uuid cannot
             * be matched to a Vulkan GPU, so it is not claimed for one. */
            if (clGetDeviceInfo(devices[d], CL_DEVICE_UUID_KHR, DeviceUuid::bytes, device_uuid, nullptr) != CL_SUCCESS)
                continue;
            if (!DeviceUuid(device_uuid).equals(uuid)) continue;
            platform_ = platforms[p];
            device_ = devices[d];
        }
    }
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
