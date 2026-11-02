#include "executor/opencl/cl_handle_registry.hpp"
#include "harness/executor/cl_handle_registry_probe.hpp"
#include "harness/executor/recording_cl_dispatch.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"

#include <cstdint>

using av::ClHandleRegistry;
using av::ClHandleRegistryProbe;
using av::MockPlatform;
using av::Status;
using av::RecordingClDispatch;
using av::WireArena;
using av::clproto::HandleKind;

namespace {
unsigned char objects[10002u];

void *object(std::size_t index) { return &objects[index]; }

std::uint64_t wire(void *host) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(host)); }

}

TEST(cl_handle_registry, a_made_object_resolves_back_from_its_wire_value)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::mem, object(0)) == Status::ok);
  CHECK(registry.resolve(HandleKind::mem, wire(object(0))) == object(0));
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, a_wire_value_never_made_resolves_to_null)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::mem, object(0)) == Status::ok);
  CHECK(registry.resolve(HandleKind::mem, wire(object(1))) == nullptr);
  CHECK(registry.resolve(HandleKind::mem, wire(object(0)) + 1u) == nullptr);
  CHECK(registry.resolve(HandleKind::mem, ~std::uint64_t{0}) == nullptr); }

TEST(cl_handle_registry, an_object_of_another_kind_resolves_to_null)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::mem, object(0)) == Status::ok);
  CHECK(registry.resolve(HandleKind::context, wire(object(0))) == nullptr);
  CHECK(registry.resolve(HandleKind::event, wire(object(0))) == nullptr);
  CHECK(registry.resolve(HandleKind::mem, wire(object(0))) == object(0)); }

TEST(cl_handle_registry, zero_resolves_to_null_and_remembering_null_holds_nothing)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.resolve(HandleKind::platform, 0u) == nullptr);
  CHECK(registry.remember(HandleKind::platform, nullptr) == Status::ok);
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.resolve(HandleKind::platform, 0u) == nullptr);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, the_last_release_forgets_it)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::kernel, object(0)) == Status::ok);
  registry.release(HandleKind::kernel, object(0));
  CHECK(registry.resolve(HandleKind::kernel, wire(object(0))) == nullptr);
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, a_retain_keeps_it_through_one_release)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::context, object(0)) == Status::ok);
  registry.retain(HandleKind::context, object(0));
  registry.release(HandleKind::context, object(0));
  CHECK(registry.resolve(HandleKind::context, wire(object(0))) == object(0));
  CHECK(registry.invariants_hold());
  registry.release(HandleKind::context, object(0));
  CHECK(registry.resolve(HandleKind::context, wire(object(0))) == nullptr);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, retain_and_release_of_an_unknown_or_mistyped_object_change_nothing)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::program, object(0)) == Status::ok);
  registry.release(HandleKind::kernel, object(0));
  CHECK(registry.resolve(HandleKind::program, wire(object(0))) == object(0));
  registry.retain(HandleKind::program, object(1));
  registry.release(HandleKind::program, object(1));
  CHECK(registry.resolve(HandleKind::program, wire(object(1))) == nullptr);
  CHECK_EQ(registry.count(), 1u);
  registry.retain(HandleKind::kernel, object(0));
  registry.release(HandleKind::program, object(0));
  CHECK(registry.resolve(HandleKind::program, wire(object(0))) == nullptr);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, remembering_the_same_object_again_adds_no_reference)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::device, object(0)) == Status::ok);
  CHECK(registry.remember(HandleKind::device, object(0)) == Status::ok);
  CHECK_EQ(registry.count(), 1u);
  registry.release(HandleKind::device, object(0));
  CHECK(registry.resolve(HandleKind::device, wire(object(0))) == nullptr); }

