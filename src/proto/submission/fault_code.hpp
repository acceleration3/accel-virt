#ifndef AV_FAULT_CODE_HPP
#define AV_FAULT_CODE_HPP

#include <stdint.h>

namespace av {

enum class FaultCode : uint32_t {
    none = 0u,
    ring_header = 1u,
    ring_tail = 2u,
    batch_header = 3u,
    batch_order = 4u,
    command_framing = 5u,
    undecodable = 6u,
    device_lost = 7u,
    no_executor = 8u,
    broken_executor = 9u,
};

}

#endif
