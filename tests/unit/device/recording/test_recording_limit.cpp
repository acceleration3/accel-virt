#include "device/recording/recording_header.hpp"
#include "device/recording/recording_limit.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::AtCeiling;
using av::LimitVerdict;
using av::RecordingHeader;
using av::RecordingLimit;

namespace {

constexpr std::uint64_t header = RecordingHeader::encoded_size;

}  // namespace

TEST(recording_limit, a_ceiling_of_zero_never_refuses_a_record)
{ RecordingLimit limit(0u, AtCeiling::stop);
  CHECK(limit.verdict(~std::uint64_t{0}) == LimitVerdict::fits);
  limit.took(~std::uint64_t{0});
  CHECK(limit.invariants_hold());
  CHECK(limit.verdict(1u) == LimitVerdict::fits);
  CHECK(limit.written() == ~std::uint64_t{0}); }

TEST(recording_limit, the_default_ceiling_is_the_documented_one)
{ const RecordingLimit limit;
  CHECK(limit.ceiling() == RecordingLimit::default_ceiling);
  CHECK(limit.written() == 0u);
  CHECK(limit.invariants_hold()); }

TEST(recording_limit_verdict, a_record_that_exactly_reaches_the_ceiling_fits)
{ RecordingLimit limit(1024u, AtCeiling::stop);
  limit.took(header);
  CHECK(limit.invariants_hold());
  CHECK(limit.verdict(1024u - header) == LimitVerdict::fits);
  CHECK(limit.verdict(1024u - header + 1u) == LimitVerdict::stop);
  limit.took(1024u - header);
  CHECK(limit.written() == 1024u);
  CHECK(limit.invariants_hold());
  CHECK(limit.verdict(0u) == LimitVerdict::fits);
  CHECK(limit.verdict(1u) == LimitVerdict::stop); }

TEST(recording_limit_verdict, a_wrapping_recording_wraps_rather_than_stopping)
{ RecordingLimit limit(1024u, AtCeiling::wrap);
  limit.took(header);
  limit.took(1024u - header);
  CHECK(limit.verdict(1u) == LimitVerdict::wrap);
  limit.wrapped();
  CHECK(limit.written() == header);
  CHECK(limit.invariants_hold());
  CHECK(limit.verdict(1u) == LimitVerdict::fits); }

TEST(recording_limit_verdict, a_record_larger_than_the_whole_ceiling_stops_even_when_wrapping)
{ RecordingLimit limit(1024u, AtCeiling::wrap);
  limit.took(header);
  CHECK(limit.verdict(1024u - header) == LimitVerdict::fits);
  limit.took(1024u - header);
  /* Full: what still fits after a wrap wraps, and what does not stops. */
  CHECK(limit.verdict(1024u - header) == LimitVerdict::wrap);
  CHECK(limit.verdict(1024u - header + 1u) == LimitVerdict::stop);
  CHECK(limit.verdict(~std::uint64_t{0}) == LimitVerdict::stop); }

TEST(recording_limit_verdict, a_ceiling_below_the_file_header_leaves_nowhere_to_wrap_to)
{ RecordingLimit limit(16u, AtCeiling::wrap);
  limit.took(16u);
  CHECK(limit.invariants_hold());
  CHECK(limit.verdict(1u) == LimitVerdict::stop); }

TEST(recording_limit_verdict, a_record_that_would_overflow_the_count_stops)
{ RecordingLimit limit(~std::uint64_t{0} - 1u, AtCeiling::stop);
  limit.took(~std::uint64_t{0} - 1u);
  CHECK(limit.invariants_hold());
  CHECK(limit.verdict(~std::uint64_t{0}) == LimitVerdict::stop);
  CHECK(limit.verdict(1u) == LimitVerdict::stop);
  CHECK(limit.verdict(0u) == LimitVerdict::fits); }

TEST(recording_limit_invariants, catch_a_count_that_passed_the_ceiling)
{ RecordingLimit limit(1024u, AtCeiling::stop);
  CHECK(limit.invariants_hold());
  /* Nothing on the recording's own path does this - every write asks the
   * verdict first - so this is what the invariant is for. */
  limit.took(1025u);
  CHECK(!limit.invariants_hold());
  limit.wrapped();
  CHECK(limit.invariants_hold()); }
