/* The OpenGL server dispatch between a guest's pixel reads and the host
 * driver.
 *
 * A read writes what GL works out - format, type, extent, pack state - at the
 * pointer it is given, and the room behind that pointer is a number the guest
 * sent. Driven over RecordingGl, with no OpenGL: what is asserted is that a
 * destination smaller than GL's own count, or an offset with no buffer bound
 * behind it, is refused before the driver is called, and that a read with
 * room still runs (feature 044, research section 2). */
#include "harness/executor/recording_gl.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_dispatch_server.h"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::RecordingGl;
using av::WireArena;
using av::WireReader;
using av::WireWriter;

namespace {

constexpr std::size_t block_bytes = 1u << 16;
constexpr std::size_t frame_bytes = 128u;
constexpr std::size_t reply_bytes = 1u << 17;

constexpr std::uint64_t rgba = 0x1908u;
constexpr std::uint64_t rgb = 0x1907u;
constexpr std::uint64_t unsigned_byte = 0x1401u;
constexpr std::uint64_t floating = 0x1406u;
constexpr std::uint64_t pack_alignment = 0x0D05u;
constexpr std::uint64_t pack_skip_rows = 0x0D03u;
constexpr std::uint64_t pixel_pack_buffer = 0x88EBu;
constexpr std::uint8_t  inline_bytes = 1u;
constexpr std::uint8_t  buffer_offset = 2u;

class GlSession {
public:
    GlSession() : block_{}, arena_(block_, sizeof block_), gl_(block_, arena_), reply_{}, answered_(0u) {}

    bool run(std::uint32_t command, const WireWriter &frame, const std::uint8_t *bytes)
    {
        arena_.reset();
        WireReader payload(bytes, frame.offset());
        WireWriter answer(reply_, sizeof reply_);
        const bool ran = av::glproto::dispatch_command(gl_.table(), command, payload, answer, arena_);
        answered_ = answer.offset();
        return ran;
    }

    std::size_t answered() const { return answered_; }

    /* glReadPixels(0, 0, width, height, format, type) into `room` inline bytes. */
    bool read(std::int64_t width, std::int64_t height, std::uint64_t format, std::uint64_t type,
              std::uint64_t room)
    {
        std::uint8_t bytes[frame_bytes];
        WireWriter frame(bytes, sizeof bytes);
        frame.write_varint(0u);
        frame.write_varint(0u);
        frame.write_varint(static_cast<std::uint64_t>(width));
        frame.write_varint(static_cast<std::uint64_t>(height));
        frame.write_varint(format);
        frame.write_varint(type);
        frame.write_pod<std::uint8_t>(inline_bytes);
        frame.write_varint(room);
        return run(av::glproto::kCmd_glReadPixels, frame, bytes);
    }

    /* A two-argument state command: glPixelStorei or glBindBuffer. */
    void set(std::uint32_t command, std::uint64_t first, std::uint64_t second)
    {
        std::uint8_t bytes[frame_bytes];
        WireWriter frame(bytes, sizeof bytes);
        frame.write_varint(first);
        frame.write_varint(second);
        CHECK(run(command, frame, bytes));
    }

    RecordingGl &gl() { return gl_; }

private:
    alignas(16) std::uint8_t block_[block_bytes];
    WireArena    arena_;
    RecordingGl  gl_;
    std::uint8_t reply_[reply_bytes];
    std::size_t  answered_;
};

}  // namespace

