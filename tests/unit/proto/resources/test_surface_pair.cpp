#include "harness/framework/test_registry.hpp"
#include "proto/resources/surface_pair.hpp"

#include <cstdint>

using av::SurfacePair;

TEST(surface_pair, names_a_copy_only_with_both_a_source_and_a_destination)
{ const SurfacePair none;
  CHECK_EQ(none.source(), 0u);
  CHECK_EQ(none.destination(), 0u);
  CHECK(!none.names());
  const SurfacePair pair(0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
  CHECK_EQ(pair.source(), 0x0123456789ABCDEFull);
  CHECK_EQ(pair.destination(), 0xFEDCBA9876543210ull);
  CHECK(pair.names());
  CHECK(!SurfacePair(5u, 0u).names());
  CHECK(!SurfacePair(0u, 5u).names()); }

TEST(surface_pair, is_equal_only_in_both_halves)
{ CHECK(SurfacePair(4u, 9u) == SurfacePair(4u, 9u));
  CHECK(!(SurfacePair(4u, 9u) == SurfacePair(5u, 9u)));
  CHECK(!(SurfacePair(4u, 9u) == SurfacePair(4u, 8u)));
  CHECK(SurfacePair() == SurfacePair(0u, 0u)); }
