/* The GL memory objects and semaphores one context imported from a descriptor.
 *
 * A GL name is a small integer the driver chooses, and the two name spaces are
 * separate: memory object 3 and semaphore 3 are different objects. So what is
 * asserted here is that a release reaches exactly the row that was made, of
 * exactly the kind it was made as - a guest naming a semaphore where a memory
 * object was imported would have the wrong entry point delete somebody's
 * object. */
#include "executor/opengl/gl_shared_objects.hpp"

#include "device/abi/av_executor.h"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::GlSharedObjects;

namespace av {

/* Rows a remember() never leaves behind, so the invariant has something to
 * refuse. */
struct GlSharedObjectsProbe {
    static void set_kind(GlSharedObjects &objects, std::uint32_t index, std::uint32_t kind)
    { objects.kinds_[index] = kind; }
    static void set_name(GlSharedObjects &objects, std::uint32_t index, std::uint32_t name)
    { objects.names_[index] = name; }
    static void set_count(GlSharedObjects &objects, std::uint32_t count) { objects.count_ = count; }
};

}  // namespace av

TEST(gl_shared_objects, a_new_table_holds_nothing)
{ GlSharedObjects objects;
  CHECK_EQ(objects.count(), 0u);
  std::uint32_t kind = 9u;
  std::uint32_t name = 9u;
  CHECK(!objects.take_any(kind, name));
  CHECK(!objects.forget(AV_SHARED_MEMORY, 1u, name));
  CHECK(objects.invariants_hold()); }

TEST(gl_shared_objects, a_remembered_name_is_found_by_its_own_kind_and_by_no_other)
{ GlSharedObjects objects;
  CHECK(objects.remember(AV_SHARED_MEMORY, 3u));
  CHECK_EQ(objects.count(), 1u);
  std::uint32_t name = 0u;
  CHECK(!objects.forget(AV_SHARED_MONITORED_FENCE, 3u, name));
  CHECK_EQ(name, 0u);
  CHECK_EQ(objects.count(), 1u);
  CHECK(objects.forget(AV_SHARED_MEMORY, 3u, name));
  CHECK_EQ(name, 3u);
  CHECK_EQ(objects.count(), 0u);
  CHECK(objects.invariants_hold()); }

/* The two name spaces really are separate, so one number is two rows. */
TEST(gl_shared_objects, one_number_is_a_memory_object_and_a_semaphore_at_once)
{ GlSharedObjects objects;
  CHECK(objects.remember(AV_SHARED_MEMORY, 4u));
  CHECK(objects.remember(AV_SHARED_NATIVE_FENCE, 4u));
  CHECK_EQ(objects.count(), 2u);
  std::uint32_t name = 0u;
  CHECK(objects.forget(AV_SHARED_NATIVE_FENCE, 4u, name));
  CHECK_EQ(name, 4u);
  CHECK(objects.forget(AV_SHARED_MEMORY, 4u, name));
  CHECK(objects.invariants_hold()); }

/* A WDDM monitored fence and a native fence are one host semaphore, so an
 * object imported as either is released by either: the guest's two kinds are
 * one thing here, and refusing the other would leak it. */
TEST(gl_shared_objects, either_fence_kind_releases_a_semaphore_the_other_imported)
{ GlSharedObjects objects;
  CHECK(objects.remember(AV_SHARED_MONITORED_FENCE, 7u));
  std::uint32_t name = 0u;
  CHECK(objects.forget(AV_SHARED_NATIVE_FENCE, 7u, name));
  CHECK_EQ(name, 7u);
  CHECK_EQ(objects.count(), 0u); }

TEST(gl_shared_objects, a_name_of_zero_or_one_already_held_is_not_remembered)
{ GlSharedObjects objects;
  CHECK(!objects.remember(AV_SHARED_MEMORY, 0u));
  CHECK(!objects.remember(9u, 3u));
  CHECK(objects.remember(AV_SHARED_MEMORY, 3u));
  CHECK(!objects.remember(AV_SHARED_MEMORY, 3u));
  CHECK(objects.remember(AV_SHARED_MONITORED_FENCE, 3u));
  CHECK(!objects.remember(AV_SHARED_NATIVE_FENCE, 3u));
  CHECK_EQ(objects.count(), 2u);
  CHECK(objects.invariants_hold()); }

/* A GL name is 32 bits. A wire handle is 64, and a guest writes it. */
TEST(gl_shared_objects, a_handle_no_gl_name_could_be_is_nobody_s)
{ GlSharedObjects objects;
  CHECK(objects.remember(AV_SHARED_MEMORY, 5u));
  std::uint32_t name = 0u;
  CHECK(!objects.forget(AV_SHARED_MEMORY, 0x1'0000'0005ull, name));
  CHECK(!objects.forget(AV_SHARED_MEMORY, ~std::uint64_t{0}, name));
  CHECK(!objects.forget(AV_SHARED_MEMORY, 0u, name));
  CHECK_EQ(objects.count(), 1u); }

