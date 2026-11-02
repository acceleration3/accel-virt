#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/region_message.hpp"
#include "proto/renderer/renderer_extent.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::RegionMessage;
using av::RendererExtent;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    RegionMessage message;
    const Status status = message.decode(bytes, length, RegionMessage::descriptors);
    if (status == Status::ok) {
        std::uint8_t again[RegionMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(region_message_encode, is_the_region_four_reserved_bytes_and_the_size)
{ GuardedBuffer bytes(RegionMessage::encoded_size);
  CHECK_STATUS(RegionMessage(0x123456u, 0x80000000ull).encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 0x123456u);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == 0x80000000ull);
  CHECK_EQ(RegionMessage::descriptors, 1u);
  CHECK(bytes.guards_intact()); }

TEST(region_message, round_trips_the_largest_region_an_extent_names)
{ std::uint8_t bytes[RegionMessage::encoded_size] = {};
  CHECK_STATUS(RegionMessage(RendererExtent::region_count - 1u, RendererExtent::region_span).encode(bytes, sizeof bytes),
               Status::ok);
  RegionMessage decoded;
  CHECK_EQ(decoded.region(), 0u);
  CHECK(decoded.size() == 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::ok);
  CHECK_EQ(decoded.region(), RendererExtent::region_count - 1u);
  CHECK(decoded.size() == RendererExtent::region_span); }

TEST(region_message_encode, refuses_what_decode_would)
{ std::uint8_t bytes[RegionMessage::encoded_size] = {};
  CHECK_STATUS(RegionMessage(RendererExtent::region_count, 1u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(RegionMessage(1u, 0u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(RegionMessage(1u, RendererExtent::region_span + 1u).encode(bytes, sizeof bytes),
               Status::invalid_parameter); }

TEST(region_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(RegionMessage::encoded_size - 1u);
  CHECK_STATUS(RegionMessage(1u, 1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(RegionMessage(1u, 1u).encode(nullptr, RegionMessage::encoded_size), Status::too_large); }

TEST(region_message_decode, needs_exactly_one_descriptor)
{ std::uint8_t bytes[RegionMessage::encoded_size] = {};
  CHECK_STATUS(RegionMessage(2u, 0x1000u).encode(bytes, sizeof bytes), Status::ok);
  RegionMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 2u), Status::malformed);
  CHECK_EQ(decoded.region(), 0u); }

TEST(region_message_decode, refuses_bad_fields_and_stays_unchanged)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  const Case cases[] = {{0u, 4u, RendererExtent::region_count}, {4u, 4u, 1u}, {8u, 8u, 0u},
                        {8u, 8u, RendererExtent::region_span + 1u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[RegionMessage::encoded_size] = {};
      CHECK_STATUS(RegionMessage(2u, 0x1000u).encode(bytes, sizeof bytes), Status::ok);
      for (std::size_t i = 0u; i < c.width; i++) bytes[c.offset + i] = static_cast<std::uint8_t>(c.value >> (8u * i));
      RegionMessage decoded(3u, 4u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 1u), Status::malformed);
      CHECK_EQ(decoded.region(), 3u);
      CHECK(decoded.size() == 4u);
  } }

TEST(region_message_decode, any_other_length_or_no_bytes_is_malformed)
{ std::uint8_t bytes[RegionMessage::encoded_size + 1u] = {};
  CHECK_STATUS(RegionMessage(2u, 0x1000u).encode(bytes, sizeof bytes), Status::ok);
  RegionMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, RegionMessage::encoded_size - 1u, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, RegionMessage::encoded_size + 1u, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, RegionMessage::encoded_size, 1u), Status::malformed); }

TEST(region_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[RegionMessage::encoded_size] = {};
  CHECK_STATUS(RegionMessage(5u, 0x10000u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u);
  const std::uint32_t acceptable = 24u + 39u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
