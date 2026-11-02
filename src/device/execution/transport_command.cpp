#include "device/execution/transport_command.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/answer_piece_command.hpp"
#include "device/execution/blob_import_command.hpp"
#include "device/execution/lose_device_command.hpp"
#include "device/execution/shared_object_command.hpp"

namespace av {

TransportCommand::TransportCommand(BlobImports &imports, ContextExecutor &executor,
                                   const CommandHeader &header, const uint8_t *body)
    : imports_(imports), executor_(executor), header_(header), body_(body)
    {
        AV_SCOPE("TransportCommand::TransportCommand");
    }

Status TransportCommand::run(uint8_t *reply, size_t capacity, size_t &produced)
{
    AV_SCOPE("TransportCommand::run");
    produced = 0u;
    if (header_.id() == CommandHeader::nop_id)
        return header_.size() == CommandHeader::encoded_size ? Status::ok : Status::malformed;
    if (header_.id() == CommandHeader::import_blob_id || header_.id() == CommandHeader::release_blob_id)
        return BlobImportCommand(imports_, header_, body_).run(reply, capacity, produced);
    if (header_.id() == CommandHeader::lose_host_device_id)
        return LoseDeviceCommand(executor_, header_).run(reply, capacity, produced);
    if (header_.id() == CommandHeader::answer_piece_id)
        return AnswerPieceCommand(executor_, header_, body_).run(reply, capacity, produced);
    return SharedObjectCommand(executor_, header_, body_).run(reply, capacity, produced);
}

}
