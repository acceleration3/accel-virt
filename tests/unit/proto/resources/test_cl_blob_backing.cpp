#include "harness/framework/test_registry.hpp"
#include "proto/resources/cl_blob_backing.hpp"

#include <cstdint>

using av::ClBlobBacking;
using av::ClBlobBackingProbe;

namespace av {

struct ClBlobBackingProbe {
    static void orphan_offset(ClBlobBacking &backing) { backing.offset_ = 0x4000u; }
};

}

TEST(cl_blob_backing, what_one_side_writes_the_other_reads)
{ ClBlobBacking written;
  written.set(0x4000u);
  CHECK(written.held());
  CHECK_EQ(written.offset(), 0x4000u);
  CHECK(written.invariants_hold());

  std::uint64_t list[4] = {};
  CHECK(written.write(list));
  CHECK_EQ(list[0], ClBlobBacking::property);
  CHECK_EQ(list[1], 0x4000u);
  CHECK_EQ(list[2], 0u);

  ClBlobBacking read;
  CHECK(read.read(list));
  CHECK_EQ(read.offset(), 0x4000u);
  CHECK(read.invariants_hold()); }

TEST(cl_blob_backing, an_offset_of_zero_is_a_backing_like_any_other)
{ ClBlobBacking written;
  written.set(0u);
  CHECK(written.held());
  std::uint64_t list[4] = {};
  CHECK(written.write(list));
  ClBlobBacking read;
  CHECK(read.read(list));
  CHECK(read.held());
  CHECK_EQ(read.offset(), 0u);
  CHECK(read.invariants_hold()); }

TEST(cl_blob_backing, a_list_that_names_nothing_this_knows_is_not_a_backing)
{ ClBlobBacking read;
  CHECK(!read.read(nullptr));
  CHECK(!read.held());
  CHECK(read.invariants_hold());

  const std::uint64_t empty[1] = {0u};
  CHECK(!read.read(empty));
  CHECK(!read.held());
  const std::uint64_t other[3] = {0x1101u, 7u, 0u};
  CHECK(!read.read(other));
  CHECK(!read.held());
  CHECK_EQ(read.offset(), 0u);
  CHECK(read.invariants_hold()); }

TEST(cl_blob_backing, a_list_with_no_terminator_ends_the_walk_rather_than_running_off)
{ ClBlobBacking read;
  const std::uint64_t unterminated[6] = {1u, 2u, 3u, 4u, 5u, 6u};
  CHECK(!read.read(unterminated));
  CHECK(!read.held());
  CHECK(read.invariants_hold());

  const std::uint64_t named[6] = {ClBlobBacking::property, 0x2000u, 3u, 4u, 5u, 6u};
  CHECK(read.read(named));
  CHECK_EQ(read.offset(), 0x2000u);
  CHECK(read.invariants_hold()); }

TEST(cl_blob_backing, a_list_naming_the_backing_twice_is_refused_rather_than_chosen_between)
{ ClBlobBacking read;
  const std::uint64_t twice[5] = {ClBlobBacking::property, 0x1000u,
                                  ClBlobBacking::property, 0x8000u, 0u};
  CHECK(!read.read(twice));
  CHECK(!read.held());
  CHECK_EQ(read.offset(), 0u);
  CHECK(read.invariants_hold()); }

TEST(cl_blob_backing, a_backing_that_holds_nothing_writes_nothing)
{ ClBlobBacking nothing;
  CHECK(!nothing.held());
  std::uint64_t list[4] = {0xEEu, 0xEEu, 0xEEu, 0xEEu};
  CHECK(!nothing.write(list));
  CHECK_EQ(list[0], 0xEEu);
  ClBlobBacking held;
  held.set(1u);
  CHECK(!held.write(nullptr));
  CHECK(held.invariants_hold()); }

TEST(cl_blob_backing, the_invariant_catches_an_offset_held_by_nothing)
{ ClBlobBacking backing;
  CHECK(backing.invariants_hold());
  ClBlobBackingProbe::orphan_offset(backing);
  CHECK(!backing.invariants_hold()); }

TEST(cl_blob_backing_set_in, names_the_memory_the_backing_is_inside_and_writes_it_into_the_list)
{ ClBlobBacking written;
  written.set_in(0x7700u, 0x4000u);
  CHECK(written.held());
  CHECK_EQ(written.memory(), 0x7700u);
  CHECK_EQ(written.offset(), 0x4000u);
  CHECK(written.invariants_hold());
  std::uint64_t list[ClBlobBacking::list_words] = {};
  CHECK(written.write(list));
  CHECK_EQ(list[0], ClBlobBacking::property);
  CHECK_EQ(list[1], 0x4000u);
  CHECK_EQ(list[2], ClBlobBacking::import_property);
  CHECK_EQ(list[3], 0x7700u);
  CHECK_EQ(list[4], 0u);
  ClBlobBacking read;
  CHECK(read.read(list));
  CHECK_EQ(read.memory(), 0x7700u);
  CHECK_EQ(read.offset(), 0x4000u);
  CHECK(read.invariants_hold()); }
