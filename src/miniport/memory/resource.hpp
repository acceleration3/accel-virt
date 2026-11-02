#ifndef AV_RESOURCE_HPP
#define AV_RESOURCE_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Platform;

class Resource {
public:
    explicit Resource(Platform &platform);
    Resource(const Resource &) = delete;
    Resource &operator=(const Resource &) = delete;

    void                 took_one();
    [[nodiscard]] Status released_one();
    uint32_t             allocations() const;

private:
    Platform &platform_;
    uint32_t  allocations_;
};

}

#endif
