/* Every host handle this executor made, and nothing else. What a test asserts
 * here is the property a guest cannot be allowed to break: a number this host
 * did not produce is not a handle. */
#include "device/executor/handle_registry.hpp"
#include "harness/executor/handle_registry_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::HandleRegistry;
using av::HandleRegistryProbe;

TEST(handle_registry, knows_only_what_it_made)
{ HandleRegistry registry;
  CHECK(!registry.knows(0x1234u));
  CHECK(registry.remember(0x1234u));
  CHECK(registry.knows(0x1234u));
  CHECK(!registry.knows(0x1235u));
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold()); }

/* The null handle is the API's own: a guest may legitimately send it, and
 * owning it would mean refusing every optional parameter. */
TEST(handle_registry, the_null_handle_is_nobody_s_and_always_known)
{ HandleRegistry registry;
  CHECK(registry.knows(0u));
  CHECK(registry.remember(0u));
  CHECK_EQ(registry.count(), 0u);
  registry.forget(0u);
  CHECK(registry.knows(0u));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, forgetting_one_takes_it_out_and_leaves_the_rest)
{ HandleRegistry registry;
  for (std::uint64_t handle = 1u; handle <= 4u; handle++) CHECK(registry.remember(handle));
  registry.forget(2u);
  CHECK(!registry.knows(2u));
  CHECK(registry.knows(1u));
  CHECK(registry.knows(3u));
  CHECK(registry.knows(4u));
  CHECK_EQ(registry.count(), 3u);
  /* Forgetting one it never made changes nothing. */
  registry.forget(0xFFFFu);
  CHECK_EQ(registry.count(), 3u);
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, remembering_the_same_one_twice_costs_one_slot)
{ HandleRegistry registry;
  CHECK(registry.remember(7u));
  CHECK(registry.remember(7u));
  CHECK_EQ(registry.count(), 1u);
  registry.forget(7u);
  CHECK(!registry.knows(7u)); }

/* A guest that makes more objects than a context may hold is refused rather
 * than quietly forgotten, because a forgotten handle is one the host would
 * later refuse to use on the guest's behalf. */
TEST(handle_registry, a_full_registry_refuses_rather_than_forgets)
{ HandleRegistry registry;
  for (std::uint64_t handle = 1u; handle <= HandleRegistry::max_handles; handle++)
      CHECK(registry.remember(handle));
  CHECK_EQ(registry.count(), HandleRegistry::max_handles);
  CHECK(!registry.remember(0xFFFFFFFFu));
  CHECK(!registry.knows(0xFFFFFFFFu));
  CHECK(registry.knows(1u));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, forgetting_everything_leaves_a_registry_that_knows_nothing)
{ HandleRegistry registry;
  for (std::uint64_t handle = 1u; handle <= 8u; handle++) CHECK(registry.remember(handle));
  registry.forget_all();
  CHECK_EQ(registry.count(), 0u);
  CHECK(!registry.knows(4u));
  CHECK(registry.knows(0u));
  CHECK(registry.invariants_hold()); }

/* A count past the table is one `knows` would walk off the end of, reading
 * whatever lies beyond as handles this host made. Every slot is filled first,
 * so nothing but the count is wrong. */
TEST(handle_registry_invariants, a_count_past_the_table_breaks_them)
{ HandleRegistry registry;
  for (std::uint64_t handle = 1u; handle <= HandleRegistry::max_handles; handle++)
      CHECK(registry.remember(handle));
  HandleRegistryProbe(registry).set_count(HandleRegistry::max_handles + 1u);
  CHECK(!registry.invariants_hold()); }

/* The null handle is always known without being held; one held is a slot
 * `forget(0)` can never give back. */
TEST(handle_registry_invariants, a_null_handle_in_the_table_breaks_them)
{ HandleRegistry registry;
  CHECK(registry.remember(1u));
  CHECK(registry.remember(2u));
  HandleRegistryProbe(registry).set_handle(1u, 0u);
  CHECK(!registry.invariants_hold()); }
