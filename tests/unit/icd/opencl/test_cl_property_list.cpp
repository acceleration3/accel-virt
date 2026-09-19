/* The one place the ICD reads an array whose length the application chose.
 *
 * A property list is pairs of a name and a value ending at a zero name, and
 * nothing bounds it but that terminator - so the bound is here, and a list
 * without one inside it is refused rather than walked off the end. The rest is
 * what the API requires a driver to notice: a property it does not implement,
 * and a property named twice. */
#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_property_list.hpp"

#include <cstdint>

using av::clicd::ClPropertyList;
using av::clicd::ClPropertyListProbe;

namespace av::clicd {

/* The states a read never leaves behind. A count past the table is a walk
 * that would read off the end of it; a pair whose name is zero is the
 * terminator stored as if it were a property, which would make the list end
 * where a reader thinks a pair begins. */
struct ClPropertyListProbe {
    static void count_past_the_table(ClPropertyList &list)
    { list.pairs_ = ClPropertyList::max_pairs + 1u; }
    static void a_pair_named_nothing(ClPropertyList &list) { list.names_[0] = 0u; }
};

}  // namespace av::clicd

TEST(cl_property_list, a_null_list_and_an_empty_one_are_both_no_properties)
{ ClPropertyList list;
  CHECK(list.read(nullptr));
  CHECK_EQ(list.pairs(), 0u);
  CHECK(!list.repeated());
  const std::uint64_t empty[] = {0u};
  CHECK(list.read(empty));
  CHECK_EQ(list.pairs(), 0u);
  /* A name of zero is the terminator, so nothing names it. */
  std::uint64_t value = 7u;
  CHECK(!list.names(0u, value));
  CHECK_EQ(value, 7u);
  CHECK(list.invariants_hold()); }

TEST(cl_property_list, every_pair_is_kept_in_the_order_it_was_written)
{ ClPropertyList list;
  const std::uint64_t written[] = {0x1084u, 0x11u, 0x1085u, 0u, 0x1086u, 0xFFFFFFFFFFFFFFFFull, 0u};
  CHECK(list.read(written));
  CHECK_EQ(list.pairs(), 3u);
  CHECK_EQ(list.name_at(0u), 0x1084u);
  CHECK_EQ(list.value_at(0u), 0x11u);
  /* A value of zero is a value, not a terminator: only the name ends a list. */
  CHECK_EQ(list.name_at(1u), 0x1085u);
  CHECK_EQ(list.value_at(1u), 0u);
  CHECK_EQ(list.value_at(2u), 0xFFFFFFFFFFFFFFFFull);
  std::uint64_t value = 0u;
  CHECK(list.names(0x1085u, value));
  CHECK_EQ(value, 0u);
  CHECK(!list.names(0x9999u, value));
  /* Past the end answers nothing rather than reading the array. */
  CHECK_EQ(list.name_at(3u), 0u);
  CHECK_EQ(list.value_at(3u), 0u);
  CHECK_EQ(list.name_at(0xFFFFFFFFu), 0u);
  CHECK(!list.repeated());
  CHECK(list.invariants_hold()); }

TEST(cl_property_list, a_name_that_appears_twice_is_noticed_whatever_its_values)
{ ClPropertyList list;
  const std::uint64_t same_value[] = {0x1084u, 5u, 0x1084u, 5u, 0u};
  CHECK(list.read(same_value));
  CHECK_EQ(list.pairs(), 2u);
  CHECK(list.repeated());
  /* names() answers the first, and the caller is what refuses the list. */
  std::uint64_t value = 0u;
  CHECK(list.names(0x1084u, value));
  CHECK_EQ(value, 5u);

  const std::uint64_t different[] = {0x1084u, 5u, 0x1085u, 6u, 0x1084u, 7u, 0u};
  CHECK(list.read(different));
  CHECK(list.repeated());
  /* And a read replaces the last one, repetition included. */
  const std::uint64_t clean[] = {0x1084u, 5u, 0u};
  CHECK(list.read(clean));
  CHECK_EQ(list.pairs(), 1u);
  CHECK(!list.repeated());
  CHECK(list.invariants_hold()); }

TEST(cl_property_list, a_list_that_fills_the_bound_exactly_is_read_and_one_longer_is_refused)
{ ClPropertyList list;
  std::uint64_t exact[ClPropertyList::max_pairs * 2u + 1u] = {};
  for (std::uint32_t pair = 0u; pair < ClPropertyList::max_pairs; ++pair) {
      exact[pair * 2u] = 0x2000u + pair;
      exact[pair * 2u + 1u] = pair;
  }
  CHECK(list.read(exact));
  CHECK_EQ(list.pairs(), ClPropertyList::max_pairs);
  CHECK_EQ(list.name_at(ClPropertyList::max_pairs - 1u), 0x2000u + ClPropertyList::max_pairs - 1u);
  CHECK(list.invariants_hold());

  /* One pair more has no terminator this can reach. Nothing of it is kept:
   * a half-read list is not a list, and a caller that took the pairs anyway
   * would be acting on a list it never saw the end of. */
  std::uint64_t over[ClPropertyList::max_pairs * 2u + 3u] = {};
  for (std::uint32_t pair = 0u; pair <= ClPropertyList::max_pairs; ++pair) {
      over[pair * 2u] = 0x2000u + pair;
      over[pair * 2u + 1u] = pair;
  }
  CHECK(!list.read(over));
  CHECK_EQ(list.pairs(), 0u);
  CHECK(list.invariants_hold()); }

/* The invariant catches what a read never leaves behind. */
TEST(cl_property_list, the_invariant_catches_a_list_a_read_would_never_leave)
{ const std::uint64_t one[] = {0x1084u, 7u, 0u};
  { ClPropertyList list;
    CHECK(list.read(one));
    CHECK(list.invariants_hold());
    ClPropertyListProbe::count_past_the_table(list);
    CHECK(!list.invariants_hold()); }
  { ClPropertyList list;
    CHECK(list.read(one));
    ClPropertyListProbe::a_pair_named_nothing(list);
    CHECK(!list.invariants_hold()); } }
