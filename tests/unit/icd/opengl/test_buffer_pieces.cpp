/* A buffer's bytes cut into commands one each fits inline (BufferPieces),
 * judged against a host that decodes them with the generated server and
 * refuses what the executor would: what the buffer holds after, and that no
 * command was larger than its room. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_transfer_host.hpp"
#include "icd/opengl/buffer_pieces.hpp"

#include "gl_dispatch_client.h"
#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

using av::GlTransferHost;
using av::glicd::BufferPieces;
namespace ids = av::glproto;

namespace {

constexpr GLenum array_buffer = 0x8892u, static_draw = 0x88E4u;

/* Bytes no two pieces share, so a piece written in the wrong place shows. */
class Pattern {
public:
    explicit Pattern(std::size_t bytes) : bytes_(static_cast<std::uint8_t *>(std::malloc(bytes))), size_(bytes)
    {
        for (std::size_t index = 0u; index < bytes; index++)
            bytes_[index] = static_cast<std::uint8_t>(index * 7u + index / 251u);
    }
    ~Pattern() { std::free(bytes_); }
    Pattern(const Pattern &) = delete;
    Pattern &operator=(const Pattern &) = delete;
    std::uint8_t *bytes() { return bytes_; }
    std::size_t   size() const { return size_; }

private:
    std::uint8_t *bytes_;
    std::size_t   size_;
};

/* A buffer of `bytes` the host made and bound at GL_ARRAY_BUFFER. */
GLuint made_buffer(GlTransferHost &host, std::size_t bytes)
{
    GLuint name = 0u;
    ids::av_glGenBuffers(host, 1, &name);
    ids::av_glBindBuffer(host, array_buffer, name);
    ids::av_glBufferData(host, array_buffer, static_cast<GLsizeiptr>(bytes), nullptr, static_draw);
    return name;
}

constexpr std::size_t awkward = 3u * BufferPieces::write_room + 17u;

}  // namespace

/* The room is the line: what fits a command goes whole, a byte more is cut;
 * nothing to send, a negative size or one past any buffer goes whole for the
 * host to judge; and what the blob arena can lend whole is lent, not cut. */
TEST(buffer_pieces, a_write_is_cut_only_past_the_room_and_what_no_lease_holds)
{ GlTransferHost host;
  BufferPieces   pieces(host);
  std::uint8_t   byte = 0u;
  constexpr auto room = static_cast<intptr_t>(BufferPieces::write_room);
  CHECK(!pieces.cuts_write(room, &byte));
  CHECK(pieces.cuts_write(room + 1, &byte));
  CHECK(pieces.cuts_write(static_cast<intptr_t>(BufferPieces::max_bytes), &byte));
  CHECK(!pieces.cuts_write(static_cast<intptr_t>(BufferPieces::max_bytes) + 1, &byte));
  CHECK(!pieces.cuts_write(-1, &byte));
  CHECK(!pieces.cuts_write(room + 1, nullptr));
  static std::uint8_t arena[1u << 20];
  host.lend(arena, sizeof arena);
  CHECK(!pieces.cuts_write(room + 1, &byte));
  CHECK(pieces.lends_whole(sizeof arena));
  CHECK(!pieces.lends_whole(sizeof arena + 1u));
  CHECK(pieces.cuts_write(static_cast<intptr_t>(sizeof arena) + 1, &byte));
  CHECK_EQ(host.commands(), 0u); }

TEST(buffer_pieces, a_read_is_cut_only_past_one_reply)
{ GlTransferHost host;
  const BufferPieces pieces(host);
  std::uint8_t   byte = 0u;
  constexpr auto room = static_cast<intptr_t>(BufferPieces::read_room);
  CHECK(!pieces.cuts_read(room, &byte));
  CHECK(pieces.cuts_read(room + 1, &byte));
  CHECK(!pieces.cuts_read(static_cast<intptr_t>(BufferPieces::max_bytes) + 1, &byte));
  CHECK(!pieces.cuts_read(-5, &byte));
  CHECK(!pieces.cuts_read(room + 1, nullptr)); }

