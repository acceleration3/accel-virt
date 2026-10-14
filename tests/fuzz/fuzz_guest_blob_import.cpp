/* The host's answer to IMPORT_BLOB and RELEASE_BLOB as the guest reads it back
 * from its reply slot, attacker-chosen (feature 045): whatever the slot holds,
 * an import is either taken with memory and no refusal, refused with a status
 * and no memory, or held as nothing; and a release of what was taken names
 * exactly that memory and forgets it. */
#include "core/support/status.hpp"
#include "guest/transport/blob_import.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockTransport transport;
    av::BlobImport    import(transport);
    transport.set_reply(data, size);
    const av::Status imported = import.import(av::HostBlobId::first);
    require(import.invariants_hold());
    require((imported == av::Status::ok) == (import.memory() != 0u));
    if (imported == av::Status::invalid_parameter) require(import.host_status() != 0u);
    if (imported != av::Status::ok) return 0;
    require(size == av::BlobImportAnswer::encoded_size);
    const std::uint64_t memory = import.memory();
    /* The same bytes answer the release, and a release is answered with none. */
    require(import.release() == av::Status::malformed);
    require(transport.last_command() == av::CommandHeader::release_blob_id);
    std::uint64_t named = 0u;
    std::memcpy(&named, transport.last_payload(), sizeof named);
    require(named == memory && import.memory() == 0u);
    return 0;
}
