#ifndef AV_OPENCL_DEVICE_HPP
#define AV_OPENCL_DEVICE_HPP

#include <stdint.h>

namespace av {

class OpenClDevice {
public:

    OpenClDevice();
    OpenClDevice(const OpenClDevice &) = delete;
    OpenClDevice &operator=(const OpenClDevice &) = delete;

    [[nodiscard]] bool find(const uint8_t *uuid);
    void               forget();

    void *platform() const;
    void *device() const;

private:
    static constexpr uint32_t max_platforms = 16u;
    static constexpr const char *rusticl_drivers = "iris,radeonsi";
    static constexpr uint32_t max_gpu_devices = 16u;

    void *platform_;
    void *device_;
};

}

#endif
