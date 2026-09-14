#include "device/execution/transport_command.hpp"

namespace av {

TransportCommand::TransportCommand(const CommandHeader &header) : header_(header) {}

Status TransportCommand::run() const
{
    if (header_.id() != CommandHeader::nop_id) return Status::not_found;
    return header_.size() == CommandHeader::encoded_size ? Status::ok : Status::malformed;
}

}  // namespace av
