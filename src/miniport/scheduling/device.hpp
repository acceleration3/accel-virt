#ifndef AV_DEVICE_HPP
#define AV_DEVICE_HPP

#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class Adapter;

class Device {
public:
    Device(Platform &platform, Adapter &adapter, void *runtime_handle, uint32_t flags, void *process);
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    Adapter &adapter() const;
    void    *runtime_handle() const;
    uint32_t flags() const;
    void    *process() const;

private:
    Platform &platform_;
    Adapter  &adapter_;
    void     *runtime_handle_;
    uint32_t  flags_;
    void     *process_;
};

}

#endif
