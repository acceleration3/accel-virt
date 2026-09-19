/* Every host handle this executor made, and nothing else. What a test asserts
 * here is the property a guest cannot be allowed to break: a number this host
 * did not produce is not a handle - and neither is one whose object, or whose
 * object's parent, the driver has since freed. */
#include "device/executor/handle_registry.hpp"
#include "harness/executor/handle_registry_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::HandleRegistry;
using av::HandleRegistryProbe;

namespace {

constexpr std::uint64_t none = 0u;
constexpr std::uint64_t instance = 0x1000u;
constexpr std::uint64_t physical = 0x2000u;
constexpr std::uint64_t device = 0x3000u;
constexpr std::uint64_t pool = 0x4000u;
constexpr std::uint64_t command_buffer = 0x5000u;
constexpr std::uint64_t buffer = 0x6000u;

/* The chain a guest's context really builds, each made through the last, and
 * a buffer beside the pool. */
void make_chain(HandleRegistry &registry)
{ CHECK(registry.remember(instance, none));
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(physical, instance));
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(device, physical));
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(pool, device));
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(command_buffer, pool));
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(buffer, device));
  CHECK(registry.invariants_hold()); }

}  // namespace

TEST(handle_registry, knows_only_what_it_made)
{ HandleRegistry registry;
  CHECK(!registry.knows(0x1234u));
  CHECK(registry.remember(0x1234u, none));
  CHECK(registry.knows(0x1234u));
  CHECK(!registry.knows(0x1235u));
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold()); }

/* The null handle is the API's own: a guest may legitimately send it, and
 * owning it would mean refusing every optional parameter. */
TEST(handle_registry, the_null_handle_is_nobody_s_and_always_known)
{ HandleRegistry registry;
  CHECK(registry.knows(0u));
  CHECK(registry.remember(0u, none));
  CHECK_EQ(registry.count(), 0u);
  registry.forget(0u);
  registry.forget_children(0u);
  CHECK(registry.knows(0u));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, forgetting_one_takes_it_out_and_leaves_the_rest)
{ HandleRegistry registry;
  for (std::uint64_t handle = 1u; handle <= 4u; handle++) CHECK(registry.remember(handle, none));
  registry.forget(2u);
  CHECK(registry.invariants_hold());
  CHECK(!registry.knows(2u));
  CHECK(registry.knows(1u));
  CHECK(registry.knows(3u));
  CHECK(registry.knows(4u));
  CHECK_EQ(registry.count(), 3u);
  /* Forgetting one it never made changes nothing. */
  registry.forget(0xFFFFu);
  registry.forget_children(0xFFFFu);
  CHECK_EQ(registry.count(), 3u);
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, remembering_the_same_one_twice_costs_one_slot)
{ HandleRegistry registry;
  CHECK(registry.remember(7u, none));
  CHECK(registry.remember(7u, none));
  CHECK_EQ(registry.count(), 1u);
  registry.forget(7u);
  CHECK(!registry.knows(7u));
  CHECK(registry.invariants_hold()); }

/* A driver that destroys a device frees everything made through it, at any
 * depth; a handle still known after that is a freed pointer a guest can name. */
TEST(handle_registry, forgetting_a_parent_forgets_everything_made_through_it)
{ HandleRegistry registry;
  make_chain(registry);
  CHECK(registry.remember(0x7000u, none));
  registry.forget(device);
  CHECK(registry.invariants_hold());
  CHECK(!registry.knows(device));
  CHECK(!registry.knows(pool));
  CHECK(!registry.knows(command_buffer));
  CHECK(!registry.knows(buffer));
  CHECK(registry.knows(physical));
  CHECK(registry.knows(instance));
  CHECK(registry.knows(0x7000u));
  CHECK_EQ(registry.count(), 3u); }

TEST(handle_registry, forgetting_the_root_forgets_the_whole_chain)
{ HandleRegistry registry;
  make_chain(registry);
  registry.forget(instance);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.count(), 0u);
  CHECK(!registry.knows(command_buffer)); }

