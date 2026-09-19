#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::CommandHeader;
using av::GuardedBuffer;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    CommandHeader header;
    const Status status = header.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[CommandHeader::encoded_size] = {};
        if (header.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(command_header_encode, places_the_id_and_the_size_with_its_flags_in_the_top_byte)
{ std::uint8_t bytes[CommandHeader::encoded_size] = {};
  CHECK_STATUS(CommandHeader(0x00010042u, 0x123456u, CommandHeader::generate_reply).encode(bytes, sizeof bytes),
               Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x00010042u);
  CHECK(layout.at(4u, 4u) == 0x01123456u); }

TEST(command_header, round_trips_and_names_its_api)
{ std::uint8_t bytes[CommandHeader::encoded_size] = {};
  CHECK_STATUS(CommandHeader(0x0003FFFFu, CommandHeader::max_size, 0u).encode(bytes, sizeof bytes), Status::ok);
  CommandHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.id(), 0x0003FFFFu);
  CHECK_EQ(decoded.api(), 3u);
  CHECK_EQ(decoded.size(), CommandHeader::max_size);
  CHECK(!decoded.generates_reply());
  CHECK_STATUS(CommandHeader(1u, 8u, CommandHeader::generate_reply).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(decoded.generates_reply());
  CHECK_EQ(decoded.api(), 0u); }

TEST(command_header_decode, a_size_below_the_header_or_an_unknown_flag_is_malformed)
{ struct Case { std::uint32_t size_word; };
  const Case cases[] = {{0u}, {7u}, {0x01000007u}, {0x02000010u}, {0x80000010u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[CommandHeader::encoded_size] = {1, 0, 1, 0};
      std::memcpy(bytes + 4, &c.size_word, 4);
      CommandHeader decoded(9u, 64u, 0u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK_EQ(decoded.id(), 9u);
  }
  CommandHeader short_buffer;
  const std::uint8_t bytes[4] = {};
  CHECK_STATUS(short_buffer.decode(bytes, sizeof bytes), Status::malformed); }

TEST(command_header_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(CommandHeader::encoded_size - 1u);
  CHECK_STATUS(CommandHeader(1u, 8u, 0u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(command_header_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[CommandHeader::encoded_size] = {};
  CHECK_STATUS(CommandHeader(0x10001u, 40u, CommandHeader::generate_reply).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 64u); }

TEST(command_header, the_shared_object_trio_are_transport_ids_and_are_all_different)
{ const std::uint32_t ids[] = {CommandHeader::share_object_id, CommandHeader::open_shared_object_id,
                               CommandHeader::release_shared_object_id};
  for (const std::uint32_t id : ids) {
      std::uint8_t bytes[CommandHeader::encoded_size] = {};
      CHECK_STATUS(CommandHeader(id, CommandHeader::encoded_size + 24u, 0u).encode(bytes, sizeof bytes),
                   Status::ok);
      CommandHeader decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
      CHECK_EQ(decoded.api(), CommandHeader::transport_api);
      CHECK_EQ(decoded.id(), id);
      CHECK_EQ(decoded.size(), CommandHeader::encoded_size + 24u);
  }
  CHECK(ids[0] != ids[1] && ids[1] != ids[2] && ids[0] != ids[2]);
  CHECK(ids[0] > CommandHeader::release_blob_id); }
