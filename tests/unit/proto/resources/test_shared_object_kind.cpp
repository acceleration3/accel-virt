/* The kind a shared object is. Every value outside the three the protocol has
 * must be unknown, because "unknown" is what every table on both sides refuses
 * on, and a fence must never answer the question memory answers. */
#include "harness/framework/test_registry.hpp"
#include "proto/resources/shared_object_kind.hpp"

#include <cstdint>

using av::SharedObjectKind;

TEST(shared_object_kind, the_three_the_protocol_has_are_known_and_nothing_else_is)
{ CHECK(SharedObjectKind(SharedObjectKind::memory).known());
  CHECK(SharedObjectKind(SharedObjectKind::monitored_fence).known());
  CHECK(SharedObjectKind(SharedObjectKind::native_fence).known());
  const std::uint32_t refused[] = {0u, 4u, 5u, 0x80000000u, 0xFFFFFFFFu};
  for (const std::uint32_t value : refused) CHECK(!SharedObjectKind(value).known()); }

TEST(shared_object_kind, a_default_kind_is_unknown_so_an_unset_field_is_never_taken_for_memory)
{ const SharedObjectKind kind;
  CHECK(!kind.known());
  CHECK(!kind.is_memory());
  CHECK(!kind.is_fence());
  CHECK_EQ(kind.value(), 0u); }

TEST(shared_object_kind, memory_is_memory_and_neither_fence_is)
{ CHECK(SharedObjectKind(SharedObjectKind::memory).is_memory());
  CHECK(!SharedObjectKind(SharedObjectKind::memory).is_fence());
  CHECK(!SharedObjectKind(SharedObjectKind::monitored_fence).is_memory());
  CHECK(!SharedObjectKind(SharedObjectKind::native_fence).is_memory()); }

TEST(shared_object_kind, both_fence_kinds_are_fences_because_the_host_holds_one_semaphore_for_either)
{ CHECK(SharedObjectKind(SharedObjectKind::monitored_fence).is_fence());
  CHECK(SharedObjectKind(SharedObjectKind::native_fence).is_fence());
  CHECK(!SharedObjectKind(4u).is_fence()); }

TEST(shared_object_kind, memory_is_whole_non_zero_pages_and_a_fence_has_no_size)
{ const SharedObjectKind memory(SharedObjectKind::memory);
  CHECK(memory.sized(SharedObjectKind::page_size));
  CHECK(memory.sized(3u * SharedObjectKind::page_size));
  CHECK(!memory.sized(0u));
  CHECK(!memory.sized(SharedObjectKind::page_size + 1u));
  CHECK(!memory.sized(SharedObjectKind::page_size - 1u));
  const std::uint32_t fences[] = {SharedObjectKind::monitored_fence, SharedObjectKind::native_fence};
  for (const std::uint32_t fence : fences) {
      CHECK(SharedObjectKind(fence).sized(0u));
      CHECK(!SharedObjectKind(fence).sized(SharedObjectKind::page_size));
  } }

TEST(shared_object_kind, two_kinds_are_the_same_only_when_their_values_are)
{ CHECK(SharedObjectKind(SharedObjectKind::memory).same_as(SharedObjectKind(SharedObjectKind::memory)));
  CHECK(!SharedObjectKind(SharedObjectKind::monitored_fence)
             .same_as(SharedObjectKind(SharedObjectKind::native_fence)));
  CHECK(SharedObjectKind(7u).same_as(SharedObjectKind(7u)));
  CHECK(!SharedObjectKind().same_as(SharedObjectKind(SharedObjectKind::memory))); }

TEST(shared_object_kind, the_value_is_carried_through_unchanged_so_a_refusal_can_name_it)
{ CHECK_EQ(SharedObjectKind(0xDEADBEEFu).value(), 0xDEADBEEFu); }
