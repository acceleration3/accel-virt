#include "core/support/status.hpp"
#include "device/recording/recording_header.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/protocol_version.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::ProtocolVersion;
using av::RecordingHeader;
using av::Status;

namespace {

const std::uint8_t sample_uuid[RecordingHeader::uuid_bytes] = {
    0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u,
    0x18u, 0x19u, 0x1Au, 0x1Bu, 0x1Cu, 0x1Du, 0x1Eu, 0x1Fu};

RecordingHeader a_header()
{
    return RecordingHeader(ProtocolVersion(1u, 1u), 0x3Fu, 0x0102030405060708u, 1u << 20,
                           0x10DEu, 0x2204u, sample_uuid);
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    /* Loaded with a header a test can recognise: a refusal must leave it as it
     * was, which is what "unchanged unless ok" promises every caller. */
    RecordingHeader header = a_header();
    const Status    status = header.decode(bytes, length);
    if (status != Status::ok) {
        if (header.limit() != (1u << 20) || header.kinds() != 0x3Fu || header.wraps() != 0u)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                        "a refused header was changed anyway");
        return true;
    }
    if (!header.invariants_hold())
        av::TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                    "an accepted header broke its invariant");
    std::uint8_t again[RecordingHeader::encoded_size] = {};
    if (header.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
        av::TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                    "accepted input did not round trip");
    return false;
}

}  // namespace

namespace av {

struct RecordingHeaderProbe {
    /* A major no reader accepts, which nothing but a probe can put there. */
    static void set_major(RecordingHeader &header, std::uint16_t major) { header.major_ = major; }
};

}  // namespace av

TEST(recording_header_encode, places_every_field_at_its_offset)
{ std::uint8_t bytes[RecordingHeader::encoded_size] = {};
  CHECK_STATUS(a_header().encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x524B5652u);
  CHECK(layout.at(4u, 2u) == RecordingHeader::current_major);
  CHECK(layout.at(6u, 2u) == RecordingHeader::current_minor);
  CHECK(layout.at(8u, 2u) == 1u);
  CHECK(layout.at(10u, 2u) == 1u);
  CHECK(layout.at(12u, 4u) == 0x3Fu);
  CHECK(layout.at(16u, 8u) == 0x0102030405060708u);
  CHECK(layout.at(24u, 8u) == (1u << 20));
  CHECK(layout.at(32u, 4u) == 0u);
  CHECK(layout.at(36u, 4u) == 0x10DEu);
  CHECK(layout.at(40u, 4u) == 0x2204u);
  CHECK(layout.at(44u, 4u) == 0u);
  CHECK(std::memcmp(bytes + 48, sample_uuid, sizeof sample_uuid) == 0); }

TEST(recording_header, round_trips_through_its_encoding)
{ std::uint8_t bytes[RecordingHeader::encoded_size] = {};
  RecordingHeader header = a_header();
  header.wrapped();
  header.wrapped();
  CHECK(header.invariants_hold());
  CHECK_STATUS(header.encode(bytes, sizeof bytes), Status::ok);
  RecordingHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.major_version(), RecordingHeader::current_major);
  CHECK_EQ(decoded.minor_version(), RecordingHeader::current_minor);
  CHECK_EQ(decoded.wire_version().major_version(), 1u);
  CHECK_EQ(decoded.wire_version().minor_version(), 1u);
  CHECK_EQ(decoded.kinds(), 0x3Fu);
  CHECK(decoded.started() == 0x0102030405060708u);
  CHECK(decoded.limit() == (1u << 20));
  CHECK_EQ(decoded.wraps(), 2u);
  CHECK_EQ(decoded.vendor_id(), 0x10DEu);
  CHECK_EQ(decoded.device_id(), 0x2204u);
  CHECK(std::memcmp(decoded.uuid(), sample_uuid, sizeof sample_uuid) == 0); }

TEST(recording_header, a_device_bound_to_no_gpu_records_sixteen_zeroes)
{ const RecordingHeader header(ProtocolVersion(1u, 0u), 0u, 0u, 0u, 0u, 0u, nullptr);
  const std::uint8_t zeroes[RecordingHeader::uuid_bytes] = {};
  CHECK(std::memcmp(header.uuid(), zeroes, sizeof zeroes) == 0);
  CHECK(header.limit() == 0u);
  CHECK(header.invariants_hold()); }

