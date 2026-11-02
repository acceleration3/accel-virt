#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/exported_image_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::ExportedImageMessage;
using av::GuardedBuffer;
using av::Status;

namespace {

constexpr std::uint32_t xr24 = 0x34325258u;

ExportedImageMessage image(bool fresh = true, std::uint32_t slot = 1u)
{
    return ExportedImageMessage(fresh, slot, 1920u, 1080u, 7680u, 64u, xr24, 0x0100000000000002u);
}

std::uint32_t descriptors_of(const std::uint8_t *bytes)
{
    std::uint32_t found = 0u, fresh = 0u;
    std::memcpy(&found, bytes, 4u);
    std::memcpy(&fresh, bytes + 4u, 4u);
    return found == 1u && fresh == 1u ? 1u : 0u;
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    ExportedImageMessage message;
    const std::uint32_t  count = length >= 8u ? descriptors_of(bytes) : 0u;
    const Status         status = message.decode(bytes, length, count);
    if (status == Status::ok) {
        std::uint8_t again[ExportedImageMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(exported_image_message_encode, lays_out_found_fresh_slot_then_the_layout)
{ GuardedBuffer bytes(ExportedImageMessage::encoded_size);
  CHECK_STATUS(image().encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 1u);
  CHECK(layout.at(4u, 4u) == 1u);
  CHECK(layout.at(8u, 4u) == 1u);
  CHECK(layout.at(12u, 4u) == 1920u);
  CHECK(layout.at(16u, 4u) == 1080u);
  CHECK(layout.at(20u, 4u) == 7680u);
  CHECK(layout.at(24u, 4u) == 64u);
  CHECK(layout.at(28u, 4u) == xr24);
  CHECK(layout.at(32u, 8u) == 0x0100000000000002u);
  CHECK(bytes.guards_intact()); }

TEST(exported_image_message_encode, nothing_found_is_all_zeros)
{ std::uint8_t bytes[ExportedImageMessage::encoded_size];
  std::memset(bytes, 0xEE, sizeof bytes);
  const ExportedImageMessage none;
  CHECK(!none.found());
  CHECK(!none.fresh());
  CHECK_EQ(none.descriptors(), 0u);
  CHECK_STATUS(none.encode(bytes, sizeof bytes), Status::ok);
  for (const std::uint8_t byte : bytes) CHECK_EQ(byte, 0u); }

TEST(exported_image_message, carries_a_descriptor_only_when_found_and_fresh)
{ CHECK_EQ(image(true).descriptors(), 1u);
  CHECK_EQ(image(false).descriptors(), 0u);
  CHECK_EQ(ExportedImageMessage().descriptors(), 0u); }

TEST(exported_image_message, round_trips_through_its_encoding)
{ const ExportedImageMessage cases[] = {image(true, 0u), image(false, 1u), ExportedImageMessage()};
  for (const ExportedImageMessage &original : cases) {
      std::uint8_t bytes[ExportedImageMessage::encoded_size] = {};
      CHECK_STATUS(original.encode(bytes, sizeof bytes), Status::ok);
      ExportedImageMessage decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, original.descriptors()), Status::ok);
      CHECK_EQ(decoded.found(), original.found());
      CHECK_EQ(decoded.fresh(), original.fresh());
      CHECK_EQ(decoded.slot(), original.slot());
      CHECK_EQ(decoded.width(), original.width());
      CHECK_EQ(decoded.height(), original.height());
      CHECK_EQ(decoded.stride(), original.stride());
      CHECK_EQ(decoded.offset(), original.offset());
      CHECK_EQ(decoded.fourcc(), original.fourcc());
      CHECK_EQ(decoded.modifier(), original.modifier());
  } }

TEST(exported_image_message_encode, refuses_what_decode_would_refuse)
{ std::uint8_t bytes[ExportedImageMessage::encoded_size] = {};
  CHECK_STATUS(ExportedImageMessage(true, ExportedImageMessage::slots, 1u, 1u, 4u, 0u, xr24, 0u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(ExportedImageMessage(true, ExportedImageMessage::slots - 1u, 1u, 1u, 4u, 0u, xr24, 0u)
                   .encode(bytes, sizeof bytes),
               Status::ok);
  CHECK_STATUS(ExportedImageMessage(true, 0u, 0u, 1u, 4u, 0u, xr24, 0u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(ExportedImageMessage(true, 0u, 1u, 0u, 4u, 0u, xr24, 0u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(ExportedImageMessage(true, 0u, 1u, 1u, 0u, 0u, xr24, 0u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  GuardedBuffer short_buffer(ExportedImageMessage::encoded_size - 1u);
  CHECK_STATUS(image().encode(short_buffer.data(), short_buffer.size()), Status::too_large);
  CHECK(short_buffer.guards_intact());
  CHECK_STATUS(image().encode(nullptr, ExportedImageMessage::encoded_size), Status::too_large); }

TEST(exported_image_message_decode, refuses_each_malformed_field_and_changes_nothing)
{ std::uint8_t valid[ExportedImageMessage::encoded_size] = {};
  CHECK_STATUS(image(true, 1u).encode(valid, sizeof valid), Status::ok);
  struct Case {
      std::size_t   offset;
      std::uint32_t value;
  };
  const Case cases[] = {
      {0u, 2u},
      {4u, 2u},
      {8u, ExportedImageMessage::slots},
      {12u, 0u},
      {16u, 0u},
      {20u, 0u},
  };
  for (const Case &test : cases) {
      std::uint8_t bytes[sizeof valid];
      std::memcpy(bytes, valid, sizeof bytes);
      std::memcpy(bytes + test.offset, &test.value, 4u);
      ExportedImageMessage decoded = image(false, 0u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::malformed);
      CHECK_EQ(decoded.slot(), 0u);
      CHECK(!decoded.fresh());
  }
  for (std::size_t offset = 4u; offset < ExportedImageMessage::encoded_size; offset += 4u) {
      std::uint8_t bytes[ExportedImageMessage::encoded_size] = {};
      bytes[offset] = 1u;
      ExportedImageMessage decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  }
  ExportedImageMessage decoded;
  CHECK_STATUS(decoded.decode(valid, sizeof valid, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(valid, sizeof valid, 2u), Status::malformed);
  std::uint8_t stale[ExportedImageMessage::encoded_size] = {};
  CHECK_STATUS(image(false).encode(stale, sizeof stale), Status::ok);
  CHECK_STATUS(decoded.decode(stale, sizeof stale, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(valid, sizeof valid - 1u, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, sizeof valid, 1u), Status::malformed);
  CHECK(!decoded.found()); }

TEST(exported_image_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[ExportedImageMessage::encoded_size] = {};
  CHECK_STATUS(image().encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 64u);
  CHECK(sweep.rejections() > ExportedImageMessage::encoded_size); }
