/* A context faulted on purpose: a vkCreateInstance whose create-info stops
 * after its structure type - cut at its first field boundary, which no host
 * decoder takes for an instance's and which reaches no host ICD - sent as one
 * synchronous call on the transport. The isolation scenario faults a context
 * of its own with it (feature 044), the Vulkan ICD an instance's for the probe
 * (feature 045). */
#ifndef AV_CONTEXT_FAULT_HPP
#define AV_CONTEXT_FAULT_HPP

#include "core/support/status.hpp"

namespace av {

class ClientTransport;

class ContextFault {
public:
    explicit ContextFault(ClientTransport &transport);
    ContextFault(const ContextFault &) = delete;
    ContextFault &operator=(const ContextFault &) = delete;

    /* ok when the transport failed the call, which is the fault; conflict when
     * the host answered it. */
    [[nodiscard]] Status send();

private:
    ClientTransport &transport_;
};

}  // namespace av

#endif  // AV_CONTEXT_FAULT_HPP
