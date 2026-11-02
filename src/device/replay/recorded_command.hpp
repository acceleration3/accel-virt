#ifndef AV_RECORDED_COMMAND_HPP
#define AV_RECORDED_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RecordedCommand {
public:
    RecordedCommand();

    [[nodiscard]] Status decode(const uint8_t *bytes, uint32_t length, uint32_t offset);

    const CommandHeader &header() const;
    const uint8_t       *payload() const;
    uint32_t             payload_length() const;
    uint32_t             offset() const;
    uint32_t             next_offset() const;
    const char          *name() const;
    const char          *api_name() const;

private:
    CommandHeader  header_;
    const uint8_t *payload_;
    uint32_t       payload_length_;
    uint32_t       offset_;
};

}

#endif
