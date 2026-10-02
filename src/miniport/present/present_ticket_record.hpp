/* The private data the miniport writes beside a present's DMA buffer when the
 * device's present engine moves the present's pixels: the ticket they were
 * queued under, and which registration of the engine's page counted it. The
 * packet completes only once the engine has finished that ticket, so nothing
 * reads the destination before its pixels are there. DdiPresent writes it and
 * SubmitCommandVirtual reads it back - WDDM's own channel between the two. */
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

    /* too_large for less room than the record. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for fewer bytes than the record, another magic, or an epoch or
     * ticket of 0: private data that is anything else. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00545641u;   /* "AVT" */

    uint32_t epoch_;
    uint64_t ticket_;
};

}  // namespace av

#endif  // AV_PRESENT_TICKET_RECORD_HPP