/* A reset frees what a pool gave out and keeps the pool. */
TEST(handle_registry, forgetting_children_keeps_the_parent_and_takes_every_depth)
{ HandleRegistry registry;
  make_chain(registry);
  registry.forget_children(physical);
  CHECK(registry.invariants_hold());
  CHECK(registry.knows(physical));
  CHECK(registry.knows(instance));
  CHECK(!registry.knows(device));
  CHECK(!registry.knows(pool));
  CHECK(!registry.knows(command_buffer));
  CHECK(!registry.knows(buffer));
  CHECK_EQ(registry.count(), 2u); }

/* Forgetting a leaf leaves its parent and siblings. */
TEST(handle_registry, forgetting_a_child_leaves_its_parent_and_siblings)
{ HandleRegistry registry;
  make_chain(registry);
  registry.forget(command_buffer);
  CHECK(registry.invariants_hold());
  CHECK(registry.knows(pool));
  CHECK(registry.knows(buffer));
  CHECK_EQ(registry.count(), 5u); }

/* A parent it does not know - one a guest named through a create info that
 * the decoder let through as null - makes a handle with no parent, which only
 * forgetting it itself takes out. */
TEST(handle_registry, a_handle_made_through_one_it_does_not_know_has_no_parent)
{ HandleRegistry registry;
  CHECK(registry.remember(device, none));
  CHECK(registry.remember(buffer, 0xDEADu));
  registry.forget(device);
  CHECK(registry.invariants_hold());
  CHECK(registry.knows(buffer)); }

/* Remembering one already known keeps what it was made through: a queue
 * answered again by a second vkGetDeviceQueue is still its device's. */
TEST(handle_registry, a_handle_answered_again_keeps_its_first_parent)
{ HandleRegistry registry;
  make_chain(registry);
  CHECK(registry.remember(buffer, instance));
  registry.forget(device);
  CHECK(!registry.knows(buffer));
  CHECK(registry.invariants_hold()); }

/* A freed slot is reused, and a handle made into it is not taken for a child
 * of whatever the slot held before: nothing names a slot once it is free. */
TEST(handle_registry, a_reused_slot_is_nobody_s_parent_from_before)
{ HandleRegistry registry;
  make_chain(registry);
  registry.forget(pool);
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(0x8000u, none));
  CHECK(registry.remember(0x9000u, 0x8000u));
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.count(), 6u);
  registry.forget(0x8000u);
  CHECK(!registry.knows(0x9000u));
  CHECK(registry.knows(buffer));
  CHECK(registry.knows(device));
  CHECK(registry.invariants_hold()); }

/* A child in a lower slot than its parent - slots freed and reused - is still
 * found, which takes more than one pass over the table. */
TEST(handle_registry, a_grandchild_below_its_parent_is_still_forgotten)
{ HandleRegistry registry;
  constexpr std::uint64_t root = 40u, child = 30u, grandchild = 20u;
  CHECK(registry.remember(1u, none));
  CHECK(registry.remember(2u, none));
  CHECK(registry.remember(3u, none));
  CHECK(registry.remember(root, none));
  registry.forget(3u);
  CHECK(registry.remember(child, root));
  registry.forget(2u);
  CHECK(registry.remember(grandchild, child));
  CHECK(registry.invariants_hold());
  registry.forget(root);
  CHECK(registry.invariants_hold());
  CHECK(!registry.knows(child));
  CHECK(!registry.knows(grandchild));
  CHECK(registry.knows(1u));
  CHECK_EQ(registry.count(), 1u); }

/* A guest that makes more objects than a context may hold is refused rather
 * than quietly forgotten, because a forgotten handle is one the host would
 * later refuse to use on the guest's behalf. */
TEST(handle_registry, a_full_registry_refuses_rather_than_forgets)
{ HandleRegistry registry;
  for (std::uint64_t handle = 1u; handle <= HandleRegistry::max_handles; handle++)
      CHECK(registry.remember(handle, none));
  CHECK_EQ(registry.count(), HandleRegistry::max_handles);
  CHECK(!registry.remember(0xFFFFFFFFu, none));
  CHECK(!registry.knows(0xFFFFFFFFu));
  CHECK(registry.knows(1u));
  CHECK(registry.invariants_hold());
  /* And one freed in the middle is room again. */
  registry.forget(100u);
  CHECK(registry.remember(0xFFFFFFFFu, none));
  CHECK(registry.knows(0xFFFFFFFFu));
  CHECK_EQ(registry.count(), HandleRegistry::max_handles);
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, forgetting_everything_leaves_a_registry_that_knows_nothing)
{ HandleRegistry registry;
  make_chain(registry);
  registry.forget(pool);
  registry.forget_all();
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.count(), 0u);
  CHECK(!registry.knows(buffer));
  CHECK(registry.knows(0u));
  /* And it takes new handles from the first slot with no parent left over. */
  CHECK(registry.remember(buffer, device));
  registry.forget(device);
  CHECK(registry.knows(buffer));
  CHECK(registry.invariants_hold()); }

