/* The one property this wire adds to clCreateBufferWithProperties: where in
 * the context's blob arena a buffer's memory is.
 *
 * Both sides use this class, so the encoding exists once. The reading half is
 * the interesting one - it runs on the device, over a list the guest wrote,
 * so a list with no terminator, a list naming the property twice and a list
 * naming nothing it knows all have to end somewhere sensible. */
#include "harness/framework/test_registry.hpp"
#include "proto/cl_blob_backing.hpp"

#include <cstdint>

using av::ClBlobBacking;
using av::ClBlobBackingProbe;

namespace av {

struct ClBlobBackingProbe {
    /* The state the class never builds: an offset remembered while nothing
     * is held, which is a caller about to be told to look at the start of
     * the arena for a buffer that was never backed there. */
    static void orphan_offset(ClBlobBacking &backing) { backing.offset_ = 0x4000u; }
};

}  // namespace av

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
  /* Terminated, because the decoder's walk ends on the zero key. */
  CHECK_EQ(list[2], 0u);

  ClBlobBacking read;
  CHECK(read.read(list));
  CHECK_EQ(read.offset(), 0x4000u);
  CHECK(read.invariants_hold()); }

TEST(cl_blob_backing, an_offset_of_zero_is_a_backing_like_any_other)
{ ClBlobBacking written;
  /* The first buffer out of the arena sits at offset zero, so zero must be
   * held rather than read as nothing held. */
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
  /* No list at all: every buffer the application makes for itself. */
  CHECK(!read.read(nullptr));
  CHECK(!read.held());
  CHECK(read.invariants_hold());

  /* An empty list, and one naming another property. */
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
  /* Every word a key, none of them zero: the guest wrote it, so the walk is
   * bounded by the only length this list can have rather than by trust. */
  const std::uint64_t unterminated[4] = {1u, 2u, 3u, 4u};
  CHECK(!read.read(unterminated));
  CHECK(!read.held());
  CHECK(read.invariants_hold());

  /* The same, but the property is in it: still found, and still bounded. */
  const std::uint64_t named[4] = {ClBlobBacking::property, 0x2000u, 3u, 4u};
  CHECK(read.read(named));
  CHECK_EQ(read.offset(), 0x2000u);
  CHECK(read.invariants_hold()); }

TEST(cl_blob_backing, a_list_naming_the_backing_twice_is_refused_rather_than_chosen_between)
{ ClBlobBacking read;
  const std::uint64_t twice[5] = {ClBlobBacking::property, 0x1000u,
                                  ClBlobBacking::property, 0x8000u, 0u};
  /* Two answers and no way to prefer one. The device must not guess which
   * memory the guest meant. */
  CHECK(!read.read(twice));
  CHECK(!read.held());
  CHECK_EQ(read.offset(), 0u);
  CHECK(read.invariants_hold()); }

TEST(cl_blob_backing, a_backing_that_holds_nothing_writes_nothing)
{ ClBlobBacking nothing;
  CHECK(!nothing.held());
  std::uint64_t list[4] = {0xEEu, 0xEEu, 0xEEu, 0xEEu};
  CHECK(!nothing.write(list));
  /* Untouched, so a caller that ignored the answer sends its own list and
   * not a half-written one. */
  CHECK_EQ(list[0], 0xEEu);
  ClBlobBacking held;
  held.set(1u);
  CHECK(!held.write(nullptr));
  CHECK(held.invariants_hold()); }

/* The invariant catches the state the class never builds. */
TEST(cl_blob_backing, the_invariant_catches_an_offset_held_by_nothing)
{ ClBlobBacking backing;
  CHECK(backing.invariants_hold());
  ClBlobBackingProbe::orphan_offset(backing);
  CHECK(!backing.invariants_hold()); }
