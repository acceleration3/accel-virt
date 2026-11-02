#ifndef AV_CONTEXT_FAULT_HPP
#define AV_CONTEXT_FAULT_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ClientTransport;

class ContextFault {
public:
    explicit ContextFault(ClientTransport &transport);
    ContextFault(ClientTransport &transport, uint32_t command);
    ContextFault(const ContextFault &) = delete;
    ContextFault &operator=(const ContextFault &) = delete;

    [[nodiscard]] Status send();

private:
    ClientTransport &transport_;
    uint32_t         command_;
    uint32_t         prefix_bytes_;
};

}

#endif
