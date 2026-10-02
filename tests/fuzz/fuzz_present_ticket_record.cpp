/* The private data of a submitted packet read as a present's ticket, fed
 * attacker-chosen bytes: Adapter::submit_command_virtual reads it for every
 * packet, and a process writes the private data of its own. What decodes must
 * re-encode to exactly the bytes read and name an epoch and a ticket; a
 * refusal leaves the record as it was. */
#include "core/support/status.hpp"
#include "miniport/present/present_ticket_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::PresentTicketRecord record(5u, 6u);
    if (record.decode(data, size) != av::Status::ok) {
        if (record.epoch() != 5u || record.ticket() != 6u) __builtin_trap();
        return 0;
    }
    std::uint8_t again[av::PresentTicketRecord::encoded_size] = {};
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    if (record.epoch() == 0u || record.ticket() == 0u) __builtin_trap();
    return 0;
}
