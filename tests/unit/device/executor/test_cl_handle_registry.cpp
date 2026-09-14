/* The OpenCL objects this executor made, by kind. What is asserted is what a
 * guest cannot be allowed to break: a number the host did not hand out, or
 * handed out as a different kind of object, or whose last reference the guest
 * gave back, names nothing the host ICD will be given. */
#include "device/executor/cl_handle_registry.hpp"
#include "harness/executor/cl_handle_registry_probe.hpp"
#include "harness/executor/recording_cl_dispatch.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"

#include <cstdint>

using av::ClHandleRegistry;
using av::ClHandleRegistryProbe;
using av::RecordingClDispatch;
using av::WireArena;
using av::clproto::HandleKind;

namespace {

/* Objects the "ICD" made: distinct addresses, never dereferenced. */
unsigned char objects[ClHandleRegistry::max_handles + 2u];

void *object(std::size_t index) { return &objects[index]; }

std::uint64_t wire(void *host) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(host)); }

}  // namespace

TEST(cl_handle_registry, a_made_object_resolves_back_from_its_wire_value)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::mem, object(0)));
  CHECK(registry.resolve(HandleKind::mem, wire(object(0))) == object(0));
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, a_wire_value_never_made_resolves_to_null)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::mem, object(0)));
  CHECK(registry.resolve(HandleKind::mem, wire(object(1))) == nullptr);
  /* One byte off a real object is not that object. */
  CHECK(registry.resolve(HandleKind::mem, wire(object(0)) + 1u) == nullptr);
  CHECK(registry.resolve(HandleKind::mem, ~std::uint64_t{0}) == nullptr); }

/* A cl_mem named where a cl_context belongs would be read by the ICD as a
 * context: the kind is part of what the guest must get right. */
TEST(cl_handle_registry, an_object_of_another_kind_resolves_to_null)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::mem, object(0)));
  CHECK(registry.resolve(HandleKind::context, wire(object(0))) == nullptr);
  CHECK(registry.resolve(HandleKind::event, wire(object(0))) == nullptr);
  CHECK(registry.resolve(HandleKind::mem, wire(object(0))) == object(0)); }

/* Zero is the API's own null. The decoder decides whether a null may pass;
 * the registry never answers an object for it. */
TEST(cl_handle_registry, zero_resolves_to_null_and_remembering_null_holds_nothing)
{ ClHandleRegistry registry;
  CHECK(registry.resolve(HandleKind::platform, 0u) == nullptr);
  CHECK(registry.remember(HandleKind::platform, nullptr));
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.resolve(HandleKind::platform, 0u) == nullptr);
  CHECK(registry.invariants_hold()); }

/* Released to zero references, the ICD is free to free it: the guest's next
 * use of the number would be a use after free in the host. */
TEST(cl_handle_registry, the_last_release_forgets_it)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::kernel, object(0)));
  registry.release(HandleKind::kernel, object(0));
  CHECK(registry.resolve(HandleKind::kernel, wire(object(0))) == nullptr);
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.invariants_hold()); }

/* A library that retains an object and later releases it has not released
 * the application's reference with it. */
TEST(cl_handle_registry, a_retain_keeps_it_through_one_release)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::context, object(0)));
  registry.retain(HandleKind::context, object(0));
  registry.release(HandleKind::context, object(0));
  CHECK(registry.resolve(HandleKind::context, wire(object(0))) == object(0));
  CHECK(registry.invariants_hold());
  registry.release(HandleKind::context, object(0));
  CHECK(registry.resolve(HandleKind::context, wire(object(0))) == nullptr);
  CHECK(registry.invariants_hold()); }

/* Retaining or releasing what it never made, or as the wrong kind, changes
 * nothing it holds. */
TEST(cl_handle_registry, retain_and_release_of_an_unknown_or_mistyped_object_change_nothing)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::program, object(0)));
  registry.release(HandleKind::kernel, object(0));
  CHECK(registry.resolve(HandleKind::program, wire(object(0))) == object(0));
  registry.retain(HandleKind::program, object(1));
  registry.release(HandleKind::program, object(1));
  CHECK(registry.resolve(HandleKind::program, wire(object(1))) == nullptr);
  CHECK_EQ(registry.count(), 1u);
  /* A mistyped retain added no reference: one release still frees it. */
  registry.retain(HandleKind::kernel, object(0));
  registry.release(HandleKind::program, object(0));
  CHECK(registry.resolve(HandleKind::program, wire(object(0))) == nullptr);
  CHECK(registry.invariants_hold()); }

/* Enumeration answers the same platform every time. That is not a new
 * reference: one release still takes it out. */
TEST(cl_handle_registry, remembering_the_same_object_again_adds_no_reference)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::device, object(0)));
  CHECK(registry.remember(HandleKind::device, object(0)));
  CHECK_EQ(registry.count(), 1u);
  registry.release(HandleKind::device, object(0));
  CHECK(registry.resolve(HandleKind::device, wire(object(0))) == nullptr); }

