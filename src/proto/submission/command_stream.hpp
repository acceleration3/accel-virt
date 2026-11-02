#ifndef AV_COMMAND_STREAM_HPP
#define AV_COMMAND_STREAM_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CommandHeader;

class CommandStream {
public:
    CommandStream(const uint8_t *bytes, size_t length);

    [[nodiscard]] Status next(CommandHeader &header, const uint8_t *&payload);
    size_t               offset() const;

    bool invariants_hold() const;

private:
    const uint8_t *bytes_;
    size_t         length_;
    size_t         offset_;
    bool           malformed_;
};

}

#endif
