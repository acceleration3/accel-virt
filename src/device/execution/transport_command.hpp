#ifndef AV_TRANSPORT_COMMAND_HPP
#define AV_TRANSPORT_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobImports;
class ContextExecutor;

class TransportCommand {
public:
    TransportCommand(BlobImports &imports, ContextExecutor &executor, const CommandHeader &header,
                     const uint8_t *body);

    [[nodiscard]] Status run(uint8_t *reply, size_t capacity, size_t &produced);

private:
    BlobImports     &imports_;
    ContextExecutor &executor_;
    CommandHeader  header_;
    const uint8_t *body_;
};

}

#endif
