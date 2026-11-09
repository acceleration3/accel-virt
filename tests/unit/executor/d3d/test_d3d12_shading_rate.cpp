#include <initializer_list>
#include "executor/d3d/d3d12_shading_rate.hpp"
#include "harness/framework/test_registry.hpp"

using av::D3d12ShadingRate;

TEST(d3d12_shading_rate, the_rates_every_tier_one_device_has_are_accepted_and_nothing_else)
{ for (const unsigned rate : {0x0u, 0x1u, 0x4u, 0x5u}) CHECK(D3d12ShadingRate(rate, 0u, 0u).holds(1u, false, false));
  for (const unsigned rate : {0x2u, 0x3u, 0x6u, 0x9u, 0xAu, 0x10u, 0xFFFFFFFFu})
      CHECK(!D3d12ShadingRate(rate, 0u, 0u).holds(1u, false, false)); }

TEST(d3d12_shading_rate, the_additional_rates_need_the_host_to_offer_them)
{ for (const unsigned rate : {0x6u, 0x9u, 0xAu}) {
      CHECK(!D3d12ShadingRate(rate, 0u, 0u).holds(2u, false, false));
      CHECK(D3d12ShadingRate(rate, 0u, 0u).holds(2u, true, false));
  } }

TEST(d3d12_shading_rate, combiners_past_the_five_are_refused_and_the_sum_needs_its_cap_and_a_second_tier_needs_tier_two)
{ CHECK(!D3d12ShadingRate(0u, 5u, 0u).holds(2u, true, true));
  CHECK(!D3d12ShadingRate(0u, 0u, 0xFFFFFFFFu).holds(2u, true, true));
  CHECK(!D3d12ShadingRate(0u, 4u, 0u).holds(2u, true, false));
  CHECK(D3d12ShadingRate(0u, 4u, 4u).holds(2u, true, true));
  CHECK(D3d12ShadingRate(0u, 1u, 3u).holds(2u, false, false));
  CHECK(!D3d12ShadingRate(0u, 1u, 0u).holds(1u, false, false));
  CHECK(!D3d12ShadingRate(0u, 0u, 0u).holds(0u, true, true)); }
