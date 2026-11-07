#include "harness/framework/test_registry.hpp"
#include "proto/vulkan/fence_wake.hpp"

#include <cstdint>

using av::vkproto::FenceWake;

TEST(fence_wake, the_arming_timeout_is_one_below_forever_and_only_it_arms)
{ const FenceWake wake;
  CHECK_EQ(wake.arm_timeout(), ~std::uint64_t{0} - 1u);
  CHECK(wake.arms(wake.arm_timeout()));
  CHECK(!wake.arms(0u));
  CHECK(!wake.arms(~std::uint64_t{0}));
  CHECK(!wake.arms(1000000000u)); }
