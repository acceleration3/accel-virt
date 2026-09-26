#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/renderer_message_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::RendererMessageHeader;
using av::Status;
using Kind = av::RendererMessageHeader::Kind;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    RendererMessageHeader header;
    const Status status = header.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[RendererMessageHeader::encoded_size] = {};
        if (header.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(renderer_message_header_encode, is_the_kind_the_body_length_and_the_sequence)
{ GuardedBuffer bytes(RendererMessageHeader::encoded_size);
  CHECK_STATUS(RendererMessageHeader(Kind::guest_blob, 0x1234u, 0x1122334455667788ull).encode(bytes.data(), bytes.size()),
               Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 4u);
  CHECK(layout.at(4u, 4u) == 0x1234u);
  CHECK(layout.at(8u, 8u) == 0x1122334455667788ull);
  CHECK(bytes.guards_intact()); }

/* The values are the protocol: a renderer built from another tree must read
 * the same kinds. */
TEST(renderer_message_header, kinds_are_numbered_from_one_in_protocol_order)
{ CHECK_EQ(static_cast<std::uint32_t>(Kind::hello), 1u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::ready), 2u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::region), 3u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::guest_blob), 4u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::host_blob), 5u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::blob_retired), 6u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::create_context), 7u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::bind_context), 8u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::destroy_context), 9u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::interrupt_lines), 10u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::interrupts_off), 11u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::reply), 12u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::export_image), 13u);
  CHECK_EQ(static_cast<std::uint32_t>(Kind::exported_image), 14u); }

TEST(renderer_message_header, round_trips_a_datagram_with_its_body)
{ std::uint8_t datagram[RendererMessageHeader::encoded_size + 3u] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::reply, 3u, ~std::uint64_t{0}).encode(datagram, sizeof datagram), Status::ok);
  RendererMessageHeader decoded;
  CHECK(decoded.kind() == static_cast<Kind>(0u));
  CHECK_STATUS(decoded.decode(datagram, sizeof datagram), Status::ok);
  CHECK(decoded.kind() == Kind::reply);
  CHECK_EQ(decoded.body_length(), 3u);
  CHECK(decoded.sequence() == ~std::uint64_t{0}); }

TEST(renderer_message_header, the_longest_body_is_accepted)
{ static std::uint8_t datagram[RendererMessageHeader::encoded_size + RendererMessageHeader::max_body_length];
  CHECK_STATUS(RendererMessageHeader(Kind::guest_blob, RendererMessageHeader::max_body_length, 1u)
                   .encode(datagram, sizeof datagram),
               Status::ok);
  RendererMessageHeader decoded;
  CHECK_STATUS(decoded.decode(datagram, sizeof datagram), Status::ok);
  CHECK_EQ(decoded.body_length(), RendererMessageHeader::max_body_length); }

