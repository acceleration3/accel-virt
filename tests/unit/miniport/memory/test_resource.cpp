/* A WDDM resource: the object D3DKMTShareObjects mints an NT handle over, and
 * the count of allocations created into it (feature 053, research.md §16).
 * What it must get right is the count, because the resource goes with the last
 * allocation in it and a count that drifts either leaks the resource or takes
 * it while dxgkrnl still names it. */
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

/* took_one and released_one are what a create and a destroy of one allocation
 * do to the resource they name. */
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

/* A release of an allocation this resource never held: refused rather than
 * wrapped, because a count that went round would read as a resource holding
 * four billion allocations and never be destroyed. */
TEST(resource, releasing_one_it_never_held_is_refused)
{ MockPlatform platform;
  Resource     resource(platform);
  CHECK_STATUS(resource.released_one(), Status::invalid_state);
  CHECK_EQ(resource.allocations(), 0u);
  resource.took_one();
  CHECK_STATUS(resource.released_one(), Status::ok);
  CHECK_STATUS(resource.released_one(), Status::invalid_state);
  CHECK_EQ(resource.allocations(), 0u); }

/* Both are at APC at most, like everything the allocation table reaches under
 * its own mutex. */
TEST(resource, is_apc_at_most)
{ MockPlatform platform;
  Resource     resource(platform);
  platform.set_irql(av::Irql::dispatch);
  resource.took_one();
  static_cast<void>(resource.released_one());
  platform.set_irql(av::Irql::passive);
  CHECK(platform.claim(av::Violation::irql_ceiling));
  CHECK(platform.claim(av::Violation::irql_ceiling)); }
