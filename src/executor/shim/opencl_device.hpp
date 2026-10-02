/* One host GPU as OpenCL names it: the platform and the device whose
 * CL_DEVICE_UUID_KHR is that GPU's Vulkan device UUID.
 *
 * cl_khr_device_uuid defines the two to be the same sixteen bytes, so the APIs
 * agree on which GPU is which without a guess from a name. Found once, at
 * realize; a guest context is then shown this platform and device and no other
 * (feature 044, T034). The first live run's OpenCL scenario took the first GPU
 * any platform offered - NVIDIA's, while the device was bound to Intel's. */
#ifndef AV_OPENCL_DEVICE_HPP
#define AV_OPENCL_DEVICE_HPP

#include <stdint.h>

namespace av {

class OpenClDevice {
public:

    OpenClDevice();
    OpenClDevice(const OpenClDevice &) = delete;
    OpenClDevice &operator=(const OpenClDevice &) = delete;

    /* Finds the GPU device `uuid` names on any platform. False, holding
     * nothing, for a host with no OpenCL, no such device, or a driver without
     * cl_khr_device_uuid. Always a CL_DEVICE_TYPE_GPU device. */
    [[nodiscard]] bool find(const uint8_t *uuid);
    void               forget();

    /* A cl_platform_id and a cl_device_id; null until found. */
    void *platform() const;
    void *device() const;

private:
    /* This host has three OpenCL platforms and two GPUs; neither number is
     * one anybody will meet. */
    static constexpr uint32_t max_platforms = 16u;
    static constexpr uint32_t max_gpu_devices = 16u;

    void *platform_;
    void *device_;
};

}  // namespace av

#endif  // AV_OPENCL_DEVICE_HPP
