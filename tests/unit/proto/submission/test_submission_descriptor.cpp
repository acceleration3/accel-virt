#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::RingHeader;
using av::Status;
using av::SubmissionDescriptor;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    SubmissionDescriptor descriptor;
    const Status status = descriptor.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[SubmissionDescriptor::encoded_size] = {};
        if (descriptor.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(submission_descriptor_encode, places_every_field_at_its_offset)
{ std::uint8_t bytes[SubmissionDescriptor::encoded_size] = {};
  CHECK_STATUS(SubmissionDescriptor(9u, 100u, 164u).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x534B5652u);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == 9u);
  CHECK(layout.at(16u, 8u) == 100u);
  CHECK(layout.at(24u, 8u) == 164u); }

TEST(submission_descriptor, round_trips_through_its_encoding)
{ std::uint8_t bytes[SubmissionDescriptor::encoded_size] = {};
  CHECK_STATUS(SubmissionDescriptor(9u, 100u, 164u).encode(bytes, sizeof bytes), Status::ok);
  SubmissionDescriptor decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(decoded.fence_id() == 9u);
  CHECK(decoded.begin() == 100u);
  CHECK(decoded.end() == 164u);
  CHECK(decoded.length() == 64u); }

TEST(submission_descriptor_decode, rejects_each_malformed_descriptor)
{ const SubmissionDescriptor bad[] = {
      SubmissionDescriptor(0u, 0u, 1u),     /* fence id 0 */
      SubmissionDescriptor(1u, 5u, 5u),     /* empty batch */
      SubmissionDescriptor(1u, 6u, 5u),     /* end before begin */
  };
  for (const SubmissionDescriptor &descriptor : bad) {
      std::uint8_t bytes[SubmissionDescriptor::encoded_size] = {};
      CHECK_STATUS(descriptor.encode(bytes, sizeof bytes), Status::ok);
      SubmissionDescriptor decoded(3u, 1u, 2u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK(decoded.fence_id() == 3u);
  }
  std::uint8_t bytes[SubmissionDescriptor::encoded_size] = {};
  CHECK_STATUS(SubmissionDescriptor(1u, 0u, 1u).encode(bytes, sizeof bytes), Status::ok);
  bytes[0] = 0u;
  SubmissionDescriptor decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  bytes[0] = 0x52u; bytes[5] = 1u;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed); }

TEST(submission_descriptor_validate_against, a_batch_up_to_the_ring_capacity_fits)
{ const RingHeader ring(12u, 0u, 0u);
  CHECK_STATUS(SubmissionDescriptor(1u, 10u, 10u + 4096u).validate_against(ring), Status::ok);
  CHECK_STATUS(SubmissionDescriptor(1u, 10u, 10u + 4097u).validate_against(ring), Status::invalid_state); }

TEST(submission_descriptor_validate_against, a_span_near_2_to_64_does_not_overflow)
{ const RingHeader ring(24u, 0u, 0u);
  CHECK_STATUS(SubmissionDescriptor(1u, 0u, ~std::uint64_t{0}).validate_against(ring), Status::invalid_state); }

TEST(submission_descriptor_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(SubmissionDescriptor::encoded_size - 1u);
  CHECK_STATUS(SubmissionDescriptor(1u, 0u, 1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(submission_descriptor_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[SubmissionDescriptor::encoded_size] = {};
  CHECK_STATUS(SubmissionDescriptor(7u, 40u, 88u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u);
  /* The descriptor constrains its magic and reserved word, forbids fence zero
   * and demands a non-empty span. The fence's 64 bits are free, because 7 has
   * three bits set and no single flip can clear them. Of the begin counter
   * only the six flips that keep 40 under the 88-byte end survive - 8, 32, 41,
   * 42, 44 and 56 - and of the end counter every flip but the one that drops
   * 88 to 24, below begin. Every shorter input is short of 32 bytes, all-zero
   * and all-ones carry no magic, and the untouched input is the last
   * acceptance. */
  const std::uint32_t acceptable = 64u + 6u + 63u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
