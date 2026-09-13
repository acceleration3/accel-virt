/* One context's view of guest memory. Every offset and length asked of it came
 * off the wire, and OpenGL's decoder asks at more than a thousand sites. */
#include "device/executor/guest_region.hpp"
#include "harness/executor/guest_region_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::GuestRegion;
using av::GuestRegionProbe;

namespace {

std::uint8_t mapping[4096];

}  // namespace

TEST(guest_region, answers_inside_the_mapping_it_was_given)
{ GuestRegion region;
  region.take(mapping, sizeof mapping);
  CHECK_EQ(region.length(), sizeof mapping);
  CHECK(region.at(0u, sizeof mapping) == mapping);
  CHECK(region.at(16u, 32u) == mapping + 16u);
  /* The empty ask at the very end is inside it, which is what a zero-length
   * array on the wire looks like. */
  CHECK(region.at(sizeof mapping, 0u) == mapping + sizeof mapping);
  CHECK(region.invariants_hold()); }

TEST(guest_region, refuses_anything_past_the_end)
{ GuestRegion region;
  region.take(mapping, sizeof mapping);
  CHECK(region.at(0u, sizeof mapping + 1u) == nullptr);
  CHECK(region.at(sizeof mapping, 1u) == nullptr);
  CHECK(region.at(sizeof mapping + 1u, 0u) == nullptr);
  CHECK(region.at(16u, sizeof mapping) == nullptr); }

/* The numbers a guest would pick to make a bounds check pass by wrapping. */
TEST(guest_region, refuses_a_length_that_would_wrap)
{ GuestRegion region;
  region.take(mapping, sizeof mapping);
  CHECK(region.at(~std::uint64_t{0}, 1u) == nullptr);
  CHECK(region.at(1u, ~std::uint64_t{0}) == nullptr);
  CHECK(region.at(~std::uint64_t{0}, ~std::uint64_t{0}) == nullptr); }

/* A context with no region refuses every ask, which is what makes a command
 * that names one refused rather than guessed at. */
TEST(guest_region, with_no_mapping_it_answers_nothing)
{ GuestRegion region;
  CHECK_EQ(region.length(), 0u);
  CHECK(region.at(0u, 0u) == nullptr);
  CHECK(region.at(0u, 1u) == nullptr);
  CHECK(region.invariants_hold());

  region.take(nullptr, 4096u);
  CHECK_EQ(region.length(), 0u);
  CHECK(region.at(0u, 1u) == nullptr);
  CHECK(region.invariants_hold()); }

TEST(guest_region, giving_it_back_leaves_nothing_to_answer_with)
{ GuestRegion region;
  region.take(mapping, sizeof mapping);
  region.give_back();
  CHECK_EQ(region.length(), 0u);
  CHECK(region.at(0u, 1u) == nullptr);
  CHECK(region.invariants_hold()); }

/* A length with nothing behind it is a region that would answer bounds checks
 * for memory nobody mapped, if `at` ever stopped checking the base first. */
TEST(guest_region_invariants, a_length_with_no_mapping_behind_it_breaks_them)
{ GuestRegion region;
  GuestRegionProbe(region).set_length(sizeof mapping);
  CHECK(!region.invariants_hold()); }
