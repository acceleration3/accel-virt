#include "core/support/status.hpp"
#include "proto/virtio_gpu/capset_request.hpp"
#include "proto/virtio_gpu/context_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

std::uint32_t field(const std::uint8_t *data, std::size_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, data + offset, sizeof value);
    return value;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader destroy_reader(data, size);
    av::ContextRequest destroy;
    if (destroy.decode_destroy(destroy_reader) == av::Status::ok &&
        (size < 24u || destroy_reader.offset() != 24u || destroy.context_id() != field(data, 16u)))
        __builtin_trap();
    av::WireReader create_reader(data, size);
    av::ContextRequest create;
    if (create.decode_create(create_reader) == av::Status::ok &&
        (size < 96u || create_reader.offset() != 96u || create.context_id() != field(data, 16u) ||
         create.name_length() != field(data, 24u) || create.context_init() != field(data, 28u)))
        __builtin_trap();
    av::WireReader submit_reader(data, size);
    av::ContextRequest submit;
    if (submit.decode_submit(submit_reader) == av::Status::ok &&
        (size < 32u || submit_reader.offset() != 32u || submit.command_size() != field(data, 24u)))
        __builtin_trap();
    av::WireReader info_reader(data, size);
    av::CapsetRequest info;
    if (info.decode_info(info_reader) == av::Status::ok && (size < 32u || info.index() != field(data, 24u)))
        __builtin_trap();
    av::WireReader capset_reader(data, size);
    av::CapsetRequest capset;
    if (capset.decode_capset(capset_reader) == av::Status::ok &&
        (size < 32u || capset.capset_id() != field(data, 24u) || capset.capset_version() != field(data, 28u)))
        __builtin_trap();
    return 0;
}