TEST(cl_handle_registry, an_address_already_held_as_another_kind_is_refused)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::mem, object(0)) == Status::ok);
  CHECK(registry.remember(HandleKind::event, object(0)) == Status::conflict);
  CHECK(registry.resolve(HandleKind::event, wire(object(0))) == nullptr);
  CHECK(registry.resolve(HandleKind::mem, wire(object(0))) == object(0));
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, holds_ten_thousand_objects_and_each_resolves)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  constexpr std::size_t many = 10000u;
  for (std::size_t i = 0u; i < many; i++) CHECK(registry.remember(HandleKind::mem, object(i)) == Status::ok);
  CHECK_EQ(registry.count(), many);
  for (std::size_t i = 0u; i < many; i++) CHECK(registry.resolve(HandleKind::mem, wire(object(i))) == object(i));
  CHECK(registry.resolve(HandleKind::mem, wire(object(many))) == nullptr);
  for (std::size_t i = 0u; i < many; i += 2u) registry.release(HandleKind::mem, object(i));
  CHECK_EQ(registry.count(), many / 2u);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, room_is_made_before_and_refused_without_forgetting_anything)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::context, object(0)) == Status::ok);
  CHECK(registry.make_room(100u) == Status::ok);
  platform.fail_allocation_at(1u);
  for (std::size_t i = 1u; i <= 100u; i++) CHECK(registry.remember(HandleKind::event, object(i)) == Status::ok);
  std::size_t next = 101u;
  while (registry.remember(HandleKind::event, object(next)) == Status::ok) next++;
  CHECK(registry.resolve(HandleKind::event, wire(object(next))) == nullptr);
  CHECK(registry.resolve(HandleKind::context, wire(object(0))) == object(0));
  CHECK(registry.make_room(1u) == Status::ok);
  CHECK(registry.make_room(av::KeyedSlots<std::uint64_t, av::ClHandle>::most_entries) == Status::no_memory);
  CHECK(registry.make_room(~std::uint64_t{0}) == Status::no_memory);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, forgetting_everything_leaves_nothing_resolvable)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::sampler, object(0)) == Status::ok);
  CHECK(registry.remember(HandleKind::command_queue, object(1)) == Status::ok);
  registry.forget_all();
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.resolve(HandleKind::sampler, wire(object(0))) == nullptr);
  CHECK(registry.resolve(HandleKind::command_queue, wire(object(1))) == nullptr);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, a_saturated_reference_count_stays_held)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::mem, object(0)) == Status::ok);
  ClHandleRegistryProbe(registry).set_references(object(0), ~std::uint32_t{0});
  registry.retain(HandleKind::mem, object(0));
  registry.release(HandleKind::mem, object(0));
  CHECK(registry.resolve(HandleKind::mem, wire(object(0))) == object(0));
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, releasing_everything_gives_back_each_reference_dependents_first)
{ struct Made {
      HandleKind kind;
      std::uint32_t retains;
  };
  constexpr Made made[] = {
      {HandleKind::platform, 0u}, {HandleKind::device, 0u},  {HandleKind::context, 1u},
      {HandleKind::command_queue, 0u}, {HandleKind::mem, 2u}, {HandleKind::program, 0u},
      {HandleKind::kernel, 0u},   {HandleKind::sampler, 0u}, {HandleKind::event, 0u},
      {HandleKind::mem, 0u},
  };
  std::uint8_t block[64];
  WireArena arena(block, sizeof block);
  RecordingClDispatch icd(block, arena);
  MockPlatform     platform;
  ClHandleRegistry registry(platform);
  for (std::size_t i = 0u; i < sizeof made / sizeof made[0]; i++) {
      CHECK(registry.remember(made[i].kind, object(i)) == Status::ok);
      for (std::uint32_t n = 0u; n < made[i].retains; n++) registry.retain(made[i].kind, object(i));
      icd.made(made[i].kind, object(i));
  }
  ClHandleRegistryProbe(registry).set_references(object(9), ~std::uint32_t{0});

  registry.release_all(icd);
  constexpr std::uint32_t expected[] = {
      av::clproto::kCmd_clReleaseEvent,        av::clproto::kCmd_clReleaseKernel,
      av::clproto::kCmd_clReleaseProgram,      av::clproto::kCmd_clReleaseSampler,
      av::clproto::kCmd_clReleaseMemObject,    av::clproto::kCmd_clReleaseMemObject,
      av::clproto::kCmd_clReleaseMemObject,    av::clproto::kCmd_clReleaseMemObject,
      av::clproto::kCmd_clReleaseCommandQueue, av::clproto::kCmd_clReleaseContext,
      av::clproto::kCmd_clReleaseContext,      av::clproto::kCmd_clReleaseDevice,
  };
  CHECK_EQ(icd.calls(), sizeof expected / sizeof expected[0]);
  for (std::size_t i = 0u; i < sizeof expected / sizeof expected[0]; i++)
      CHECK_EQ(icd.called_at(i), expected[i]);
  CHECK_EQ(icd.strangers(), 0u);
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.resolve(HandleKind::context, wire(object(2))) == nullptr);
  CHECK(registry.resolve(HandleKind::platform, wire(object(0))) == nullptr);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, releasing_everything_held_by_nobody_calls_nothing)
{ std::uint8_t block[64];
  WireArena arena(block, sizeof block);
  RecordingClDispatch icd(block, arena);
  MockPlatform     platform;
  ClHandleRegistry registry(platform);
  registry.release_all(icd);
  CHECK_EQ(icd.calls(), 0u);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry_invariants, a_null_object_in_the_table_breaks_them)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::mem, object(0)) == Status::ok);
  ClHandleRegistryProbe(registry).hold_null();
  CHECK(!registry.invariants_hold()); }

TEST(cl_handle_registry_invariants, an_entry_with_no_references_breaks_them)
{ MockPlatform     platform;
  ClHandleRegistry registry(platform);
  CHECK(registry.remember(HandleKind::mem, object(0)) == Status::ok);
  CHECK(registry.remember(HandleKind::mem, object(1)) == Status::ok);
  ClHandleRegistryProbe(registry).set_references(object(1), 0u);
  CHECK(!registry.invariants_hold()); }
