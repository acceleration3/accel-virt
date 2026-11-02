#ifndef AV_PRESENT_TICKET_RECORD_HPP
#define AV_PRESENT_TICKET_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PresentTicketRecord {
public:
    static constexpr size_t encoded_size = 16u;

    PresentTicketRecord();
    PresentTicketRecord(uint32_t epoch, uint64_t ticket);

    uint32_t epoch() const;
    uint64_t ticket() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00545641u;

    uint32_t epoch_;
    uint64_t ticket_;
};

}

#endif
