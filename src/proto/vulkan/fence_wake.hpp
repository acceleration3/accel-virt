#ifndef AV_FENCE_WAKE_HPP
#define AV_FENCE_WAKE_HPP

#include <stdint.h>

namespace av::vkproto {

class FenceWake {
public:
    FenceWake();

    uint64_t arm_timeout() const;
    bool     arms(uint64_t timeout) const;

private:
    uint64_t arm_timeout_;
};

}

#endif
