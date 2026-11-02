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
