#ifndef AV_D3D_DEVICE_COOKIE_HPP
#define AV_D3D_DEVICE_COOKIE_HPP

#include <stdint.h>

namespace av::d3d {

class UmdDevice;

class DeviceCookie {
public:
    static constexpr uint64_t magic = 0x4156440000000001ull;

    DeviceCookie();
    DeviceCookie(const DeviceCookie &) = delete;
    DeviceCookie &operator=(const DeviceCookie &) = delete;

    void       write(UmdDevice &device);
    UmdDevice *named() const;
    void       clear();

    bool invariants_hold() const;

private:
    uint64_t   magic_;
    UmdDevice *device_;
};

}

#endif
