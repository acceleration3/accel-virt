/* The reply slot reader over attacker-chosen bytes: the device shares the slot,
 * so what it wrote is hostile. Whatever the reader accepts must never hand back
 * an answer that overruns the slot, and it must be consistent about the fence
 * it answers for. */
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
        /* An accepted answer needs a header, and its bytes lie inside the slot. */
        if (size < av::ReplyHeader::encoded_size) __builtin_trap();
        if (reader.required() > size - av::ReplyHeader::encoded_size) __builtin_trap();
        /* The header's fence is the one asked for. */
        if (field(data, size, 8u, 8u) != expected) __builtin_trap();
    } else if (status == av::Status::too_large) {
        /* Too large means the answer did not fit; nothing is read back. */
        if (size < av::ReplyHeader::encoded_size) __builtin_trap();
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    /* The fence the header names - the one a caller that wrote this slot would
     * wait on - and two the device did not answer for. */
    probe(data, size, field(data, size, 8u, 8u));
    probe(data, size, 1u);
    probe(data, size, 0xFFFFFFFFFFFFFFFFull);
    return 0;
}