TEST(gl_shared_objects, fills_up_and_then_remembers_nothing_more)
{ GlSharedObjects objects;
  for (std::uint32_t i = 1u; i <= GlSharedObjects::max_objects; i++)
      CHECK(objects.remember(AV_SHARED_MEMORY, i));
  CHECK_EQ(objects.count(), GlSharedObjects::max_objects);
  CHECK(!objects.remember(AV_SHARED_MEMORY, GlSharedObjects::max_objects + 1u));
  CHECK(objects.invariants_hold());
  std::uint32_t name = 0u;
  CHECK(objects.forget(AV_SHARED_MEMORY, 1u, name));
  CHECK(objects.remember(AV_SHARED_MEMORY, GlSharedObjects::max_objects + 1u));
  CHECK(objects.invariants_hold()); }

/* What a teardown deletes everything through: every row comes back once. */
TEST(gl_shared_objects, take_any_hands_back_every_row_and_then_nothing)
{ GlSharedObjects objects;
  CHECK(objects.remember(AV_SHARED_MEMORY, 1u));
  CHECK(objects.remember(AV_SHARED_MEMORY, 2u));
  CHECK(objects.remember(AV_SHARED_MONITORED_FENCE, 1u));
  std::uint32_t seen = 0u;
  std::uint32_t fences = 0u;
  std::uint32_t kind = 0u;
  std::uint32_t name = 0u;
  while (objects.take_any(kind, name)) {
      CHECK(objects.invariants_hold());
      seen += name;
      if (kind != AV_SHARED_MEMORY) fences++;
  }
  CHECK_EQ(seen, 4u);
  CHECK_EQ(fences, 1u);
  CHECK_EQ(objects.count(), 0u); }

TEST(gl_shared_objects_invariants, refuse_a_row_with_no_name_an_unknown_kind_or_a_pair_held_twice)
{ GlSharedObjects objects;
  CHECK(objects.remember(AV_SHARED_MEMORY, 1u));
  CHECK(objects.remember(AV_SHARED_MEMORY, 2u));
  CHECK(objects.invariants_hold());
  av::GlSharedObjectsProbe::set_name(objects, 1u, 0u);
  CHECK(!objects.invariants_hold());
  av::GlSharedObjectsProbe::set_name(objects, 1u, 1u);
  CHECK(!objects.invariants_hold());
  av::GlSharedObjectsProbe::set_name(objects, 1u, 3u);
  CHECK(objects.invariants_hold());
  av::GlSharedObjectsProbe::set_kind(objects, 1u, 0u);
  CHECK(!objects.invariants_hold());
  /* A row never holds the guest's monitored-fence kind: both fence kinds are
   * one host semaphore and a row records the name space, not what the guest
   * called it. */
  av::GlSharedObjectsProbe::set_kind(objects, 1u, AV_SHARED_MONITORED_FENCE);
  CHECK(!objects.invariants_hold());
  av::GlSharedObjectsProbe::set_kind(objects, 1u, AV_SHARED_NATIVE_FENCE + 1u);
  CHECK(!objects.invariants_hold());
  av::GlSharedObjectsProbe::set_kind(objects, 1u, AV_SHARED_MEMORY);
  CHECK(objects.invariants_hold());
  av::GlSharedObjectsProbe::set_count(objects, GlSharedObjects::max_objects + 1u);
  CHECK(!objects.invariants_hold()); }

/* The three things a forget refuses before it looks for a row: a kind with no
 * namespace, a name of zero, and a wire handle no GL name fits in. Each of
 * them would otherwise match whatever the table happens to hold at that
 * number, which is another context's object (feature 053). */
TEST(gl_shared_objects_forget, refuses_a_kind_a_name_and_a_handle_no_row_could_have)
{ GlSharedObjects objects;
  CHECK(objects.remember(AV_SHARED_MEMORY, 7u));
  std::uint32_t name = 0u;
  CHECK(!objects.forget(AV_SHARED_NATIVE_FENCE + 1u, 7u, name));
  CHECK(!objects.forget(AV_SHARED_MEMORY, 0u, name));
  CHECK(!objects.forget(AV_SHARED_MEMORY, 0x1'0000'0007ull, name));
  CHECK_EQ(name, 0u);
  /* And the row is still there, which it would not be had any of them found
   * it. */
  CHECK(objects.forget(AV_SHARED_MEMORY, 7u, name));
  CHECK_EQ(name, 7u); }
