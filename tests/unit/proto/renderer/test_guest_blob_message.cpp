#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/guest_blob_message.hpp"
#include "proto/renderer/renderer_extent.hpp"
#include "proto/renderer/renderer_message_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::GuestBlobMessage;
using av::RendererExtent;
using av::RendererMessageHeader;
using av::Status;

namespace {

const RendererExtent two_regions[] = {RendererExtent(1u, 0x1000u, 0x1000u), RendererExtent(2u, 0u, 0x2000u)};
constexpr std::size_t two_length = GuestBlobMessage::fixed_size + 2u * RendererExtent::encoded_size;

GuestBlobMessage sample()
{
    return GuestBlobMessage(3u, 0x3000u, two_regions, 2u);
}

void put(std::uint8_t *bytes, std::size_t offset, std::size_t width, std::uint64_t value)
{
    for (std::size_t i = 0u; i < width; i++) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    RendererExtent   extents[4] = {};
    GuestBlobMessage message;
    const Status     status = message.decode(bytes, length, GuestBlobMessage::descriptors, extents, 4u);
    if (status == Status::ok) {
        std::uint8_t again[two_length] = {};
        if (message.encoded_length() != length || message.encode(again, sizeof again) != Status::ok ||
            std::memcmp(again, bytes, length) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(guest_blob_message_encode, is_the_blob_the_count_the_size_then_each_extent)
{ GuardedBuffer bytes(two_length);
  CHECK_EQ(sample().encoded_length(), two_length);
  CHECK_STATUS(sample().encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 3u);
  CHECK(layout.at(4u, 4u) == 2u);
  CHECK(layout.at(8u, 8u) == 0x3000u);
  CHECK(layout.at(16u, 8u) == ((std::uint64_t{1} << 40) | 0x1000u));
  CHECK(layout.at(24u, 8u) == 0x1000u);
  CHECK(layout.at(32u, 8u) == (std::uint64_t{2} << 40));
  CHECK(layout.at(40u, 8u) == 0x2000u);
  CHECK_EQ(GuestBlobMessage::descriptors, 0u);
  CHECK(bytes.guards_intact()); }

TEST(guest_blob_message, round_trips_into_the_extents_it_was_given)
{ std::uint8_t bytes[two_length] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  RendererExtent   extents[2] = {};
  GuestBlobMessage decoded;
  CHECK_EQ(decoded.count(), 0u);
  CHECK(decoded.extent(0u).length() == 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u, extents, 2u), Status::ok);
  CHECK_EQ(decoded.blob(), 3u);
  CHECK(decoded.size() == 0x3000u);
  CHECK_EQ(decoded.count(), 2u);
  CHECK_EQ(decoded.extent(0u).region(), 1u);
  CHECK(decoded.extent(0u).offset() == 0x1000u);
  CHECK(decoded.extent(1u).length() == 0x2000u);
  CHECK(decoded.extent(2u).length() == 0u);
  CHECK_EQ(extents[1].region(), 2u); }

TEST(guest_blob_message, the_most_extents_fit_one_datagram_and_round_trip)
{ static RendererExtent extents[GuestBlobMessage::max_extents];
  static RendererExtent decoded_extents[GuestBlobMessage::max_extents];
  static std::uint8_t   bytes[RendererMessageHeader::max_body_length];
  for (std::uint32_t index = 0u; index < GuestBlobMessage::max_extents; index++)
      extents[index] = RendererExtent(index % 3u, 0x2000u * index, 0x1000u);
  const GuestBlobMessage blob(9u, 0x1000ull * GuestBlobMessage::max_extents, extents, GuestBlobMessage::max_extents);
  CHECK(blob.encoded_length() <= sizeof bytes);
  CHECK_STATUS(blob.encode(bytes, sizeof bytes), Status::ok);
  GuestBlobMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, blob.encoded_length(), 0u, decoded_extents, GuestBlobMessage::max_extents),
               Status::ok);
  CHECK_EQ(decoded.count(), GuestBlobMessage::max_extents);
  CHECK(decoded.extent(GuestBlobMessage::max_extents - 1u).offset() == 0x2000ull * (GuestBlobMessage::max_extents - 1u)); }

TEST(guest_blob_message_encode, refuses_what_decode_would)
{ static RendererExtent many[GuestBlobMessage::max_extents + 1u];
  static std::uint8_t   bytes[GuestBlobMessage::fixed_size + sizeof many];
  for (RendererExtent &extent : many) extent = RendererExtent(0u, 0u, 1u);
  const RendererExtent unfit[] = {RendererExtent(0u, RendererExtent::region_span - 1u, 2u)};
  const RendererExtent overshooting[] = {RendererExtent(0u, 0u, RendererExtent::region_span),
                                     RendererExtent(1u, 0u, RendererExtent::region_span)};
  CHECK_STATUS(GuestBlobMessage(0u, 0x3000u, two_regions, 2u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(GuestBlobMessage(3u, 0u, two_regions, 2u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(GuestBlobMessage(3u, 0x3000u, two_regions, 0u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(GuestBlobMessage(3u, 0x3000u, nullptr, 2u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(GuestBlobMessage(3u, GuestBlobMessage::max_extents + 1u, many, GuestBlobMessage::max_extents + 1u)
                   .encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(GuestBlobMessage(3u, 2u, unfit, 1u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(GuestBlobMessage(3u, 0x2FFFu, two_regions, 2u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(GuestBlobMessage(3u, 0x3001u, two_regions, 2u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(GuestBlobMessage(3u, RendererExtent::region_span, overshooting, 2u).encode(bytes, sizeof bytes),
               Status::invalid_parameter); }

TEST(guest_blob_message, a_null_extent_array_is_none)
{ const GuestBlobMessage blob(3u, 0x3000u, nullptr, 5u);
  CHECK_EQ(blob.count(), 0u);
  CHECK(blob.extent(0u).length() == 0u); }

TEST(guest_blob_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(two_length - 1u);
  CHECK_STATUS(sample().encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(sample().encode(nullptr, two_length), Status::too_large); }

TEST(guest_blob_message_decode, more_extents_than_the_caller_holds_is_no_space)
{ std::uint8_t bytes[two_length] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  RendererExtent   extents[1] = {};
  GuestBlobMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u, extents, 1u), Status::no_space);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u, nullptr, 2u), Status::no_space);
  CHECK_EQ(decoded.count(), 0u); }

TEST(guest_blob_message_decode, any_descriptor_or_a_length_other_than_the_count_gives_is_malformed)
{ std::uint8_t bytes[two_length + 1u] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  RendererExtent   extents[2] = {};
  GuestBlobMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, two_length, 1u, extents, 2u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, two_length - 1u, 0u, extents, 2u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, two_length + 1u, 0u, extents, 2u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, GuestBlobMessage::fixed_size - 1u, 0u, extents, 2u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, two_length, 0u, extents, 2u), Status::malformed);
  CHECK_EQ(decoded.count(), 0u); }

TEST(guest_blob_message_decode, a_count_past_the_bound_is_malformed)
{ static std::uint8_t bytes[GuestBlobMessage::fixed_size +
                            (GuestBlobMessage::max_extents + 1u) * RendererExtent::encoded_size];
  static RendererExtent extents[GuestBlobMessage::max_extents + 1u];
  put(bytes, 0u, 4u, 3u);
  put(bytes, 4u, 4u, GuestBlobMessage::max_extents + 1u);
  GuestBlobMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u, extents, GuestBlobMessage::max_extents + 1u),
               Status::malformed); }

TEST(guest_blob_message_decode, refuses_bad_fields_and_stays_unchanged)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  const Case cases[] = {
      {0u, 4u, 0u},
      {8u, 8u, 0u},
      {8u, 8u, 0x2FFFu},
      {8u, 8u, 0x3001u},
      {24u, 8u, 0u},
      {16u, 8u, (std::uint64_t{1} << 40) | 0xFFFFFFFFFFull},
      {40u, 8u, ~std::uint64_t{0} - 0xFFFu},
  };
  for (const Case &c : cases) {
      std::uint8_t bytes[two_length] = {};
      CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
      put(bytes, c.offset, c.width, c.value);
      RendererExtent   extents[2] = {};
      const RendererExtent kept[] = {RendererExtent(7u, 0u, 5u)};
      GuestBlobMessage decoded(8u, 5u, kept, 1u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u, extents, 2u), Status::malformed);
      CHECK_EQ(decoded.blob(), 8u);
      CHECK_EQ(decoded.count(), 1u);
      CHECK_EQ(decoded.extent(0u).region(), 7u);
  } }

TEST(guest_blob_message_decode, no_extents_is_malformed)
{ std::uint8_t bytes[GuestBlobMessage::fixed_size] = {};
  put(bytes, 0u, 4u, 3u);
  put(bytes, 8u, 8u, 0x1000u);
  RendererExtent   extents[1] = {};
  GuestBlobMessage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u, extents, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u, nullptr, 0u), Status::malformed); }

TEST(guest_blob_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[two_length] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u);
  const std::uint32_t acceptable = 32u + 128u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