/* Why the cut exists: one command past the room is lost - a write too large
 * for the request is never sent, a read too large for the reply is refused
 * by the host, which faults a real context. */
TEST(buffer_pieces, one_command_past_the_room_is_lost)
{ GlTransferHost host;
  Pattern        data(BufferPieces::write_room * 2u);
  const GLuint   name = made_buffer(host, data.size());
  const std::uint32_t before = host.commands();
  ids::av_glBufferSubData(host, array_buffer, 0, static_cast<GLsizeiptr>(data.size()), data.bytes());
  CHECK_EQ(host.commands(), before);
  CHECK_EQ(host.buffer(name)[1], 0u);
  ids::av_glGetBufferSubData(host, array_buffer, 0, static_cast<GLsizeiptr>(BufferPieces::read_room * 2u),
                             data.bytes());
  CHECK_EQ(host.refusals(), 1u); }

/* Each spelling: the bytes land whole at their offset, in as many commands as
 * the room says, none of them larger than a request holds. */
TEST(buffer_pieces, a_write_of_any_size_lands_whole_at_its_offset)
{ for (std::uint32_t way = 0u; way < 3u; way++) {
      GlTransferHost host;
      Pattern        data(awkward);
      const GLuint   name = made_buffer(host, awkward + 5u);
      BufferPieces   pieces(host);
      const std::uint32_t before = host.commands();
      if (way == 0u) pieces.write(array_buffer, 5, awkward, data.bytes());
      if (way == 1u) pieces.write_named(name, 5, awkward, data.bytes());
      if (way == 2u) pieces.write_named_ext(name, 5, awkward, data.bytes());
      CHECK_EQ(pieces.commands(), 4u);
      CHECK_EQ(host.commands() - before, 4u);
      CHECK_EQ(host.refusals(), 0u);
      CHECK(host.largest_request() <= 256u * 1024u);
      CHECK(std::memcmp(host.buffer(name) + 5, data.bytes(), awkward) == 0);
      CHECK_EQ(host.buffer(name)[4], 0u);
  } }

TEST(buffer_pieces, a_read_of_any_size_comes_back_whole_from_its_offset)
{ for (std::uint32_t way = 0u; way < 3u; way++) {
      GlTransferHost host;
      Pattern        data(awkward);
      const GLuint   name = made_buffer(host, awkward + 9u);
      ids::av_glBufferSubData(host, array_buffer, 0, 9, data.bytes());
      BufferPieces(host).write(array_buffer, 9, awkward, data.bytes());
      Pattern      back(awkward + 1u);
      back.bytes()[awkward] = 0x5Au;
      BufferPieces pieces(host);
      if (way == 0u) pieces.read(array_buffer, 9, awkward, back.bytes());
      if (way == 1u) pieces.read_named(name, 9, awkward, back.bytes());
      if (way == 2u) pieces.read_named_ext(name, 9, awkward, back.bytes());
      const std::size_t replies = (awkward + BufferPieces::read_room - 1u) / BufferPieces::read_room;
      CHECK_EQ(pieces.commands(), static_cast<std::uint32_t>(replies));
      CHECK_EQ(host.refusals(), 0u);
      CHECK(std::memcmp(back.bytes(), data.bytes(), awkward) == 0);
      CHECK_EQ(back.bytes()[awkward], 0x5Au);
  } }

/* Nothing to move is no command at all. */
TEST(buffer_pieces, nothing_to_move_sends_nothing)
{ GlTransferHost host;
  BufferPieces   pieces(host);
  std::uint8_t   byte = 0u;
  pieces.write(array_buffer, 0, 0u, &byte);
  pieces.read(array_buffer, 0, 0u, &byte);
  CHECK_EQ(pieces.commands(), 0u);
  CHECK_EQ(host.commands(), 0u); }
