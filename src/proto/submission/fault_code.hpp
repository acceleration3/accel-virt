/* Why the device faulted a context (docs/protocol.md): the fence page's
 * fault_code, read by the guest's TDR report. */
#ifndef AV_FAULT_CODE_HPP
#define AV_FAULT_CODE_HPP

#include <stdint.h>

namespace av {

enum class FaultCode : uint32_t {
    none = 0u,
    ring_header = 1u,       /* malformed, or its order changed */
    ring_tail = 2u,         /* backwards, or more than the ring ahead of head */
    batch_header = 3u,      /* malformed, or cannot fit the ring */
    batch_order = 4u,       /* a fence not greater than the last */
    command_framing = 5u,   /* commands framed wrong, or an answer asked for with no reply slot */
    undecodable = 6u,       /* a payload the executor cannot decode */
    device_lost = 7u,       /* the host API lost its device */
    no_executor = 8u,       /* the context's executor could not be created */
    broken_executor = 9u,   /* the executor library answered a value the ABI does not define */
};

}  // namespace av

#endif  // AV_FAULT_CODE_HPP
