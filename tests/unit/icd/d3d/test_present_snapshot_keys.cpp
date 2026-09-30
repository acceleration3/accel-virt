/* A device's present snapshot keys: never 0, never the same twice from one
 * device, and not where another device's start. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/present_snapshot_keys.hpp"

#include <cstdint>

using av::d3d::PresentSnapshotKeys;

TEST(present_snapshot_keys, one_device_s_keys_are_new_each_present_and_never_zero)
{ PresentSnapshotKeys keys;
  std::uint64_t seen[64] = {};
  for (std::uint32_t index = 0u; index < 64u; index++) {
      seen[index] = keys.next();
      CHECK(seen[index] != 0u);
      for (std::uint32_t before = 0u; before < index; before++) CHECK(seen[before] != seen[index]);
  } }

TEST(present_snapshot_keys, two_devices_draw_bases_of_their_own)
{ PresentSnapshotKeys first;
  PresentSnapshotKeys second;
  const std::uint64_t a = first.next();
  const std::uint64_t b = second.next();
  /* Two draws of 64 random bits agree once in 2^64: this is a failed draw. */
  CHECK(a != b);
  CHECK(a - b > 1000u && b - a > 1000u); }
