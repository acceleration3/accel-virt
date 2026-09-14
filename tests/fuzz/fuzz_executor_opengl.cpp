/* Hostile batches into the OpenGL server dispatch.
 *
 * Decoded one command after another over RecordingGl, which answers a
 * representative of every kind of pointer the decoder hands a driver - reads,
 * uploads, count-sized outputs and inputs, text, list queries, offsets and the
 * arrays a draw reads - and the state each depends on, with no OpenGL behind
 * it, and leaves every other entry point null for the decoder to refuse after
 * decoding. A batch, because what a call may touch depends on the
 * glPixelStorei, glBindBuffer and array calls before it. What must hold after
 * every command, whatever the bytes: no call was handed a pointer GL would
 * read or write past, or a null it would follow (feature 044's glReadPixels
 * hole and T030's), no offset or client array was followed as an address with
 * no buffer bound, nothing was written past the reply.
 *
 * Each record is a two-byte command within OpenGL's range, a two-byte length
 * and that many payload bytes. */
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
/* A small 3D level, so a texture read has an extent and skipped images count. */
constexpr GLint level_width = 8;
constexpr GLint level_height = 8;
constexpr GLint level_depth = 2;

alignas(16) std::uint8_t block[arena_bytes];
std::uint8_t reply[reply_bytes + 1u];

}  // namespace

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
