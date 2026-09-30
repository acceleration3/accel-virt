#include "proto/submission/present_ticket_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PresentTicketRecord::PresentTicketRecord() : epoch_(0u), ticket_(0u)
{
    AV_SCOPE("PresentTicketRecord::PresentTicketRecord");
}

PresentTicketRecord::PresentTicketRecord(uint32_t epoch, uint64_t ticket) : epoch_(epoch), ticket_(ticket)
{
    AV_SCOPE("PresentTicketRecord::PresentTicketRecord");
}

uint32_t PresentTicketRecord::epoch() const
{
    AV_SCOPE("PresentTicketRecord::epoch");
    return epoch_;
}

uint64_t PresentTicketRecord::ticket() const
{
    AV_SCOPE("PresentTicketRecord::ticket");
    return ticket_;
}

Status PresentTicketRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("PresentTicketRecord::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(epoch_);
    writer.write_u64(ticket_);
    return writer.status();
}

Status PresentTicketRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("PresentTicketRecord::decode");
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint32_t epoch = reader.read_u32();
    const uint64_t ticket = reader.read_u64();
    if (reader.status() != Status::ok) return Status::malformed;
    if (found_magic != magic || epoch == 0u || ticket == 0u) return Status::malformed;
    epoch_ = epoch;
    ticket_ = ticket;
    return Status::ok;
}

}  // namespace av
