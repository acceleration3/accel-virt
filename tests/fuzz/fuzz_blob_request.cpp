#include "core/support/status.hpp"
#include "proto/virtio_gpu/blob_request.hpp"
#include "proto/virtio_gpu/create_blob_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

std::uint64_t field(const std::uint8_t *data, std::size_t offset, std::size_t width)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, data + offset, width);
    return value;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader create_reader(data, size);
    av::CreateBlobRequest create;
    if (create.decode(create_reader) == av::Status::ok) {
        if (size < av::CreateBlobRequest::fixed_size || create_reader.offset() != av::CreateBlobRequest::fixed_size)
            __builtin_trap();
        if (create.resource_id() != field(data, 24u, 4u) || create.memory() != field(data, 28u, 4u) ||
            create.flags() != field(data, 32u, 4u) || create.entries() != field(data, 36u, 4u) ||
            create.blob_id() != field(data, 40u, 8u) || create.size() != field(data, 48u, 8u))
            __builtin_trap();
    }
    av::WireReader map_reader(data, size);
    av::BlobRequest map;
    if (map.decode_map(map_reader) == av::Status::ok &&
        (size < 40u || map.resource_id() != field(data, 24u, 4u) || map.offset() != field(data, 32u, 8u)))
        __builtin_trap();
    av::WireReader resource_reader(data, size);
    av::BlobRequest resource;
    if (resource.decode_resource(resource_reader) == av::Status::ok &&
        (size < 32u || resource.resource_id() != field(data, 24u, 4u)))
        __builtin_trap();
    return 0;
}
