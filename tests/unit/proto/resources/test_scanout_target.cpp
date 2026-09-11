#include "harness/framework/test_registry.hpp"
#include "proto/resources/scanout_target.hpp"

using av::ScanoutTarget;

TEST(scanout_target, remembers_its_fields)
{ const ScanoutTarget target(2u, 60000u, 1001u);
  CHECK_EQ(target.source(), 2u);
  CHECK_EQ(target.refresh_numerator(), 60000u);
  CHECK_EQ(target.refresh_denominator(), 1001u); }

TEST(scanout_target_is_empty, only_when_every_field_is_zero)
{ CHECK(ScanoutTarget().is_empty());
  CHECK(!ScanoutTarget(1u, 0u, 0u).is_empty());
  CHECK(!ScanoutTarget(0u, 1u, 0u).is_empty());
  CHECK(!ScanoutTarget(0u, 0u, 1u).is_empty()); }
