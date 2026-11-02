#include "harness/executor/recording_gl.hpp"
#include "proto/generated/opengl/gl_dispatch_server.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t   record_header_bytes = 4u;
constexpr std::size_t   max_commands = 64u;
constexpr std::size_t   reply_bytes = 4096u;
constexpr std::size_t   arena_bytes = 1u << 16;
constexpr std::uint32_t opengl_api = 3u;
constexpr std::uint8_t  guard = 0xABu;
constexpr GLint level_width = 8;
constexpr GLint level_height = 8;
constexpr GLint level_depth = 2;

alignas(16) std::uint8_t block[arena_bytes];
std::uint8_t reply[reply_bytes + 1u];

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireArena arena(block, sizeof block);
    av::RecordingGl gl(block, arena);
    gl.set_level_extent(level_width, level_height, level_depth);
    std::size_t offset = 0u;
    for (std::size_t command = 0u; command < max_commands; command++) {
        if (size - offset < record_header_bytes) break;
        const std::uint32_t id = (opengl_api << 16u) | (static_cast<std::uint32_t>(data[offset]) | (static_cast<std::uint32_t>(data[offset + 1u]) << 8u));
        std::size_t length = static_cast<std::size_t>(data[offset + 2u]) | (static_cast<std::size_t>(data[offset + 3u]) << 8u);
        offset += record_header_bytes;
        if (length > size - offset) length = size - offset;

        std::memset(reply, guard, sizeof reply);
        arena.reset();
        av::WireReader reader(data + offset, length);
        av::WireWriter writer(reply, reply_bytes);
        (void)av::glproto::dispatch_command(gl.table(), id, reader, writer, arena);
        offset += length;

        if (gl.strays() != 0u) __builtin_trap();
        if (arena.used() > arena.capacity()) __builtin_trap();
        if (reply[reply_bytes] != guard) __builtin_trap();
    }
    return 0;
}