TEST(renderer_message_header_encode, refuses_what_decode_would)
{ std::uint8_t bytes[RendererMessageHeader::encoded_size] = {};
  CHECK_STATUS(RendererMessageHeader(static_cast<Kind>(0u), 0u, 1u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(RendererMessageHeader(static_cast<Kind>(16u), 0u, 1u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(RendererMessageHeader(Kind::shared_released, 0u, 1u).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(RendererMessageHeader(Kind::hello, RendererMessageHeader::max_body_length + 1u, 1u)
                   .encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(RendererMessageHeader(Kind::interrupts_off, 1u, 1u).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(RendererMessageHeader(Kind::interrupts_off, 0u, 1u).encode(bytes, sizeof bytes), Status::ok); }

TEST(renderer_message_header_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(RendererMessageHeader::encoded_size - 1u);
  CHECK_STATUS(RendererMessageHeader(Kind::hello, 0u, 1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(RendererMessageHeader(Kind::hello, 0u, 1u).encode(nullptr, RendererMessageHeader::encoded_size),
               Status::too_large); }

/* A kind neither side knows would be dispatched nowhere; zero is a buffer
 * nobody wrote. */
TEST(renderer_message_header_decode, refuses_kinds_zero_and_past_shared_released_and_stays_unchanged)
{ CHECK_EQ(static_cast<std::uint32_t>(Kind::shared_released), 15u);
  std::uint8_t last[RendererMessageHeader::encoded_size] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::shared_released, 0u, 5u).encode(last, sizeof last), Status::ok);
  RendererMessageHeader known;
  CHECK_STATUS(known.decode(last, sizeof last), Status::ok);
  CHECK(known.kind() == Kind::shared_released);
  const std::uint32_t kinds[] = {0u, 16u, 0xFFFFFFFFu};
  for (std::uint32_t kind : kinds) {
      std::uint8_t bytes[RendererMessageHeader::encoded_size] = {};
      CHECK_STATUS(RendererMessageHeader(Kind::hello, 0u, 5u).encode(bytes, sizeof bytes), Status::ok);
      for (std::size_t i = 0u; i < 4u; i++) bytes[i] = static_cast<std::uint8_t>(kind >> (8u * i));
      RendererMessageHeader decoded(Kind::ready, 0u, 7u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK(decoded.kind() == Kind::ready);
      CHECK(decoded.sequence() == 7u);
  } }

/* A datagram cut short (MSG_TRUNC) or carrying bytes past its body is not the
 * message the header describes. */
TEST(renderer_message_header_decode, a_body_other_than_what_arrived_is_malformed)
{ std::uint8_t datagram[RendererMessageHeader::encoded_size + 9u] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::region, 8u, 1u).encode(datagram, sizeof datagram), Status::ok);
  RendererMessageHeader decoded;
  CHECK_STATUS(decoded.decode(datagram, RendererMessageHeader::encoded_size + 7u), Status::malformed);
  CHECK_STATUS(decoded.decode(datagram, RendererMessageHeader::encoded_size + 9u), Status::malformed);
  CHECK_STATUS(decoded.decode(datagram, RendererMessageHeader::encoded_size + 8u), Status::ok);
  CHECK_EQ(decoded.body_length(), 8u); }

TEST(renderer_message_header_decode, a_body_past_the_maximum_is_malformed_even_when_it_arrived)
{ static std::uint8_t datagram[RendererMessageHeader::encoded_size + RendererMessageHeader::max_body_length + 1u];
  CHECK_STATUS(RendererMessageHeader(Kind::guest_blob, RendererMessageHeader::max_body_length, 1u)
                   .encode(datagram, sizeof datagram),
               Status::ok);
  datagram[4] = static_cast<std::uint8_t>(datagram[4] + 1u);
  RendererMessageHeader decoded;
  CHECK_STATUS(decoded.decode(datagram, sizeof datagram), Status::malformed);
  CHECK_EQ(decoded.body_length(), 0u); }

TEST(renderer_message_header_decode, interrupts_off_carries_no_body)
{ std::uint8_t datagram[RendererMessageHeader::encoded_size + 1u] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::interrupts_off, 0u, 1u).encode(datagram, sizeof datagram), Status::ok);
  datagram[4] = 1u;
  RendererMessageHeader decoded;
  CHECK_STATUS(decoded.decode(datagram, sizeof datagram), Status::malformed);
  CHECK_STATUS(RendererMessageHeader(Kind::interrupts_off, 0u, 1u).encode(datagram, sizeof datagram), Status::ok);
  CHECK_STATUS(decoded.decode(datagram, RendererMessageHeader::encoded_size), Status::ok);
  CHECK(decoded.kind() == Kind::interrupts_off); }

TEST(renderer_message_header_decode, short_or_missing_bytes_are_malformed)
{ std::uint8_t bytes[RendererMessageHeader::encoded_size] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::hello, 0u, 1u).encode(bytes, sizeof bytes), Status::ok);
  RendererMessageHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, RendererMessageHeader::encoded_size - 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, RendererMessageHeader::encoded_size), Status::malformed);
  CHECK(decoded.kind() == static_cast<Kind>(0u)); }

TEST(renderer_message_header_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[RendererMessageHeader::encoded_size] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::hello, 0u, 0x0123456789ABCDEFull).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u);
  /* Kind 1 flipped at bit 1, 2 or 3 is kind 3, 5 or 9, all known and with a
   * body allowed; every other kind flip is 0 or past reply. Any body length
   * flip claims bytes that did not arrive. The sequence is free: 64 flips.
   * With the untouched header, 3 + 64 + 1 acceptances. */
  const std::uint32_t acceptable = 3u + 64u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
