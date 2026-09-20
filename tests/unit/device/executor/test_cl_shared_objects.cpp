/* What one OpenCL context holds for each shared object it opened
 * (feature 053, T019).
 *
 * Three host objects stand behind one guest handle here: the cl_mem the guest
 * names, the GL buffer OpenCL made it from, and the GL memory object the
 * descriptor became. They are made in that order and destroyed in the reverse
 * one, and the row is what remembers which three go together - a release that
 * freed the cl_mem and left the other two would leak the allocation on the
 * host for as long as the device lives, and one that deleted the memory object
 * first would pull the storage out from under a live cl_mem.
 */
#include "device/abi/rvk_executor.h"
#include "device/executor/cl_shared_objects.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace av {

struct ClSharedObjectsProbe {
    static void set_count(ClSharedObjects &objects, std::uint32_t count) { objects.count_ = count; }
    static std::uint64_t &mem(ClSharedObjects &objects, std::uint32_t index) { return objects.mems_[index]; }
    static std::uint32_t &buffer(ClSharedObjects &objects, std::uint32_t index)
    { return objects.buffers_[index]; }
    static std::uint32_t &memory(ClSharedObjects &objects, std::uint32_t index)
    { return objects.memories_[index]; }
};

}  // namespace av

namespace {

using av::ClSharedObjects;
using av::ClSharedObjectsProbe;

constexpr std::uint64_t first_mem = 0x9100u;

}  // namespace

TEST(cl_shared_objects, a_new_table_holds_nothing_and_finds_nothing)
{ ClSharedObjects objects;
  CHECK_EQ(objects.count(), 0u);
  std::uint32_t buffer = 9u;
  std::uint32_t memory = 9u;
  CHECK(!objects.forget(first_mem, buffer, memory));
  CHECK_EQ(buffer, 9u);
  CHECK_EQ(memory, 9u);
  std::uint64_t mem = 9u;
  CHECK(!objects.take_any(mem, buffer, memory));
  CHECK(objects.invariants_hold()); }

TEST(cl_shared_objects, a_row_gives_back_the_three_objects_that_were_made_together)
{ ClSharedObjects objects;
  CHECK(objects.remember(first_mem, 11u, 21u));
  CHECK(objects.remember(first_mem + 1u, 12u, 22u));
  CHECK_EQ(objects.count(), 2u);
  CHECK(objects.invariants_hold());

  std::uint32_t buffer = 0u;
  std::uint32_t memory = 0u;
  CHECK(objects.forget(first_mem, buffer, memory));
  CHECK_EQ(buffer, 11u);
  CHECK_EQ(memory, 21u);
  CHECK_EQ(objects.count(), 1u);
  /* Forgotten once: a second release names nothing, which is what stops a
   * guest deleting one object twice. */
  CHECK(!objects.forget(first_mem, buffer, memory));
  /* And the other row is untouched by the first one going. */
  CHECK(objects.forget(first_mem + 1u, buffer, memory));
  CHECK_EQ(buffer, 12u);
  CHECK_EQ(memory, 22u);
  CHECK_EQ(objects.count(), 0u);
  CHECK(objects.invariants_hold()); }

/* Every way a row is refused, each of which would leave two objects behind or
 * destroy one twice. */
TEST(cl_shared_objects, a_row_that_could_not_be_given_back_is_never_made)
{ ClSharedObjects objects;
  CHECK(!objects.remember(0u, 11u, 21u));          /* no cl_mem to name it by */
  CHECK(!objects.remember(first_mem, 0u, 21u));    /* no buffer to delete */
  CHECK(!objects.remember(first_mem, 11u, 0u));    /* no memory object */
  CHECK_EQ(objects.count(), 0u);
  CHECK(objects.remember(first_mem, 11u, 21u));
  /* One cl_mem twice: the host answered one number for two objects, which
   * nothing here could unpick. */
  CHECK(!objects.remember(first_mem, 12u, 22u));
  CHECK_EQ(objects.count(), 1u);
  CHECK(objects.invariants_hold()); }

TEST(cl_shared_objects, it_holds_as_many_as_its_table_and_no_more)
{ ClSharedObjects objects;
  for (std::uint32_t index = 0u; index < ClSharedObjects::max_objects; index++)
      CHECK(objects.remember(first_mem + index, index + 1u, index + 1001u));
  CHECK_EQ(objects.count(), ClSharedObjects::max_objects);
  CHECK(!objects.remember(first_mem + ClSharedObjects::max_objects, 1u, 1u));
  CHECK(objects.invariants_hold());

  /* A teardown takes them all, one at a time, and the table ends empty. */
  std::uint32_t taken = 0u;
  std::uint64_t mem = 0u;
  std::uint32_t buffer = 0u;
  std::uint32_t memory = 0u;
  while (objects.take_any(mem, buffer, memory)) {
      CHECK(mem != 0u);
      CHECK(buffer != 0u);
      CHECK(memory != 0u);
      taken++;
      CHECK(objects.invariants_hold());
  }
  CHECK_EQ(taken, ClSharedObjects::max_objects);
  CHECK_EQ(objects.count(), 0u); }

TEST(cl_shared_objects, invariants_reject_rows_no_import_leaves_behind)
{ ClSharedObjects objects;
  CHECK(objects.invariants_hold());
  ClSharedObjectsProbe::set_count(objects, 1u);
  CHECK(!objects.invariants_hold());                       /* a row of nothing */
  ClSharedObjectsProbe::mem(objects, 0u) = first_mem;
  CHECK(!objects.invariants_hold());                       /* no buffer */
  ClSharedObjectsProbe::buffer(objects, 0u) = 11u;
  CHECK(!objects.invariants_hold());                       /* no memory object */
  ClSharedObjectsProbe::memory(objects, 0u) = 21u;
  CHECK(objects.invariants_hold());
  /* One cl_mem in two rows is a table no release could unpick. */
  ClSharedObjectsProbe::set_count(objects, 2u);
  ClSharedObjectsProbe::mem(objects, 1u) = first_mem;
  ClSharedObjectsProbe::buffer(objects, 1u) = 12u;
  ClSharedObjectsProbe::memory(objects, 1u) = 22u;
  CHECK(!objects.invariants_hold());
  ClSharedObjectsProbe::mem(objects, 1u) = first_mem + 1u;
  CHECK(objects.invariants_hold());
  ClSharedObjectsProbe::set_count(objects, ClSharedObjects::max_objects + 1u);
  CHECK(!objects.invariants_hold()); }
