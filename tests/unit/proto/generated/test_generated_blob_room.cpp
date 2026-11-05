#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"

#include <cstdint>

using av::MockTransport;

namespace {

std::uint8_t bulk[256u * 1024u];

}

TEST(generated_blob_room, a_bulk_upload_asks_for_room_before_its_writer_is_lent)
{ MockTransport wire;
  const av::d3dproto::Box box = {};
  static_cast<void>(av::d3dproto::av_d3d_UpdateSubresource(wire, 1u, 0u, 0u, box, 0u, 0u, 0u, bulk, 100000u));
  CHECK_EQ(wire.room_requests(), 1u);
  CHECK_EQ(wire.last_room_blobs(), 1u);
  CHECK_EQ(wire.last_room_bytes(), 100000u);
  CHECK(wire.room_asked_at() < wire.writer_lent_at()); }

TEST(generated_blob_room, an_upload_that_fits_inline_asks_for_nothing)
{ MockTransport wire;
  const av::d3dproto::Box box = {};
  static_cast<void>(av::d3dproto::av_d3d_UpdateSubresource(wire, 1u, 0u, 0u, box, 0u, 0u, 0u, bulk, 4096u));
  CHECK_EQ(wire.room_requests(), 0u);
  CHECK(wire.writer_lent_at() != 0u); }

TEST(generated_blob_room, a_read_that_must_come_back_through_the_arena_asks_for_its_reply_room_first)
{ MockTransport wire;
  std::uint32_t hresult = 0u;
  std::uint32_t row_pitch = 0u;
  std::uint32_t depth_pitch = 0u;
  std::uint64_t size = 0u;
  static_cast<void>(av::d3dproto::av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, bulk,
                                                          200000u, &size));
  CHECK_EQ(wire.room_requests(), 1u);
  CHECK_EQ(wire.last_room_bytes(), 200000u);
  CHECK(wire.room_asked_at() < wire.writer_lent_at()); }

TEST(generated_blob_room, a_read_the_reply_slot_can_hold_asks_for_nothing)
{ MockTransport wire;
  std::uint32_t hresult = 0u;
  std::uint32_t row_pitch = 0u;
  std::uint32_t depth_pitch = 0u;
  std::uint64_t size = 0u;
  static_cast<void>(av::d3dproto::av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, bulk, 1024u,
                                                          &size));
  CHECK_EQ(wire.room_requests(), 0u); }

TEST(generated_blob_room, a_command_with_no_bulk_parameter_never_asks)
{ MockTransport wire;
  std::uint32_t hresult = 0u;
  static_cast<void>(av::d3d12proto::av_d3d12_OpenDevice12(wire, 0u, &hresult));
  CHECK_EQ(wire.room_requests(), 0u); }
