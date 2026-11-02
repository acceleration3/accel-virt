#include "executor/vulkan/handle_registry.hpp"
#include "harness/executor/handle_registry_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::HandleRegistry;
using av::HandleRegistryProbe;
using av::MockPlatform;
using av::Status;

namespace {

constexpr std::uint64_t none = 0u;
constexpr std::uint64_t instance = 0x1000u;
constexpr std::uint64_t physical = 0x2000u;
constexpr std::uint64_t device = 0x3000u;
constexpr std::uint64_t pool = 0x4000u;
constexpr std::uint64_t command_buffer = 0x5000u;
constexpr std::uint64_t buffer = 0x6000u;

void make_chain(HandleRegistry &registry)
{ CHECK(registry.remember(instance, none) == Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(physical, instance) == Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(device, physical) == Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(pool, device) == Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(command_buffer, pool) == Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(buffer, device) == Status::ok);
  CHECK(registry.invariants_hold()); }

}

TEST(handle_registry, knows_only_what_it_made)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  CHECK(!registry.knows(0x1234u));
  CHECK(registry.remember(0x1234u, none) == Status::ok);
  CHECK(registry.knows(0x1234u));
  CHECK(!registry.knows(0x1235u));
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, the_null_handle_is_nobody_s_and_always_known)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  CHECK(registry.knows(0u));
  CHECK(registry.remember(0u, none) == Status::ok);
  CHECK_EQ(registry.count(), 0u);
  registry.forget(0u);
  registry.forget_children(0u);
  CHECK(registry.knows(0u));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, forgetting_one_takes_it_out_and_leaves_the_rest)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  for (std::uint64_t handle = 1u; handle <= 4u; handle++) CHECK(registry.remember(handle, none) == Status::ok);
  registry.forget(2u);
  CHECK(registry.invariants_hold());
  CHECK(!registry.knows(2u));
  CHECK(registry.knows(1u));
  CHECK(registry.knows(3u));
  CHECK(registry.knows(4u));
  CHECK_EQ(registry.count(), 3u);
  registry.forget(0xFFFFu);
  registry.forget_children(0xFFFFu);
  CHECK_EQ(registry.count(), 3u);
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, remembering_the_same_one_twice_costs_one_slot)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  CHECK(registry.remember(7u, none) == Status::ok);
  CHECK(registry.remember(7u, none) == Status::ok);
  CHECK_EQ(registry.count(), 1u);
  registry.forget(7u);
  CHECK(!registry.knows(7u));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, forgetting_a_parent_forgets_everything_made_through_it)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  make_chain(registry);
  CHECK(registry.remember(0x7000u, none) == Status::ok);
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
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  make_chain(registry);
  registry.forget(instance);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.count(), 0u);
  CHECK(!registry.knows(command_buffer)); }

TEST(handle_registry, forgetting_children_keeps_the_parent_and_takes_every_depth)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
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

TEST(handle_registry, forgetting_a_child_leaves_its_parent_and_siblings)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  make_chain(registry);
  registry.forget(command_buffer);
  CHECK(registry.invariants_hold());
  CHECK(registry.knows(pool));
  CHECK(registry.knows(buffer));
  CHECK_EQ(registry.count(), 5u); }

TEST(handle_registry, a_handle_made_through_one_it_does_not_know_has_no_parent)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  CHECK(registry.remember(device, none) == Status::ok);
  CHECK(registry.remember(buffer, 0xDEADu) == Status::ok);
  registry.forget(device);
  CHECK(registry.invariants_hold());
  CHECK(registry.knows(buffer)); }

TEST(handle_registry, a_handle_answered_again_keeps_its_first_parent)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  make_chain(registry);
  CHECK(registry.remember(buffer, instance) == Status::ok);
  registry.forget(device);
  CHECK(!registry.knows(buffer));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, a_reused_slot_is_nobody_s_parent_from_before)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  make_chain(registry);
  registry.forget(pool);
  CHECK(registry.invariants_hold());
  CHECK(registry.remember(0x8000u, none) == Status::ok);
  CHECK(registry.remember(0x9000u, 0x8000u) == Status::ok);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.count(), 6u);
  registry.forget(0x8000u);
  CHECK(!registry.knows(0x9000u));
  CHECK(registry.knows(buffer));
  CHECK(registry.knows(device));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, a_grandchild_below_its_parent_is_still_forgotten)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  constexpr std::uint64_t root = 40u, child = 30u, grandchild = 20u;
  CHECK(registry.remember(1u, none) == Status::ok);
  CHECK(registry.remember(2u, none) == Status::ok);
  CHECK(registry.remember(3u, none) == Status::ok);
  CHECK(registry.remember(root, none) == Status::ok);
  registry.forget(3u);
  CHECK(registry.remember(child, root) == Status::ok);
  registry.forget(2u);
  CHECK(registry.remember(grandchild, child) == Status::ok);
  CHECK(registry.invariants_hold());
  registry.forget(root);
  CHECK(registry.invariants_hold());
  CHECK(!registry.knows(child));
  CHECK(!registry.knows(grandchild));
  CHECK(registry.knows(1u));
  CHECK_EQ(registry.count(), 1u); }

