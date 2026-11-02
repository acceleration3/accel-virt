#ifndef AV_HOST_POLL_HPP
#define AV_HOST_POLL_HPP

#include <stdint.h>

namespace av {

enum class HostPoll : uint8_t { idle, pending, lost };

}

#endif
