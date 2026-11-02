#include "harness/executor/recording_cl_dispatch.hpp"
#include "proto/generated/opencl/cl_dispatch_server.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t   record_header_bytes = 4u;
constexpr std::size_t   max_payload_bytes = 1024u;
constexpr std::size_t   max_commands = 64u;
constexpr std::size_t   reply_bytes = 512u;
constexpr std::size_t   arena_bytes = 1u << 16;
constexpr std::uint32_t opencl_api = 2u;
constexpr std::uint32_t made_marker = 0xC1C1C1C1u;
constexpr std::uint8_t  guard = 0xABu;

alignas(16) std::uint8_t block[arena_bytes];

void substitute(const av::RecordingClDispatch &dispatch, std::uint8_t *payload, std::size_t length)
{
    for (std::size_t at = 0u; at + sizeof(std::uint64_t) <= length; at++) {
        std::uint64_t value = 0u;
        std::memcpy(&value, payload + at, sizeof value);
        if (value >> 32u != made_marker) continue;
        value = dispatch.wire_of_made(static_cast<std::uint32_t>(value));
        std::memcpy(payload + at, &value, sizeof value);
        at += sizeof value - 1u;
    }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireArena arena(block, sizeof block);
    av::RecordingClDispatch dispatch(block, arena);
    std::size_t offset = 0u;
    for (std::size_t command = 0u; command < max_commands; command++) {
        if (size - offset < record_header_bytes) break;
        const std::uint32_t id = (opencl_api << 16u) | (static_cast<std::uint32_t>(data[offset]) | (static_cast<std::uint32_t>(data[offset + 1u]) << 8u));
        std::size_t length = static_cast<std::size_t>(data[offset + 2u]) | (static_cast<std::size_t>(data[offset + 3u]) << 8u);
        offset += record_header_bytes;
        if (length > size - offset) length = size - offset;
        if (length > max_payload_bytes) length = max_payload_bytes;

        std::uint8_t payload[max_payload_bytes];
        std::memcpy(payload, data + offset, length);
        offset += length;
        substitute(dispatch, payload, length);

        std::uint8_t reply[reply_bytes + 1u];
        std::memset(reply, guard, sizeof reply);
        arena.reset();
        av::WireReader reader(payload, length);
        av::WireWriter writer(reply, reply_bytes);
        (void)av::clproto::dispatch_command(dispatch, id, reader, writer, arena);

        if (dispatch.strangers() != 0u || dispatch.strays() != 0u) __builtin_trap();
        if (dispatch.builds_without_argument_info() != 0u) __builtin_trap();
        if (arena.used() > arena.capacity()) __builtin_trap();
        if (reply[reply_bytes] != guard) __builtin_trap();
        if (!dispatch.handles().invariants_hold()) __builtin_trap();
    }
    return 0;
}
