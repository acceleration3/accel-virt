#include "core/support/status.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"

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
    av::GpuResponseReader reader;
    if (reader.decode(data, size) != av::Status::ok) return 0;
    if (size < 24u || reader.type() != field(data, 0u)) __builtin_trap();
    if (reader.type() == 0x1102u &&
        (size < 40u || reader.capset_id() != field(data, 24u) || reader.capset_max_size() != field(data, 32u)))
        __builtin_trap();
    if (reader.type() == 0x1103u) {
        std::uint8_t again[av::CapsetInfo::encoded_size] = {};
        if (reader.capset().encode(again, sizeof again) != av::Status::ok) __builtin_trap();
        if (size < 24u + sizeof again || std::memcmp(again, data + 24u, sizeof again) != 0) __builtin_trap();
    }
    return 0;
}
