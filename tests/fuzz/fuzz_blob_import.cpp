/* A transport blob command as a hostile guest frames it, run over a context
 * whose process opened host blob 0, beside host blob 1 of another context:
 * whatever the bytes, the command answers or refuses the framing, an import
 * it answers names memory only when it made some, releasing that import
 * leaves the context as it was, and nothing reads or writes outside what it
 * was given (ASan).
 *
 * Input: u8 command (0 IMPORT_BLOB, 1 RELEASE_BLOB, anything else another
 * transport id), u8 reply capacity, then the body. */
#include "core/support/status.hpp"
#include "device/execution/blob_import_command.hpp"
#include "device/execution/blob_imports.hpp"
#include "harness/executor/import_rig.hpp"
#include "harness/host/host_violation.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t max_body = 64u;

std::uint32_t command_of(std::uint8_t selector)
{
    if (selector == 0u) return av::CommandHeader::import_blob_id;
    return selector == 1u ? av::CommandHeader::release_blob_id : selector;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < 2u) return 0;
    const std::uint32_t id = command_of(data[0]);
    const std::size_t   capacity = data[1] % 33u;
    const std::size_t   length = size - 2u < max_body ? size - 2u : max_body;
    std::uint8_t        body[max_body] = {};
    std::memcpy(body, data + 2, length);

    av::ImportRig rig;
    if (rig.create_host_blob(rig.blob(0u), av::ImportRig::context) != av::Status::ok ||
        rig.create_host_blob(rig.blob(1u), av::ImportRig::other_context) != av::Status::ok)
        __builtin_trap();
    std::uint8_t      reply[32] = {};
    std::size_t       produced = 0u;
    av::Status        ran = av::Status::invalid_state;
    const av::CommandHeader header(id, static_cast<std::uint32_t>(av::CommandHeader::encoded_size + length), 0u);
    rig.on_worker([&] { ran = av::BlobImportCommand(rig.imports(), header, body).run(reply, capacity, produced); });
    if (!rig.imports().invariants_hold()) __builtin_trap();
    if (ran != av::Status::ok) {
        if (produced != 0u || rig.imports().imports() != 0u) __builtin_trap();
        return 0;
    }
    const bool import = id == av::CommandHeader::import_blob_id;
    if (produced != (import ? av::BlobImportAnswer::encoded_size : 0u)) __builtin_trap();
    if (!import || capacity < av::BlobImportAnswer::encoded_size) return 0;
    av::BlobImportAnswer answer;
    if (answer.decode(reply, sizeof reply) != av::Status::ok) __builtin_trap();
    const bool imported = answer.status() == av::BlobImportAnswer::imported;
    if (rig.imports().imports() != (imported ? 1u : 0u) || rig.ops().live_imports() != rig.imports().imports())
        __builtin_trap();
    rig.on_worker([&] { rig.imports().release(answer.device_memory()); });
    if (rig.imports().imports() != 0u || rig.ops().live_imports() != 0u) __builtin_trap();
    return 0;
}
