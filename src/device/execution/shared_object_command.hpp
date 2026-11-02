#ifndef AV_SHARED_OBJECT_COMMAND_HPP
#define AV_SHARED_OBJECT_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextExecutor;
class SharedObjectRequest;

class SharedObjectCommand {
public:
    SharedObjectCommand(ContextExecutor &executor, const CommandHeader &header, const uint8_t *body);

    [[nodiscard]] Status run(uint8_t *reply, size_t capacity, size_t &produced);

private:
    uint32_t             refusal_of(Status answered) const;
    [[nodiscard]] Status answer(uint32_t status, uint64_t wire_handle, uint8_t *reply, size_t capacity,
                                size_t &produced);
    size_t               body_size() const;

    ContextExecutor &executor_;
    CommandHeader    header_;
    const uint8_t   *body_;
};

}

#endif
