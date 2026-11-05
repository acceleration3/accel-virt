#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/generated/opengl/gl_dispatch_client.h"

#include <cstdint>

using av::MockTransport;

namespace {

std::uint8_t bulk[256u * 1024u];

}

TEST(generated_gl_blob_room, an_opengl_array_upload_asks_for_room_before_its_writer_is_lent)
{ MockTransport wire;
  av::glproto::av_glBufferSubData(wire, 0x8892u, 0, 100000, bulk);
  CHECK_EQ(wire.room_requests(), 1u);
  CHECK_EQ(wire.last_room_bytes(), 100000u);
  CHECK(wire.room_asked_at() < wire.writer_lent_at()); }

TEST(generated_gl_blob_room, an_opengl_upload_under_a_page_asks_for_nothing)
{ MockTransport wire;
  av::glproto::av_glBufferSubData(wire, 0x8892u, 0, 1000, bulk);
  CHECK_EQ(wire.room_requests(), 0u);
  CHECK(wire.writer_lent_at() != 0u); }

TEST(generated_gl_blob_room, an_opengl_upload_with_no_data_names_no_blob)
{ MockTransport wire;
  av::glproto::av_glBufferSubData(wire, 0x8892u, 0, 100000, nullptr);
  CHECK_EQ(wire.room_requests(), 0u); }

TEST(generated_gl_blob_room, an_opengl_pixel_upload_asks_for_room_unless_it_names_a_buffer_offset)
{ MockTransport wire;
  av::glproto::av_glClearTexImage(wire, 1u, 0, 0x1908u, 0x1401u, bulk, 65536u, false);
  CHECK_EQ(wire.room_requests(), 1u);
  CHECK_EQ(wire.last_room_bytes(), 65536u);
  CHECK(wire.room_asked_at() < wire.writer_lent_at());
  av::glproto::av_glClearTexImage(wire, 1u, 0, 0x1908u, 0x1401u, nullptr, 65536u, true);
  CHECK_EQ(wire.room_requests(), 1u); }
