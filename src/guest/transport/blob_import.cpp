#include "guest/transport/blob_import.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/client/sync_call_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

BlobImport::BlobImport(ClientTransport &transport) : transport_(transport), memory_(0u), host_status_(0u)
{
    AV_SCOPE("BlobImport::BlobImport");
}

Status BlobImport::import(uint32_t resource_id)
{
    AV_SCOPE("BlobImport::import");
    if (memory_ != 0u) return Status::invalid_state;
    uint8_t body[BlobImportRequest::encoded_size] = {};
    static_cast<void>(BlobImportRequest(resource_id).encode(body, sizeof body));
    WireWriter request = transport_.request_writer();
    request.write(body, sizeof body);
    const SyncCallScope call(transport_);
    WireReader          answer(nullptr, 0u);
    if (transport_.send_sync(CommandHeader::import_blob_id, request, answer) != TransportResult::ok)
        return Status::invalid_state;
    return judge(answer);
}

/* An answer of any other length than the contract's is no answer, even one
 * whose leading bytes would decode. */
Status BlobImport::judge(WireReader &answer)
{
    AV_SCOPE("BlobImport::judge");
    const size_t length = answer.remaining();
    if (length == 0u) return Status::not_supported;
    if (length != BlobImportAnswer::encoded_size) return Status::malformed;
    BlobImportAnswer decoded;
    uint8_t          bytes[BlobImportAnswer::encoded_size] = {};
    answer.copy_bytes(bytes, sizeof bytes);
    if (decoded.decode(bytes, sizeof bytes) != Status::ok) return Status::malformed;
    host_status_ = decoded.status();
    memory_ = decoded.device_memory();
    if (host_status_ == BlobImportAnswer::imported) return Status::ok;
    return host_status_ == BlobImportAnswer::no_room ? Status::no_memory : Status::invalid_parameter;
}

Status BlobImport::release()
{
    AV_SCOPE("BlobImport::release");
    if (memory_ == 0u) return Status::ok;
    WireWriter request = transport_.request_writer();
    request.write_u64(memory_);
    memory_ = 0u;
    const SyncCallScope call(transport_);
    WireReader          answer(nullptr, 0u);
    if (transport_.send_sync(CommandHeader::release_blob_id, request, answer) != TransportResult::ok)
        return Status::invalid_state;
    return answer.remaining() == 0u ? Status::ok : Status::malformed;
}

void BlobImport::forget()
{
    AV_SCOPE("BlobImport::forget");
    memory_ = 0u;
}

uint64_t BlobImport::memory() const
{
    AV_SCOPE("BlobImport::memory");
    return memory_;
}
uint32_t BlobImport::host_status() const
{
    AV_SCOPE("BlobImport::host_status");
    return host_status_;
}

/* The host either took the import or refused it, never both. */
bool BlobImport::invariants_hold() const
{
    AV_SCOPE("BlobImport::invariants_hold");
    return memory_ == 0u || host_status_ == 0u;
}

}  // namespace av