TEST(handle_registry, holds_a_hundred_thousand_handles_and_forgets_them_through_their_parent)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  constexpr std::uint64_t many = 100000u;
  CHECK(registry.remember(device, none, av::HandleKind::device) == Status::ok);
  for (std::uint64_t handle = 1u; handle <= many; handle++)
      CHECK(registry.remember(0x100000u + handle * 16u, device) == Status::ok);
  CHECK_EQ(registry.count(), many + 1u);
  CHECK(registry.invariants_hold());
  for (std::uint64_t handle = 1u; handle <= many; handle++) CHECK(registry.knows(0x100000u + handle * 16u));
  CHECK(registry.knows(device, av::HandleKind::device));
  registry.forget(device);
  CHECK_EQ(registry.count(), 0u);
  CHECK(!registry.knows(0x100000u + many * 16u));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, room_is_made_before_and_refused_without_forgetting_anything)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  make_chain(registry);
  CHECK(registry.make_room(1000u) == Status::ok);
  platform.fail_allocation_at(1u);
  for (std::uint64_t handle = 1u; handle <= 1000u; handle++)
      CHECK(registry.remember(0x10000u + handle, device) == Status::ok);
  CHECK_EQ(registry.count(), 1006u);
  std::uint64_t handle = 0x20000u;
  while (registry.remember(handle, device) == Status::ok) handle++;
  CHECK(!registry.knows(handle));
  CHECK(registry.knows(buffer));
  CHECK(registry.invariants_hold());
  CHECK(registry.make_room(1u) == Status::ok);
  CHECK(registry.remember(handle, device) == Status::ok);
  CHECK(registry.make_room(av::KeyedSlots<std::uint64_t, av::HandleSlot>::most_entries) == Status::no_memory);
  CHECK(registry.make_room(~std::uint64_t{0}) == Status::no_memory);
  CHECK(registry.knows(handle));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, a_handle_answered_again_needs_no_room)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  make_chain(registry);
  platform.fail_allocation_at(1u);
  CHECK(registry.remember(physical, instance) == Status::ok);
  CHECK(registry.remember(0u, instance) == Status::ok);
  CHECK_EQ(registry.count(), 6u);
  platform.fail_allocation_at(0u); }

TEST(handle_registry, forgetting_everything_leaves_a_registry_that_knows_nothing)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  make_chain(registry);
  registry.forget(pool);
  registry.forget_all();
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.count(), 0u);
  CHECK(!registry.knows(buffer));
  CHECK(registry.knows(0u));
  CHECK(registry.remember(buffer, device) == Status::ok);
  registry.forget(device);
  CHECK(registry.knows(buffer));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, a_handle_is_known_only_as_the_kind_it_was_made)
{ MockPlatform   platform;
  HandleRegistry registry(platform);
  CHECK(registry.remember(0x1000u, 0u, av::HandleKind::device) == Status::ok);
  CHECK(registry.remember(0x2000u, 0u, av::HandleKind::queue) == Status::ok);
  CHECK(registry.remember(0x3000u, 0u) == Status::ok);
  CHECK(registry.knows(0x1000u, av::HandleKind::device));
  CHECK(!registry.knows(0x1000u, av::HandleKind::queue));
  CHECK(!registry.knows(0x1000u, av::HandleKind::other));
  CHECK(registry.knows(0x2000u, av::HandleKind::queue));
  CHECK(registry.knows(0x3000u, av::HandleKind::other));
  CHECK(!registry.knows(0x3000u, av::HandleKind::device));
  CHECK(!registry.knows(0x4000u, av::HandleKind::device));
  CHECK(registry.knows(0u));
  CHECK(registry.remember(0x6000u, 0u, av::HandleKind::external_image) == Status::ok);
  constexpr av::HandleKind kinds[] = {av::HandleKind::other, av::HandleKind::instance,
                                      av::HandleKind::physical_device, av::HandleKind::device,
                                      av::HandleKind::queue, av::HandleKind::command_buffer,
                                      av::HandleKind::external_image};
  for (const av::HandleKind kind : kinds) CHECK(!registry.knows(0u, kind));
  HandleRegistry empty(platform);
  CHECK(!empty.knows(0u, av::HandleKind::external_image));
  registry.forget(0x1000u);
  CHECK(registry.remember(0x5000u, 0u, av::HandleKind::instance) == Status::ok);
  CHECK(registry.knows(0x5000u, av::HandleKind::instance));
  CHECK(!registry.knows(0x5000u, av::HandleKind::device));
  CHECK(registry.invariants_hold()); }

TEST(handle_registry_invariants, a_table_the_operations_never_leave_breaks_them)
{ MockPlatform platform;
  { HandleRegistry registry(platform);
    make_chain(registry);
    HandleRegistryProbe probe(registry);
    probe.set_parent(pool, probe.slot_of(pool));
    CHECK(!registry.invariants_hold()); }
  { HandleRegistry registry(platform);
    make_chain(registry);
    HandleRegistryProbe probe(registry);
    probe.set_parent(buffer, 4096u);
    CHECK(!registry.invariants_hold()); }
  { HandleRegistry registry(platform);
    make_chain(registry);
    HandleRegistryProbe probe(registry);
    probe.set_children(device, 1u);
    CHECK(!registry.invariants_hold()); }
  { HandleRegistry registry(platform);
    make_chain(registry);
    HandleRegistryProbe probe(registry);
    probe.hold_null();
    CHECK(!registry.invariants_hold()); } }
