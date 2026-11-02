#ifndef AV_HW_QUEUE_HPP
#define AV_HW_QUEUE_HPP

#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class Context;

class HwQueue {
public:
    HwQueue(Platform &platform, Context &context, uint64_t *progress);
    HwQueue(const HwQueue &) = delete;
    HwQueue &operator=(const HwQueue &) = delete;

    Context  &context() const;
    uint64_t *progress() const;

private:
    Platform &platform_;
    Context  &context_;
    uint64_t *progress_;
};

}

#endif
