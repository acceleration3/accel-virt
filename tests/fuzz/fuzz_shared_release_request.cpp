#include "core/support/status.hpp"
#include "proto/virtio_gpu/shared_release_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader           reader(data, size);
    av::SharedReleaseRequest release;
    if (release.decode(reader) != av::Status::ok) {
        if (release.object_id() != 0u) __builtin_trap();
        return 0;
    }
    std::uint64_t id = 0u;
    std::memcpy(&id, data + 24u, sizeof id);
    if (size < av::SharedReleaseRequest::encoded_size || reader.offset() != av::SharedReleaseRequest::encoded_size ||
        release.object_id() != id || id == 0u)
        __builtin_trap();
    return 0;
}
