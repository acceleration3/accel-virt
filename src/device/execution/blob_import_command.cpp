#include "device/execution/blob_import_command.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/blob_imports.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

BlobImportCommand::BlobImportCommand(BlobImports &imports, const CommandHeader &header, const uint8_t *body)
    : imports_(imports), header_(header), body_(body)
    {
        AV_SCOPE("BlobImportCommand::BlobImportCommand");
    }

Status BlobImportCommand::run(uint8_t *reply, size_t capacity, size_t &produced)
{
    AV_SCOPE("BlobImportCommand::run");
    produced = 0u;
    switch (header_.id()) {
    case CommandHeader::import_blob_id: return import(reply, capacity, produced);
    case CommandHeader::release_blob_id: return release();
    default: return Status::not_found;
    }
}

size_t BlobImportCommand::body_size() const
{
    AV_SCOPE("BlobImportCommand::body_size");
    return header_.size() - CommandHeader::encoded_size;
}

Status BlobImportCommand::import(uint8_t *reply, size_t capacity, size_t &produced)
{
    AV_SCOPE("BlobImportCommand::import");
    BlobImportRequest request;
    const Status      decoded = request.decode(body_, body_size());
    if (decoded == Status::malformed) return Status::malformed;
    uint64_t     device_memory = 0u;
    const Status imported =
        decoded == Status::ok ? imports_.import(request.resource_id(), device_memory) : Status::invalid_parameter;
    uint32_t answer = BlobImportAnswer::executor_refused;
    switch (imported) {
    case Status::ok: answer = BlobImportAnswer::imported; break;
    case Status::invalid_parameter: answer = BlobImportAnswer::refused_request; break;
    case Status::not_found: answer = BlobImportAnswer::unknown_resource; break;
    case Status::no_memory: answer = BlobImportAnswer::no_room; break;
    default: break;
    }
    produced = BlobImportAnswer::encoded_size;
    static_cast<void>(BlobImportAnswer(answer, device_memory).encode(reply, capacity));
    return Status::ok;
}

Status BlobImportCommand::release()
{
    AV_SCOPE("BlobImportCommand::release");
    if (body_size() != release_body_size) return Status::malformed;
    WireReader reader(body_, release_body_size);
    imports_.release(reader.read_u64());
    return Status::ok;
}

}