TEST(recording_header_wrapped, saturates_rather_than_counting_backwards)
{ RecordingHeader header = a_header();
  std::uint8_t bytes[RecordingHeader::encoded_size] = {};
  CHECK_STATUS(header.encode(bytes, sizeof bytes), Status::ok);
  bytes[32] = 0xFFu; bytes[33] = 0xFFu; bytes[34] = 0xFFu; bytes[35] = 0xFFu;
  CHECK_STATUS(header.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(header.wraps(), 0xFFFFFFFFu);
  header.wrapped();
  CHECK_EQ(header.wraps(), 0xFFFFFFFFu);
  CHECK(header.invariants_hold()); }

TEST(recording_header_invariants, a_ceiling_smaller_than_the_header_is_not_a_ceiling)
{ const RecordingHeader header(ProtocolVersion(1u, 1u), 0u, 0u, RecordingHeader::encoded_size - 1u,
                               0u, 0u, sample_uuid);
  CHECK(!header.invariants_hold());
  GuardedBuffer buffer(RecordingHeader::encoded_size);
  CHECK_STATUS(header.encode(buffer.data(), buffer.size()), Status::invalid_parameter);
  CHECK(buffer.guards_intact());
  const RecordingHeader smallest(ProtocolVersion(1u, 1u), 0u, 0u, RecordingHeader::encoded_size,
                                 0u, 0u, sample_uuid);
  CHECK(smallest.invariants_hold()); }

TEST(recording_header_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(RecordingHeader::encoded_size - 1u);
  CHECK_STATUS(a_header().encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(recording_header_decode, refuses_a_major_it_does_not_know)
{ std::uint8_t bytes[RecordingHeader::encoded_size] = {};
  CHECK_STATUS(a_header().encode(bytes, sizeof bytes), Status::ok);
  bytes[4] = 2u;
  RecordingHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::not_supported);
  bytes[4] = 0u;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::not_supported); }

TEST(recording_header_decode, reads_a_minor_above_its_own_as_far_as_it_knows)
{ std::uint8_t bytes[RecordingHeader::encoded_size] = {};
  CHECK_STATUS(a_header().encode(bytes, sizeof bytes), Status::ok);
  bytes[6] = 9u;
  RecordingHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.minor_version(), 9u);
  CHECK_EQ(decoded.kinds(), 0x3Fu);
  CHECK(decoded.invariants_hold()); }

TEST(recording_header_decode, rejects_a_bad_magic_a_reserved_field_and_a_ceiling_below_the_header)
{ struct Case { std::size_t offset; std::uint8_t value; };
  const Case cases[] = {{0u, 0u}, {3u, 0x53u}, {44u, 1u}, {45u, 1u}, {46u, 1u}, {47u, 1u}};
  for (const Case &c : cases) {
      std::uint8_t bytes[RecordingHeader::encoded_size] = {};
      CHECK_STATUS(a_header().encode(bytes, sizeof bytes), Status::ok);
      bytes[c.offset] = c.value;
      RecordingHeader decoded = a_header();
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK(decoded.limit() == (1u << 20));
  }
  std::uint8_t bytes[RecordingHeader::encoded_size] = {};
  CHECK_STATUS(a_header().encode(bytes, sizeof bytes), Status::ok);
  for (std::size_t index = 24u; index < 32u; index++) bytes[index] = 0u;
  bytes[24] = RecordingHeader::encoded_size - 1u;
  RecordingHeader decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  bytes[24] = RecordingHeader::encoded_size;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(decoded.limit() == RecordingHeader::encoded_size); }

TEST(recording_header_decode, a_short_buffer_is_malformed)
{ std::uint8_t bytes[RecordingHeader::encoded_size] = {};
  CHECK_STATUS(a_header().encode(bytes, sizeof bytes), Status::ok);
  RecordingHeader decoded;
  for (std::size_t length = 0u; length < sizeof bytes; length++)
      CHECK_STATUS(decoded.decode(bytes, length), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, sizeof bytes), Status::malformed); }

TEST(recording_header_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[RecordingHeader::encoded_size] = {};
  CHECK_STATUS(a_header().encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  /* Every truncation, every bit flipped, and the all-zero and all-ones cases:
   * a sweep that quietly stopped short would still be a passing sweep. */
  CHECK_EQ(sweep.cases_run(), sizeof bytes + 1u + 8u * sizeof bytes + 2u);
  /* The format constrains the magic, the major it was written by and the
   * reserved word; everything else a reader takes as written. The minor, both
   * wire versions, the kinds mask, the start time, the wrap count, the two PCI
   * ids and the UUID are 46 free bytes, 368 acceptable bits, and the 64 bits of
   * the ceiling are acceptable too: clearing the only set bit of 1 MiB leaves
   * "no ceiling" and every other flip leaves a ceiling above the header. Each
   * shorter input is short of 64 bytes, all-zero and all-ones carry no magic,
   * and the untouched input is the one acceptance left. */
  const std::uint32_t acceptable = 368u + 64u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }

TEST(recording_header_invariants, catch_a_major_this_writer_never_wrote)
{ RecordingHeader header = a_header();
  CHECK(header.invariants_hold());
  av::RecordingHeaderProbe::set_major(header, RecordingHeader::current_major + 1u);
  CHECK(!header.invariants_hold());
  GuardedBuffer buffer(RecordingHeader::encoded_size);
  CHECK_STATUS(header.encode(buffer.data(), buffer.size()), Status::invalid_parameter);
  CHECK(buffer.guards_intact());
  av::RecordingHeaderProbe::set_major(header, RecordingHeader::current_major);
  CHECK(header.invariants_hold()); }