/* T006: 16 by 16 RGBA8 is 1024 bytes, and one fewer is a write past the room. */
TEST(opengl_dispatch, a_read_into_less_room_than_its_pixels_is_refused_before_the_driver)
{ GlSession session;
  CHECK(!session.read(16, 16, rgba, unsigned_byte, 1023u));
  CHECK(!session.read(16, 16, rgba, floating, 1024u));
  CHECK_EQ(session.gl().reads(), 0u);
  CHECK(session.read(16, 16, rgba, unsigned_byte, 1024u));
  CHECK(session.read(16, 16, rgba, floating, 4096u));
  CHECK_EQ(session.gl().reads(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

/* Width times height times bytes per pixel is not what GL writes once the
 * pack store says otherwise: at alignment 8 every row of three RGB pixels is
 * padded from 9 bytes to 16, and skipped rows come first. */
TEST(opengl_dispatch, the_pack_store_counts_toward_what_a_read_needs)
{ GlSession session;
  session.set(av::glproto::kCmd_glPixelStorei, pack_alignment, 8u);
  /* 4 rows: 3 padded rows of 16 and a last row of 9. */
  CHECK(!session.read(3, 4, rgb, unsigned_byte, 36u));
  CHECK(!session.read(3, 4, rgb, unsigned_byte, 56u));
  CHECK(session.read(3, 4, rgb, unsigned_byte, 57u));
  session.set(av::glproto::kCmd_glPixelStorei, pack_skip_rows, 2u);
  CHECK(!session.read(3, 4, rgb, unsigned_byte, 57u));
  CHECK(session.read(3, 4, rgb, unsigned_byte, 89u));
  CHECK_EQ(session.gl().reads(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

/* Sizes that multiply past 64 bits, negative ones, and a format this server
 * cannot size are not reads it can bound. An empty one writes nothing. */
TEST(opengl_dispatch, a_read_it_cannot_size_is_refused_and_an_empty_one_runs)
{ GlSession session;
  CHECK(!session.read(0x7FFFFFFF, 0x7FFFFFFF, rgba, floating, 64u));
  CHECK(!session.read(-1, 4, rgba, unsigned_byte, 64u));
  CHECK(!session.read(4, 4, 0x1A00u, unsigned_byte, 64u));
  CHECK(!session.read(4, 4, rgba, 0x140Au, 1024u));
  CHECK_EQ(session.gl().reads(), 0u);
  CHECK(session.read(0, 4, rgba, unsigned_byte, 0u));
  CHECK_EQ(session.gl().reads(), 1u); }

/* Without glGetIntegerv the pack store cannot be asked, so no read is sized. */
TEST(opengl_dispatch, a_host_without_state_queries_reads_nothing_inline)
{ GlSession session;
  session.gl().forget_state_queries();
  CHECK(!session.read(1, 1, rgba, unsigned_byte, 4u));
  CHECK_EQ(session.gl().reads(), 0u); }

/* An offset is an offset only while a pack buffer is bound; without one GL
 * writes at the guest's number as an address. */
TEST(opengl_dispatch, an_offset_read_needs_a_pack_buffer_bound)
{ GlSession session;
  std::uint8_t bytes[frame_bytes];
  WireWriter frame(bytes, sizeof bytes);
  frame.write_varint(0u);
  frame.write_varint(0u);
  frame.write_varint(4u);
  frame.write_varint(4u);
  frame.write_varint(rgba);
  frame.write_varint(unsigned_byte);
  frame.write_pod<std::uint8_t>(buffer_offset);
  frame.write_varint(0x7F0000001000u);
  CHECK(!session.run(av::glproto::kCmd_glReadPixels, frame, bytes));
  CHECK_EQ(session.gl().reads(), 0u);
  session.set(av::glproto::kCmd_glBindBuffer, pixel_pack_buffer, 5u);
  CHECK(session.run(av::glproto::kCmd_glReadPixels, frame, bytes));
  CHECK_EQ(session.gl().reads(), 1u); }

/* glReadnPixels writes no more than its bufSize, so bufSize is what must fit. */
TEST(opengl_dispatch, a_bounded_read_whose_bound_exceeds_its_room_is_refused)
{ GlSession session;
  struct Case { std::int64_t bound; std::uint64_t room; bool runs; };
  constexpr Case cases[] = {{65, 64u, false}, {-1, 64u, false}, {64, 64u, true}, {0, 0u, true}};
  std::uint32_t ran = 0u;
  for (const Case &item : cases) {
      std::uint8_t bytes[frame_bytes];
      WireWriter frame(bytes, sizeof bytes);
      const std::uint64_t leading[] = {0u, 0u, 4u, 4u, rgba, unsigned_byte};
      for (const std::uint64_t value : leading) frame.write_varint(value);
      frame.write_varint(static_cast<std::uint64_t>(item.bound));
      frame.write_pod<std::uint8_t>(inline_bytes);
      frame.write_varint(item.room);
      CHECK_EQ(session.run(av::glproto::kCmd_glReadnPixels, frame, bytes), item.runs);
      if (item.runs) ran++;
  }
  CHECK_EQ(session.gl().reads(), ran);
  CHECK_EQ(session.gl().strays(), 0u); }

/* glGetTexImage's extent is the level's, which the driver is asked for. */
TEST(opengl_dispatch, a_texture_read_is_sized_by_the_level_the_driver_reports)
{ GlSession session;
  session.gl().set_level_extent(8, 8, 2);
  const std::uint64_t rooms[] = {511u, 512u};
  for (const std::uint64_t room : rooms) {
      std::uint8_t bytes[frame_bytes];
      WireWriter frame(bytes, sizeof bytes);
      frame.write_varint(0x806Fu);
      frame.write_varint(0u);
      frame.write_varint(rgba);
      frame.write_varint(unsigned_byte);
      frame.write_pod<std::uint8_t>(inline_bytes);
      frame.write_varint(room);
      CHECK_EQ(session.run(av::glproto::kCmd_glGetTexImage, frame, bytes), room == 512u);
  }
  CHECK_EQ(session.gl().reads(), 1u);
  CHECK_EQ(session.gl().strays(), 0u); }

/* A compressed level's size is state this server does not ask for yet, so it
 * is read only into a bound pack buffer. A query result is one value wide. */
TEST(opengl_dispatch, a_compressed_read_goes_only_to_a_buffer_and_a_query_needs_one_value)
{ GlSession session;
  std::uint8_t bytes[frame_bytes];
  { WireWriter frame(bytes, sizeof bytes);
    frame.write_varint(0x0DE1u);
    frame.write_varint(0u);
    frame.write_pod<std::uint8_t>(inline_bytes);
    frame.write_varint(4096u);
    CHECK(!session.run(av::glproto::kCmd_glGetCompressedTexImage, frame, bytes)); }
  CHECK_EQ(session.gl().reads(), 0u);
  const std::uint64_t rooms[] = {3u, 4u};
  for (const std::uint64_t room : rooms) {
      WireWriter frame(bytes, sizeof bytes);
      frame.write_varint(1u);
      frame.write_varint(0x8866u);
      frame.write_pod<std::uint8_t>(inline_bytes);
      frame.write_varint(room);
      CHECK_EQ(session.run(av::glproto::kCmd_glGetQueryObjectiv, frame, bytes), room == 4u);
  }
  CHECK_EQ(session.gl().reads(), 1u);
  CHECK_EQ(session.gl().strays(), 0u); }

/* GL has no state query that takes a null destination: the driver writes
 * through it. A guest that sends none has the answer written into the block
 * and nothing sent back, which is what it asked for. */
TEST(opengl_dispatch, a_state_query_with_no_destination_is_answered_into_the_block_and_not_sent)
{ GlSession session;
  std::uint8_t bytes[frame_bytes];
  { WireWriter frame(bytes, sizeof bytes);
    frame.write_varint(pack_alignment);
    frame.write_pod<std::uint8_t>(0u);
    CHECK(session.run(av::glproto::kCmd_glGetIntegerv, frame, bytes)); }
  CHECK_EQ(session.answered(), 0u);
  { WireWriter frame(bytes, sizeof bytes);
    frame.write_varint(pack_alignment);
    frame.write_pod<std::uint8_t>(inline_bytes);
    frame.write_varint(sizeof(GLint));
    CHECK(session.run(av::glproto::kCmd_glGetIntegerv, frame, bytes)); }
  /* A one-byte length and the four bytes of alignment 4. */
  CHECK_EQ(session.answered(), 1u + sizeof(GLint)); }
