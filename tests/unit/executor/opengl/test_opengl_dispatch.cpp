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

}

TEST(opengl_dispatch, a_read_into_less_room_than_its_pixels_is_refused_before_the_driver)
{ GlSession session;
  CHECK(!session.read(16, 16, rgba, unsigned_byte, 1023u));
  CHECK(!session.read(16, 16, rgba, floating, 1024u));
  CHECK_EQ(session.gl().reads(), 0u);
  CHECK(session.read(16, 16, rgba, unsigned_byte, 1024u));
  CHECK(session.read(16, 16, rgba, floating, 4096u));
  CHECK_EQ(session.gl().reads(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, the_pack_store_counts_toward_what_a_read_needs)
{ GlSession session;
  session.set(av::glproto::kCmd_glPixelStorei, pack_alignment, 8u);
  CHECK(!session.read(3, 4, rgb, unsigned_byte, 36u));
  CHECK(!session.read(3, 4, rgb, unsigned_byte, 56u));
  CHECK(session.read(3, 4, rgb, unsigned_byte, 57u));
  session.set(av::glproto::kCmd_glPixelStorei, pack_skip_rows, 2u);
  CHECK(!session.read(3, 4, rgb, unsigned_byte, 57u));
  CHECK(session.read(3, 4, rgb, unsigned_byte, 89u));
  CHECK_EQ(session.gl().reads(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, a_read_it_cannot_size_is_refused_and_an_empty_one_runs)
{ GlSession session;
  CHECK(!session.read(0x7FFFFFFF, 0x7FFFFFFF, rgba, floating, 64u));
  CHECK(!session.read(-1, 4, rgba, unsigned_byte, 64u));
  CHECK(!session.read(4, 4, 0x1A00u, unsigned_byte, 64u));
  CHECK(!session.read(4, 4, rgba, 0x140Au, 1024u));
  CHECK_EQ(session.gl().reads(), 0u);
  CHECK(session.read(0, 4, rgba, unsigned_byte, 0u));
  CHECK_EQ(session.gl().reads(), 1u); }

TEST(opengl_dispatch, a_host_without_state_queries_reads_nothing_inline)
{ GlSession session;
  session.gl().forget_state_queries();
  CHECK(!session.read(1, 1, rgba, unsigned_byte, 4u));
  CHECK_EQ(session.gl().reads(), 0u); }

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
  CHECK_EQ(session.answered(), 1u + sizeof(GLint)); }

namespace {

constexpr std::uint64_t unsigned_short = 0x1403u;
constexpr std::uint64_t texture_2d = 0x0DE1u;
constexpr std::uint64_t unpack_alignment = 0x0CF5u;
constexpr std::uint64_t unpack_skip_rows = 0x0CF3u;
constexpr std::uint64_t unpack_compressed_block_size = 0x912Au;
constexpr std::uint64_t pixel_unpack_buffer = 0x88ECu;
constexpr std::uint64_t array_buffer = 0x8892u;
constexpr std::uint64_t element_array_buffer = 0x8893u;
constexpr std::uint64_t texture_border_color = 0x1004u;
constexpr std::uint64_t texture_min_filter = 0x2801u;
constexpr std::uint64_t compressed_texture_formats = 0x86A3u;
constexpr std::uint64_t active_uniform_indices = 0x8A43u;
constexpr std::uint64_t vertex_array = 0x8074u;
constexpr std::uint64_t texture_coord_array = 0x8078u;
constexpr std::uint64_t texture1 = 0x84C1u;
constexpr std::uint64_t map1_vertex_3 = 0x0D97u;
constexpr std::uint64_t map_coefficients = 0x0A00u;
constexpr std::uint64_t triangles = 0x0004u;
constexpr std::uint8_t  absent = 0u;
constexpr std::uint8_t  via_blob = 3u;
constexpr std::size_t   big_frame_bytes = 512u;

class Frame {
public:
    Frame() : bytes_{}, writer_(bytes_, sizeof bytes_) {}

    Frame &varint(std::uint64_t value) { writer_.write_varint(value); return *this; }
    Frame &pod64(std::uint64_t value) { writer_.write_pod<std::uint64_t>(value); return *this; }
    Frame &real(GLfloat value) { writer_.write_pod<GLfloat>(value); return *this; }
    Frame &none() { writer_.write_pod<std::uint8_t>(absent); return *this; }
    Frame &run(std::uint64_t length)
    {
        writer_.write_pod<std::uint8_t>(inline_bytes);
        writer_.write_varint(length);
        for (std::uint64_t i = 0u; i < length; i++) writer_.write_pod<std::uint8_t>(static_cast<std::uint8_t>(i));
        return *this;
    }
    Frame &text(const char *characters, std::size_t length)
    {
        writer_.write_pod<std::uint8_t>(inline_bytes);
        writer_.write_varint(length);
        writer_.write(characters, length);
        return *this;
    }
    Frame &room(std::uint64_t length)
    {
        writer_.write_pod<std::uint8_t>(inline_bytes);
        writer_.write_varint(length);
        return *this;
    }
    Frame &offset(std::uint64_t value)
    {
        writer_.write_pod<std::uint8_t>(buffer_offset);
        writer_.write_varint(value);
        return *this;
    }
    Frame &tag(std::uint8_t value) { writer_.write_pod<std::uint8_t>(value); return *this; }

    const WireWriter &writer() const { return writer_; }
    const std::uint8_t *bytes() const { return bytes_; }

private:
    std::uint8_t bytes_[big_frame_bytes];
    WireWriter   writer_;
};

bool send(GlSession &session, std::uint32_t command, const Frame &frame)
{
    return session.run(command, frame.writer(), frame.bytes());
}

Frame upload(bool specifies, std::int64_t width, std::int64_t height)
{
    Frame frame;
    frame.varint(texture_2d).varint(0u);
    if (specifies) frame.varint(rgb);
    else frame.varint(0u).varint(0u);
    frame.varint(static_cast<std::uint64_t>(width)).varint(static_cast<std::uint64_t>(height));
    if (specifies) frame.varint(0u);
    frame.varint(rgb).varint(unsigned_byte);
    return frame;
}

}

TEST(opengl_dispatch, a_count_sized_output_gets_room_for_the_count_the_driver_writes)
{ GlSession session;
  CHECK(send(session, av::glproto::kCmd_glGenBuffers, Frame().varint(64u).room(4u)));
  CHECK_EQ(session.answered(), 1u + 4u);
  CHECK(send(session, av::glproto::kCmd_glGenBuffers, Frame().varint(8u).none()));
  CHECK_EQ(session.answered(), 0u);
  CHECK_EQ(session.gl().calls(), 2u);
  CHECK(!send(session, av::glproto::kCmd_glGenBuffers, Frame().varint(static_cast<std::uint32_t>(-1)).room(4u)));
  CHECK(!send(session, av::glproto::kCmd_glGenBuffers, Frame().varint(1u << 28).room(4u)));
  CHECK_EQ(session.gl().calls(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, a_string_output_gets_its_stated_room_and_is_never_null)
{ GlSession session;
  const std::uint32_t log = av::glproto::kCmd_glGetShaderInfoLog;
  CHECK(send(session, log, Frame().varint(1u).varint(256u).room(4u).room(8u)));
  CHECK(send(session, log, Frame().varint(1u).varint(256u).none().none()));
  CHECK_EQ(session.gl().calls(), 2u);
  CHECK(!send(session, log, Frame().varint(1u).varint(static_cast<std::uint32_t>(-1)).none().room(8u)));
  CHECK_EQ(session.gl().calls(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, an_input_array_shorter_than_its_count_or_null_is_refused)
{ GlSession session;
  const std::uint32_t uniform = av::glproto::kCmd_glUniform4fv;
  struct Case { std::int64_t count; bool present; std::uint64_t bytes; bool runs; };
  constexpr Case cases[] = {{4, true, 64u, true},  {4, true, 63u, false}, {4, false, 0u, false},
                            {-1, true, 64u, false}, {0, false, 0u, true}};
  std::uint32_t ran = 0u;
  for (const Case &item : cases) {
      Frame frame;
      frame.varint(0u).varint(static_cast<std::uint64_t>(static_cast<std::uint32_t>(item.count)));
      if (item.present) frame.run(item.bytes);
      else frame.none();
      CHECK_EQ(send(session, uniform, frame), item.runs);
      if (item.runs) ran++;
  }
  CHECK_EQ(session.gl().calls(), ran);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, a_parameter_list_is_held_to_what_its_enum_takes)
{ GlSession session;
  const std::uint32_t parameter = av::glproto::kCmd_glTexParameteriv;
  CHECK(!send(session, parameter, Frame().varint(texture_2d).varint(texture_border_color).run(12u)));
  CHECK(!send(session, parameter, Frame().varint(texture_2d).varint(texture_border_color).tag(via_blob).varint(16u).varint(0u)));
  CHECK_EQ(session.gl().calls(), 0u);
  CHECK(send(session, parameter, Frame().varint(texture_2d).varint(texture_border_color).run(16u)));
  CHECK(send(session, parameter, Frame().varint(texture_2d).varint(texture_min_filter).run(4u)));
  CHECK_EQ(session.gl().calls(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, an_upload_is_held_to_the_unpack_store_and_an_offset_to_a_bound_buffer)
{ GlSession session;
  const std::uint32_t sub = av::glproto::kCmd_glTexSubImage2D;
  session.set(av::glproto::kCmd_glPixelStorei, unpack_alignment, 8u);
  CHECK(!send(session, sub, upload(false, 3, 4).run(56u)));
  CHECK(send(session, sub, upload(false, 3, 4).run(57u)));
  session.set(av::glproto::kCmd_glPixelStorei, unpack_skip_rows, 2u);
  CHECK(!send(session, sub, upload(false, 3, 4).run(57u)));
  CHECK(send(session, sub, upload(false, 3, 4).run(89u)));
  CHECK(!send(session, sub, upload(false, 3, 4).none()));
  CHECK(!send(session, sub, upload(false, 3, 4).offset(0x7F0000001000u)));
  CHECK(!send(session, sub, upload(false, -1, 4).run(89u)));
  CHECK_EQ(session.gl().calls(), 2u);
  CHECK(send(session, av::glproto::kCmd_glTexImage2D, upload(true, 3, 4).none()));
  session.set(av::glproto::kCmd_glBindBuffer, pixel_unpack_buffer, 7u);
  CHECK(send(session, sub, upload(false, 3, 4).none()));
  CHECK(send(session, sub, upload(false, 3, 4).offset(64u)));
  CHECK_EQ(session.gl().calls(), 5u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, a_compressed_upload_is_held_to_its_size_and_its_start)
{ GlSession session;
  const std::uint32_t compressed = av::glproto::kCmd_glCompressedTexImage2D;
  const auto image = [](std::int64_t size) {
      Frame frame;
      frame.varint(texture_2d).varint(0u).varint(0x83F0u).varint(4u).varint(4u).varint(0u)
           .varint(static_cast<std::uint64_t>(static_cast<std::uint32_t>(size)));
      return frame;
  };
  CHECK(!send(session, compressed, image(64).run(63u)));
  CHECK(!send(session, compressed, image(-1).run(64u)));
  CHECK(send(session, compressed, image(64).run(64u)));
  CHECK(send(session, compressed, image(64).none()));
  session.set(av::glproto::kCmd_glPixelStorei, unpack_skip_rows, 1u);
  CHECK(send(session, compressed, image(64).run(64u)));
  session.set(av::glproto::kCmd_glPixelStorei, unpack_compressed_block_size, 8u);
  CHECK(!send(session, compressed, image(64).run(64u)));
  CHECK_EQ(session.gl().calls(), 3u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, clear_data_is_one_element_of_its_format_and_never_an_offset)
{ GlSession session;
  const std::uint32_t clear = av::glproto::kCmd_glClearBufferData;
  const auto head = [](std::uint64_t format) {
      Frame frame;
      frame.varint(array_buffer).varint(0x8058u).varint(format).varint(unsigned_byte);
      return frame;
  };
  session.set(av::glproto::kCmd_glBindBuffer, pixel_unpack_buffer, 7u);
  CHECK(!send(session, clear, head(rgba).run(3u)));
  CHECK(!send(session, clear, head(rgba).offset(0u)));
  CHECK(!send(session, clear, head(0x1A00u).run(64u)));
  CHECK_EQ(session.gl().calls(), 0u);
  CHECK(send(session, clear, head(rgba).run(4u)));
  CHECK(send(session, clear, head(rgba).none()));
  CHECK_EQ(session.gl().calls(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, buffer_data_may_be_null_and_is_otherwise_held_to_its_size)
{ GlSession session;
  const std::uint32_t data = av::glproto::kCmd_glBufferData;
  CHECK(!send(session, data, Frame().varint(array_buffer).varint(16u).run(15u).varint(0x88E4u)));
  CHECK(send(session, data, Frame().varint(array_buffer).varint(16u).run(16u).varint(0x88E4u)));
  CHECK(send(session, data, Frame().varint(array_buffer).varint(1u << 20).none().varint(0x88E4u)));
  CHECK_EQ(session.gl().calls(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, call_lists_is_held_to_its_count_times_its_type)
{ GlSession session;
  const std::uint32_t lists = av::glproto::kCmd_glCallLists;
  CHECK(!send(session, lists, Frame().varint(4u).varint(unsigned_short).run(7u)));
  CHECK(!send(session, lists, Frame().varint(static_cast<std::uint32_t>(-1)).varint(unsigned_short).run(8u)));
  CHECK(send(session, lists, Frame().varint(4u).varint(unsigned_short).run(8u)));
  constexpr std::uint64_t three_bytes = 0x1408u;
  CHECK(!send(session, lists, Frame().varint(4u).varint(three_bytes).run(12u)));
  CHECK(send(session, lists, Frame().varint(4u).varint(three_bytes).run(16u)));
  CHECK(send(session, lists, Frame().varint(4u).varint(0x1400u).run(4u)));
  CHECK(send(session, lists, Frame().varint(4u).varint(0x140Bu).run(8u)));
  CHECK_EQ(session.gl().calls(), 4u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, a_draw_follows_no_offset_and_reads_no_array_without_a_buffer)
{ GlSession session;
  const Frame elements = Frame().varint(triangles).varint(3u).varint(unsigned_short).pod64(0u);
  const Frame arrays = Frame().varint(triangles).varint(0u).varint(3u);
  const auto pointer = [](std::uint64_t at) {
      return Frame().varint(0u).varint(3u).varint(floating).varint(0u).varint(0u).pod64(at);
  };
  CHECK(!send(session, av::glproto::kCmd_glDrawElements, elements));
  CHECK(!send(session, av::glproto::kCmd_glVertexAttribPointer, pointer(0x7F0000001000u)));
  CHECK(!send(session, av::glproto::kCmd_glDrawArraysIndirect, Frame().varint(triangles).pod64(0u)));
  CHECK_EQ(session.gl().calls(), 0u);
  session.set(av::glproto::kCmd_glBindBuffer, element_array_buffer, 2u);
  CHECK(send(session, av::glproto::kCmd_glDrawElements, elements));
  CHECK(send(session, av::glproto::kCmd_glVertexAttribPointer, pointer(0u)));
  CHECK(send(session, av::glproto::kCmd_glEnableVertexAttribArray, Frame().varint(0u)));
  CHECK(!send(session, av::glproto::kCmd_glDrawArrays, arrays));
  CHECK_EQ(session.gl().calls(), 2u);
  session.set(av::glproto::kCmd_glBindBuffer, array_buffer, 3u);
  CHECK(send(session, av::glproto::kCmd_glVertexAttribPointer, pointer(16u)));
  CHECK(send(session, av::glproto::kCmd_glDrawArrays, arrays));
  const std::uint8_t name[4] = {3u, 0u, 0u, 0u};
  CHECK(!send(session, av::glproto::kCmd_glDeleteBuffers, Frame().varint(1u).run(0u)));
  { Frame frame; frame.varint(1u).tag(inline_bytes).varint(4u);
    for (const std::uint8_t byte : name) frame.tag(byte);
    CHECK(send(session, av::glproto::kCmd_glDeleteBuffers, frame)); }
  CHECK(!send(session, av::glproto::kCmd_glDrawArrays, arrays));
  CHECK(!send(session, av::glproto::kCmd_glDrawElements, elements));
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, a_draw_reads_no_client_array_on_any_texture_unit)
{ GlSession session;
  const Frame arrays = Frame().varint(triangles).varint(0u).varint(3u);
  CHECK(send(session, av::glproto::kCmd_glDrawArrays, arrays));
  CHECK(send(session, av::glproto::kCmd_glEnableClientState, Frame().varint(vertex_array)));
  CHECK(!send(session, av::glproto::kCmd_glDrawArrays, arrays));
  session.set(av::glproto::kCmd_glBindBuffer, array_buffer, 4u);
  CHECK(send(session, av::glproto::kCmd_glVertexPointer, Frame().varint(3u).varint(floating).varint(0u).pod64(0u)));
  CHECK(send(session, av::glproto::kCmd_glDrawArrays, arrays));
  CHECK(send(session, av::glproto::kCmd_glClientActiveTexture, Frame().varint(texture1)));
  session.set(av::glproto::kCmd_glBindBuffer, array_buffer, 0u);
  CHECK(!send(session, av::glproto::kCmd_glTexCoordPointer, Frame().varint(2u).varint(floating).varint(0u).pod64(8u)));
  CHECK(send(session, av::glproto::kCmd_glEnableClientState, Frame().varint(texture_coord_array)));
  CHECK(!send(session, av::glproto::kCmd_glDrawArrays, arrays));
  CHECK(send(session, av::glproto::kCmd_glDisableClientState, Frame().varint(texture_coord_array)));
  CHECK(send(session, av::glproto::kCmd_glDrawArrays, arrays));
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, a_core_context_draw_asks_nothing_of_the_fixed_function_arrays)
{ GlSession session;
  const Frame arrays = Frame().varint(triangles).varint(0u).varint(3u);
  CHECK(send(session, av::glproto::kCmd_glDrawArrays, arrays));
  CHECK(session.gl().capability_queries() > 0u);
  session.gl().be_core();
  const std::uint32_t asked = session.gl().capability_queries();
  CHECK(send(session, av::glproto::kCmd_glDrawArrays, arrays));
  CHECK_EQ(session.gl().capability_queries(), asked);
  CHECK(send(session, av::glproto::kCmd_glEnableVertexAttribArray, Frame().varint(0u)));
  CHECK(!send(session, av::glproto::kCmd_glDrawArrays, arrays));
  CHECK_EQ(session.gl().capability_queries(), asked);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, a_multi_draw_holds_its_lists_to_the_draw_count)
{ GlSession session;
  session.set(av::glproto::kCmd_glBindBuffer, element_array_buffer, 2u);
  const auto draw = [](std::uint64_t drawcount) {
      Frame frame;
      frame.varint(triangles).run(8u).varint(unsigned_short);
      frame.tag(inline_bytes).varint(2u).pod64(0u).pod64(6u);
      frame.varint(drawcount);
      return frame;
  };
  CHECK(!send(session, av::glproto::kCmd_glMultiDrawElements, draw(3u)));
  CHECK(send(session, av::glproto::kCmd_glMultiDrawElements, draw(2u)));
  CHECK_EQ(session.gl().calls(), 1u);
  session.set(av::glproto::kCmd_glBindBuffer, element_array_buffer, 0u);
  CHECK(!send(session, av::glproto::kCmd_glMultiDrawElements, draw(2u)));
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, shader_source_lengths_may_not_reach_past_the_strings_that_crossed)
{ GlSession session;
  const auto source = [](std::uint64_t count, std::int32_t first_length, std::uint64_t lengths_bytes) {
      Frame frame;
      frame.varint(1u).varint(count);
      frame.tag(inline_bytes).varint(2u).varint(3u);
      frame.tag('a').tag('b').tag('c');
      frame.varint(2u).tag('d').tag('e');
      frame.tag(inline_bytes).varint(lengths_bytes);
      const std::int32_t lengths[2] = {first_length, 2};
      for (std::uint64_t i = 0u; i < lengths_bytes; i++)
          frame.tag(static_cast<std::uint8_t>(static_cast<std::uint32_t>(lengths[i / 4u]) >> (8u * (i % 4u))));
      return frame;
  };
  const std::uint32_t shader = av::glproto::kCmd_glShaderSource;
  CHECK(send(session, shader, source(2u, 3, 8u)));
  CHECK(send(session, shader, source(2u, -1, 8u)));
  CHECK(!send(session, shader, source(2u, 4, 8u)));
  CHECK(!send(session, shader, source(3u, 3, 8u)));
  CHECK(!send(session, shader, source(2u, 3, 4u)));
  CHECK_EQ(session.gl().calls(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, debug_text_is_read_no_further_than_what_crossed)
{ GlSession session;
  const std::uint32_t insert = av::glproto::kCmd_glDebugMessageInsert;
  const auto message = [](std::int32_t length, bool present) {
      Frame frame;
      frame.varint(0x824Au).varint(0x824Cu).varint(1u).varint(0x9146u)
           .varint(static_cast<std::uint64_t>(static_cast<std::uint32_t>(length)));
      if (present) frame.text("hello", 5u);
      else frame.none();
      return frame;
  };
  CHECK(send(session, insert, message(5, true)));
  CHECK(send(session, insert, message(-1, true)));
  CHECK(!send(session, insert, message(6, true)));
  CHECK(!send(session, insert, message(-1, false)));
  CHECK_EQ(session.gl().calls(), 2u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, a_list_query_gets_room_for_the_whole_list_the_driver_counts)
{ GlSession session;
  CHECK(send(session, av::glproto::kCmd_glGetIntegerv, Frame().varint(compressed_texture_formats).room(4u)));
  CHECK_EQ(session.answered(), 1u + 4u);
  CHECK(send(session, av::glproto::kCmd_glGetActiveUniformBlockiv,
             Frame().varint(1u).varint(0u).varint(active_uniform_indices).room(4u)));
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, a_map_is_held_to_its_order_and_stride_and_answered_by_its_order)
{ GlSession session;
  const auto map = [](std::uint64_t stride, std::uint64_t order, std::uint64_t bytes) {
      Frame frame;
      frame.varint(map1_vertex_3).real(0.0f).real(1.0f).varint(stride).varint(order).run(bytes);
      return frame;
  };
  const std::uint32_t map1 = av::glproto::kCmd_glMap1f;
  CHECK(!send(session, map1, map(3u, 12u, 143u)));
  CHECK(!send(session, map1, map(5u, 4u, 71u)));
  CHECK(!send(session, map1, map(3u, 0u, 16u)));
  CHECK(send(session, map1, map(5u, 4u, 72u)));
  CHECK(send(session, map1, map(3u, 12u, 144u)));
  CHECK(send(session, av::glproto::kCmd_glGetMapfv, Frame().varint(map1_vertex_3).varint(map_coefficients).room(4u)));
  CHECK_EQ(session.gl().calls(), 3u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, stipple_and_bitmap_are_held_to_their_stores)
{ GlSession session;
  session.set(av::glproto::kCmd_glPixelStorei, pack_alignment, 8u);
  CHECK(send(session, av::glproto::kCmd_glGetPolygonStipple, Frame().room(1u)));
  const auto bitmap = [](std::uint64_t width, std::uint64_t height) {
      Frame frame;
      frame.varint(width).varint(height).real(0.0f).real(0.0f).real(1.0f).real(0.0f);
      return frame;
  };
  const std::uint32_t draw = av::glproto::kCmd_glBitmap;
  CHECK(!send(session, draw, bitmap(9u, 2u).run(5u)));
  CHECK(send(session, draw, bitmap(9u, 2u).run(6u)));
  CHECK(!send(session, draw, bitmap(9u, 2u).none()));
  CHECK(send(session, draw, bitmap(0u, 0u).none()));
  CHECK_EQ(session.gl().calls(), 3u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, selection_and_feedback_buffers_are_refused_whatever_they_carry)
{ GlSession session;
  CHECK(!send(session, av::glproto::kCmd_glSelectBuffer, Frame().varint(4u).room(16u)));
  CHECK(!send(session, av::glproto::kCmd_glFeedbackBuffer, Frame().varint(4u).varint(0x0600u).room(16u)));
  CHECK_EQ(session.gl().calls(), 0u);
  CHECK_EQ(session.gl().strays(), 0u); }

TEST(opengl_dispatch, only_the_arrays_a_draw_is_checked_for_may_be_enabled)
{ GlSession session;
  constexpr std::uint64_t weight_array = 0x86ADu;
  constexpr std::uint64_t nv_attribute_array = 0x8650u;
  CHECK(!send(session, av::glproto::kCmd_glEnableClientState, Frame().varint(weight_array)));
  CHECK(!send(session, av::glproto::kCmd_glEnableClientState, Frame().varint(nv_attribute_array)));
  CHECK_EQ(session.gl().calls(), 0u);
  CHECK(send(session, av::glproto::kCmd_glEnableClientState, Frame().varint(texture_coord_array)));
  CHECK(send(session, av::glproto::kCmd_glDisableClientState, Frame().varint(weight_array)));
  CHECK_EQ(session.gl().calls(), 2u); }
