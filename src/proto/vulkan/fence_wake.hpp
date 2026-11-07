#ifndef AV_FENCE_WAKE_HPP
#define AV_FENCE_WAKE_HPP

#include <stdint.h>

namespace av::vkproto {

class FenceWake {
public:
    static constexpr uint64_t arm_timeout = ~uint64_t{0} - 1u;
};

}

#endif
