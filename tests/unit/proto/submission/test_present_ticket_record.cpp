/* A present's ticket as the miniport writes it beside the DMA buffer and reads
 * it back at submission: the fields at their offsets, a round trip, and
 * private data that is anything else - another magic, no epoch, no ticket, too
 * few bytes - refused with the reader unchanged. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/present_ticket_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::PresentTicketRecord;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    PresentTicketRecord record;
    const Status status = record.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[PresentTicketRecord::encoded_size] = {};
        if (record.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(present_ticket_record_encode, places_every_field_at_its_offset)
{ std::uint8_t bytes[PresentTicketRecord::encoded_size] = {};
  CHECK_STATUS(PresentTicketRecord(0x11223344u, 0x5566778899AABBCCull).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x544B5652u);   /* "RVKT" */
  CHECK(layout.at(4u, 4u) == 0x11223344u);
  CHECK(layout.at(8u, 8u) == 0x5566778899AABBCCull); }

TEST(present_ticket_record, round_trips_and_reads_only_its_own_bytes_of_longer_private_data)
{ std::uint8_t bytes[32] = {};
  std::memset(bytes, 0xEE, sizeof bytes);
  CHECK_STATUS(PresentTicketRecord(2u, 9u).encode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(bytes[16], 0xEEu);
  PresentTicketRecord decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);   /* an rvk context's private data is longer */
  CHECK_EQ(decoded.epoch(), 2u);
  CHECK(decoded.ticket() == 9u);
  const PresentTicketRecord none;
  CHECK_EQ(none.epoch(), 0u);
  CHECK(none.ticket() == 0u); }

TEST(present_ticket_record_decode, rejects_a_bad_magic_no_epoch_no_ticket_or_too_few_bytes_unchanged)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  const Case cases[] = {{0u, 4u, 0x544B5653u}, {4u, 4u, 0u}, {8u, 8u, 0u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[PresentTicketRecord::encoded_size] = {};
      CHECK_STATUS(PresentTicketRecord(7u, 9u).encode(bytes, sizeof bytes), Status::ok);
      for (std::size_t i = 0u; i < c.width; i++) bytes[c.offset + i] = static_cast<std::uint8_t>(c.value >> (8u * i));
      PresentTicketRecord decoded(3u, 4u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK_EQ(decoded.epoch(), 3u);
      CHECK(decoded.ticket() == 4u);
  }
  std::uint8_t bytes[PresentTicketRecord::encoded_size] = {};
  CHECK_STATUS(PresentTicketRecord(7u, 9u).encode(bytes, sizeof bytes), Status::ok);
  PresentTicketRecord decoded;
  CHECK_STATUS(decoded.decode(bytes, PresentTicketRecord::encoded_size - 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, PresentTicketRecord::encoded_size), Status::malformed);
  CHECK(decoded.ticket() == 0u); }

TEST(present_ticket_record_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(PresentTicketRecord::encoded_size - 1u);
  CHECK_STATUS(PresentTicketRecord(7u, 9u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(present_ticket_record_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[PresentTicketRecord::encoded_size] = {};
  CHECK_STATUS(PresentTicketRecord(3u, 3u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u);
  /* Only the magic is constrained beyond non-zero: epoch and ticket are 3, so
   * no single flip clears either, and all 32 + 64 of their flips are records. */
  const std::uint32_t acceptable = 32u + 64u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