/* The ICD answering an address this host still holds as another kind means
 * the host no longer knows which object a guest's number names. */
TEST(cl_handle_registry, an_address_already_held_as_another_kind_is_refused)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::mem, object(0)));
  CHECK(!registry.remember(HandleKind::event, object(0)));
  CHECK(registry.resolve(HandleKind::event, wire(object(0))) == nullptr);
  CHECK(registry.resolve(HandleKind::mem, wire(object(0))) == object(0));
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold()); }

/* Past its capacity the registry refuses rather than forgets, and what it
 * refused is not something a guest can name. */
TEST(cl_handle_registry, past_its_capacity_it_refuses_and_the_refused_one_resolves_to_null)
{ ClHandleRegistry registry;
  for (std::size_t i = 0u; i < ClHandleRegistry::max_handles; i++)
      CHECK(registry.remember(HandleKind::event, object(i)));
  CHECK_EQ(registry.count(), ClHandleRegistry::max_handles);
  CHECK(!registry.remember(HandleKind::event, object(ClHandleRegistry::max_handles)));
  CHECK(registry.resolve(HandleKind::event, wire(object(ClHandleRegistry::max_handles))) == nullptr);
  CHECK(registry.resolve(HandleKind::event, wire(object(ClHandleRegistry::max_handles - 1u))) ==
        object(ClHandleRegistry::max_handles - 1u));
  CHECK(registry.invariants_hold());
  /* A slot given back is a slot the next one gets. */
  registry.release(HandleKind::event, object(7u));
  CHECK(registry.remember(HandleKind::event, object(ClHandleRegistry::max_handles)));
  CHECK(registry.resolve(HandleKind::event, wire(object(ClHandleRegistry::max_handles))) ==
        object(ClHandleRegistry::max_handles));
  CHECK(registry.resolve(HandleKind::event, wire(object(8u))) == object(8u));
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry, forgetting_everything_leaves_nothing_resolvable)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::sampler, object(0)));
  CHECK(registry.remember(HandleKind::command_queue, object(1)));
  registry.forget_all();
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.resolve(HandleKind::sampler, wire(object(0))) == nullptr);
  CHECK(registry.resolve(HandleKind::command_queue, wire(object(1))) == nullptr);
  CHECK(registry.invariants_hold()); }

/* A reference count saturates rather than wraps: a wrapped count would be a
 * live object the next release forgets, which is safe, but zero references in
 * the table is an entry nothing can ever take out. */
TEST(cl_handle_registry, a_saturated_reference_count_stays_held)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::mem, object(0)));
  ClHandleRegistryProbe(registry).set_references(0u, ~std::uint32_t{0});
  registry.retain(HandleKind::mem, object(0));
  registry.release(HandleKind::mem, object(0));
  CHECK(registry.resolve(HandleKind::mem, wire(object(0))) == object(0));
  CHECK(registry.invariants_hold()); }

/* A context that dies still holding objects gives the ICD back every reference
 * the guest held, dependents before what they depend on, and then names
 * nothing. Made here in creation order, which is the opposite of the order
 * they must go back in. */
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
  ClHandleRegistry registry;
  for (std::size_t i = 0u; i < sizeof made / sizeof made[0]; i++) {
      CHECK(registry.remember(made[i].kind, object(i)));
      for (std::uint32_t n = 0u; n < made[i].retains; n++) registry.retain(made[i].kind, object(i));
      /* Held by the recording too, so a release handed the wrong object or the
       * wrong kind counts as a stranger. */
      icd.made(made[i].kind, object(i));
  }
  /* A count that saturated no longer says how many the guest holds: one. */
  ClHandleRegistryProbe(registry).set_references(9u, ~std::uint32_t{0});

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
  ClHandleRegistry registry;
  registry.release_all(icd);
  CHECK_EQ(icd.calls(), 0u);
  CHECK(registry.invariants_hold()); }

TEST(cl_handle_registry_invariants, a_count_past_the_table_breaks_them)
{ ClHandleRegistry registry;
  ClHandleRegistryProbe(registry).set_count(ClHandleRegistry::max_handles + 1u);
  CHECK(!registry.invariants_hold()); }

/* Null is never held: it would be an entry every null a guest sends matches. */
TEST(cl_handle_registry_invariants, a_null_object_in_the_table_breaks_them)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::mem, object(0)));
  ClHandleRegistryProbe(registry).set_object(0u, 0u);
  CHECK(!registry.invariants_hold()); }

/* An entry with no references left is one no release will ever take out. */
TEST(cl_handle_registry_invariants, an_entry_with_no_references_breaks_them)
{ ClHandleRegistry registry;
  CHECK(registry.remember(HandleKind::mem, object(0)));
  CHECK(registry.remember(HandleKind::mem, object(1)));
  ClHandleRegistryProbe(registry).set_references(1u, 0u);
  CHECK(!registry.invariants_hold()); }
