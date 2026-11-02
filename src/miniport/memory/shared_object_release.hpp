#ifndef AV_SHARED_OBJECT_RELEASE_HPP
#define AV_SHARED_OBJECT_RELEASE_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class VirtioGpuDevice;

class SharedObjectRelease {
public:
    explicit SharedObjectRelease(Platform &platform);
    SharedObjectRelease(const SharedObjectRelease &) = delete;
    SharedObjectRelease &operator=(const SharedObjectRelease &) = delete;

    [[nodiscard]] Status send(VirtioGpuDevice &device, uint64_t object_id);

private:
    static constexpr size_t request_bytes = 32u;
    static constexpr size_t answer_bytes = 24u;

    Platform &platform_;
    uint8_t   request_[request_bytes];
    uint8_t   answer_[answer_bytes];
};

}

#endif