/* A table end past the table is one `knows` would walk off, reading whatever
 * lies beyond as handles this host made. */
/* The five handles the loader dispatches on are told apart by kind: it reads a
 * dispatchable handle as a pointer to the table it calls through, so a queue
 * where a device belongs aborts the process. A handle made as one kind is no
 * more the other than a number this registry never made. */
TEST(handle_registry, a_handle_is_known_only_as_the_kind_it_was_made)
{ HandleRegistry registry;
  CHECK(registry.remember(0x1000u, 0u, av::HandleKind::device));
  CHECK(registry.remember(0x2000u, 0u, av::HandleKind::queue));
  CHECK(registry.remember(0x3000u, 0u));
  CHECK(registry.knows(0x1000u, av::HandleKind::device));
  CHECK(!registry.knows(0x1000u, av::HandleKind::queue));
  CHECK(!registry.knows(0x1000u, av::HandleKind::other));
  CHECK(registry.knows(0x2000u, av::HandleKind::queue));
  CHECK(registry.knows(0x3000u, av::HandleKind::other));
  CHECK(!registry.knows(0x3000u, av::HandleKind::device));
  /* A number nobody made is no kind at all, and the null handle is every
   * kind: the API defines it and a guest may legitimately send it. */
  CHECK(!registry.knows(0x4000u, av::HandleKind::device));
  CHECK(registry.knows(0u, av::HandleKind::device));
  CHECK(registry.knows(0u, av::HandleKind::other));
  /* And a slot reused for another handle carries the new one's kind. */
  registry.forget(0x1000u);
  CHECK(registry.remember(0x5000u, 0u, av::HandleKind::instance));
  CHECK(registry.knows(0x5000u, av::HandleKind::instance));
  CHECK(!registry.knows(0x5000u, av::HandleKind::device));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry_invariants, an_end_past_the_table_breaks_them)
{ HandleRegistry registry;
  for (std::uint64_t handle = 1u; handle <= HandleRegistry::max_handles; handle++)
      CHECK(registry.remember(handle, none));
  HandleRegistryProbe(registry).set_end(HandleRegistry::max_handles + 1u);
  CHECK(!registry.invariants_hold()); }

/* Each of these is a table the operations could never leave, and each breaks
 * exactly one rule. */
TEST(handle_registry_invariants, a_table_the_operations_never_leave_breaks_them)
{ struct Corruption {
      std::uint32_t index;
      std::uint64_t handle;
      std::uint16_t parent;
      std::uint32_t count;
  };
  /* Slots 0..5 hold the chain: instance, physical, device, pool, command
   * buffer, buffer; the count is 6. */
  const Corruption corruptions[] = {
      /* A free slot at the end, which the sweep would have given back. */
      {5u, 0u, 0xFFFFu, 5u},
      /* A free slot naming a parent: a handle reused into it would be a child
       * of an object it never came from. */
      {4u, 0u, 2u, 5u},
      /* A parent past the end is a slot no sweep looks at. */
      {4u, command_buffer, 7u, 6u},
      {4u, command_buffer, 4u, 6u},
      /* A live handle whose parent is free outlives the object it was made
       * through - the freed pointer this registry exists to refuse. */
      {3u, 0u, 0xFFFFu, 5u},
      /* A count that disagrees with the table. */
      {4u, command_buffer, 3u, 5u},
  };
  for (const Corruption &corruption : corruptions) {
      HandleRegistry registry;
      make_chain(registry);
      HandleRegistryProbe probe(registry);
      probe.set_handle(corruption.index, corruption.handle);
      probe.set_parent(corruption.index, corruption.parent);
      probe.set_count(corruption.count);
      CHECK(!registry.invariants_hold());
  } }
