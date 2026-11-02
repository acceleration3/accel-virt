#include "miniport/memory/allocation.hpp"
#include "miniport/memory/resource.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::Allocation;
using av::MockPlatform;
using av::Resource;
using av::Status;

TEST(resource, counts_the_allocations_created_into_it)
{ MockPlatform platform;
  Resource     resource(platform);
  CHECK_EQ(resource.allocations(), 0u);
  resource.took_one();
  resource.took_one();
  CHECK_EQ(resource.allocations(), 2u);
  CHECK_STATUS(resource.released_one(), Status::ok);
  CHECK_EQ(resource.allocations(), 1u);
  CHECK_STATUS(resource.released_one(), Status::ok);
  CHECK_EQ(resource.allocations(), 0u); }

TEST(resource, releasing_one_it_never_held_is_refused)
{ MockPlatform platform;
  Resource     resource(platform);
  CHECK_STATUS(resource.released_one(), Status::invalid_state);
  CHECK_EQ(resource.allocations(), 0u);
  resource.took_one();
  CHECK_STATUS(resource.released_one(), Status::ok);
  CHECK_STATUS(resource.released_one(), Status::invalid_state);
  CHECK_EQ(resource.allocations(), 0u); }

TEST(resource, is_apc_at_most)
{ MockPlatform platform;
  Resource     resource(platform);
  platform.set_irql(av::Irql::dispatch);
  resource.took_one();
  static_cast<void>(resource.released_one());
  platform.set_irql(av::Irql::passive);
  CHECK(platform.claim(av::Violation::irql_ceiling));
  CHECK(platform.claim(av::Violation::irql_ceiling)); }
