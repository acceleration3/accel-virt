/* A context faulted on purpose: one synchronous call no host decoder takes.
 * By default a vkCreateInstance whose create-info stops after its structure
 * type - cut at its first field boundary, which reaches no host ICD; the
 * isolation scenario faults a context of its own with it (feature 044), the
 * Vulkan ICD an instance's for the probe (feature 045). A context of another
 * API skips a Vulkan command instead of faulting, so its fault names one of
 * its own commands, sent with no arguments (feature 050). */
#ifndef AV_CONTEXT_FAULT_HPP
#define AV_CONTEXT_FAULT_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ClientTransport;

class ContextFault {
public:
    explicit ContextFault(ClientTransport &transport);
    /* `command` with an empty payload. */
    ContextFault(ClientTransport &transport, uint32_t command);
    ContextFault(const ContextFault &) = delete;
    ContextFault &operator=(const ContextFault &) = delete;

    /* ok when the transport failed the call, which is the fault; conflict when
     * the host answered it. */
    [[nodiscard]] Status send();

private:
    ClientTransport &transport_;
    uint32_t         command_;
    /* The payload: the create-info's structure type, or nothing. */
    uint32_t         prefix_bytes_;
};

}  // namespace av

#endif  // AV_CONTEXT_FAULT_HPP
