#include "device/execution/transport_command.hpp"

#include "device/execution/blob_import_command.hpp"

namespace av {

TransportCommand::TransportCommand(BlobImports &imports, const CommandHeader &header, const uint8_t *body)
    : imports_(imports), header_(header), body_(body) {}

Status TransportCommand::run(uint8_t *reply, size_t capacity, size_t &produced)
{
    produced = 0u;
    if (header_.id() != CommandHeader::nop_id) return BlobImportCommand(imports_, header_, body_).run(reply, capacity, produced);
    return header_.size() == CommandHeader::encoded_size ? Status::ok : Status::malformed;
}

}  // namespace av
