#include "core/support/status.hpp"
#include "guest/transport/reply_reader.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

std::uint64_t field(const std::uint8_t *data, std::size_t size, std::size_t offset, std::size_t width)
{
    std::uint64_t value = 0u;
    if (offset + width <= size) std::memcpy(&value, data + offset, width);
    return value;
}

void probe(const std::uint8_t *data, std::size_t size, std::uint64_t expected)
{
    av::ReplyReader reader(data, size);
    av::WireReader  answer(nullptr, 0u);
    const av::Status status = reader.read(expected, answer);
    if (status == av::Status::ok) {
        if (size < av::ReplyHeader::encoded_size) __builtin_trap();
        if (reader.required() > size - av::ReplyHeader::encoded_size) __builtin_trap();
        if (field(data, size, 8u, 8u) != expected) __builtin_trap();
    } else if (status == av::Status::too_large) {
        if (size < av::ReplyHeader::encoded_size) __builtin_trap();
    }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    probe(data, size, field(data, size, 8u, 8u));
    probe(data, size, 1u);
    probe(data, size, 0xFFFFFFFFFFFFFFFFull);
    return 0;
}
