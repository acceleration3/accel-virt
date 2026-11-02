#ifndef AV_COMPOSE_ORDER_HPP
#define AV_COMPOSE_ORDER_HPP

#include "executor/shim/vulkan_timeline.hpp"

#include <stdint.h>

namespace av {

class OwnedFd;
class D3d11HostDevice;

class ComposeOrder {
public:
    ComposeOrder();
    ComposeOrder(const ComposeOrder &) = delete;
    ComposeOrder &operator=(const ComposeOrder &) = delete;

    [[nodiscard]] bool connect(D3d11HostDevice &device11, OwnedFd &descriptor);
    bool               connected() const;
    uint64_t           next() const;
    uint64_t           value() const;
    [[nodiscard]] bool presented();
    void               release();

private:
    D3d11HostDevice *device11_;
    VulkanTimeline   waited_;
    uint64_t         value_;
};

}

#endif
