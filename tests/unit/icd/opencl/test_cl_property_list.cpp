#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_property_list.hpp"

#include <cstdint>

using av::clicd::ClPropertyList;
using av::clicd::ClPropertyListProbe;

namespace av::clicd {
struct ClPropertyListProbe {
    static void count_past_the_table(ClPropertyList &list)
    { list.pairs_ = ClPropertyList::max_pairs + 1u; }
    static void a_pair_named_nothing(ClPropertyList &list) { list.names_[0] = 0u; }
};

}

TEST(cl_property_list, a_null_list_and_an_empty_one_are_both_no_properties)
{ ClPropertyList list;
  CHECK(list.read(static_cast<const std::uint64_t *>(nullptr)));
  CHECK_EQ(list.pairs(), 0u);
  CHECK(!list.repeated());
  const std::uint64_t empty[] = {0u};
  CHECK(list.read(empty));
  CHECK_EQ(list.pairs(), 0u);
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
  CHECK_EQ(list.name_at(1u), 0x1085u);
  CHECK_EQ(list.value_at(1u), 0u);
  CHECK_EQ(list.value_at(2u), 0xFFFFFFFFFFFFFFFFull);
  std::uint64_t value = 0u;
  CHECK(list.names(0x1085u, value));
  CHECK_EQ(value, 0u);
  CHECK(!list.names(0x9999u, value));
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
  std::uint64_t value = 0u;
  CHECK(list.names(0x1084u, value));
  CHECK_EQ(value, 5u);

  const std::uint64_t different[] = {0x1084u, 5u, 0x1085u, 6u, 0x1084u, 7u, 0u};
  CHECK(list.read(different));
  CHECK(list.repeated());
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

  std::uint64_t over[ClPropertyList::max_pairs * 2u + 3u] = {};
  for (std::uint32_t pair = 0u; pair <= ClPropertyList::max_pairs; ++pair) {
      over[pair * 2u] = 0x2000u + pair;
      over[pair * 2u + 1u] = pair;
  }
  CHECK(!list.read(over));
  CHECK_EQ(list.pairs(), 0u);
  CHECK(list.invariants_hold()); }

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

TEST(cl_property_list, a_list_lays_out_as_it_was_written_or_not_at_all)
{ ClPropertyList      list;
  const std::uint64_t written[] = {0x1084u, 0xABCDu, 0x1085u, 1u, 0u};
  CHECK(list.read(written));
  std::uint64_t out[5] = {9u, 9u, 9u, 9u, 9u};
  CHECK_EQ(list.lay_out(out, 5u), 5u);
  for (std::uint32_t at = 0u; at < 5u; ++at) CHECK_EQ(out[at], written[at]);
  std::uint64_t short_of[4] = {9u, 9u, 9u, 9u};
  CHECK_EQ(list.lay_out(short_of, 4u), 0u);
  CHECK_EQ(short_of[0], 9u);
  CHECK_EQ(list.lay_out(static_cast<std::uint64_t *>(nullptr), 5u), 0u);
  ClPropertyList none;
  CHECK(none.read(static_cast<const std::uint64_t *>(nullptr)));
  CHECK_EQ(none.lay_out(out, 5u), 0u); }

TEST(cl_property_list, a_narrow_list_is_read_and_laid_out_in_its_own_width)
{ ClPropertyList list;
  const std::uint32_t narrow[] = {0x1084u, 0x80000010u, 0x10B6u, 7u, 0u, 0xDEADu};
  CHECK(list.read(narrow));
  CHECK_EQ(list.pairs(), 2u);
  CHECK_EQ(list.name_at(0u), 0x1084u);
  CHECK_EQ(list.value_at(0u), 0x80000010u);
  CHECK_EQ(list.value_at(1u), 7u);
  std::uint32_t out[5] = {9u, 9u, 9u, 9u, 9u};
  CHECK_EQ(list.lay_out(out, 5u), 5u);
  CHECK_EQ(out[0], 0x1084u);
  CHECK_EQ(out[1], 0x80000010u);
  CHECK_EQ(out[2], 0x10B6u);
  CHECK_EQ(out[3], 7u);
  CHECK_EQ(out[4], 0u);
  CHECK(list.invariants_hold()); }

TEST(cl_property_list, a_device_handle_list_runs_to_its_terminator_and_the_pairs_go_on_after_it)
{ ClPropertyList list;
  const std::uint64_t one[] = {ClPropertyList::device_handle_list, 0xD1u, 0u, 0x2063u, 0xAA00u, 0u};
  CHECK(list.read(one));
  CHECK_EQ(list.pairs(), 2u);
  CHECK_EQ(list.name_at(0u), ClPropertyList::device_handle_list);
  CHECK_EQ(list.value_at(0u), 0xD1u);
  CHECK_EQ(list.name_at(1u), 0x2063u);
  CHECK_EQ(list.value_at(1u), 0xAA00u);

  const std::uint64_t two[] = {ClPropertyList::device_handle_list, 0xD1u, 0xD2u, 0u, 0x2063u, 0xBB00u, 0u};
  CHECK(list.read(two));
  CHECK_EQ(list.pairs(), 2u);
  CHECK_EQ(list.value_at(1u), 0xBB00u);

  const std::uint64_t none[] = {ClPropertyList::device_handle_list, 0u, 0x2063u, 0xCC00u, 0u};
  CHECK(list.read(none));
  CHECK_EQ(list.pairs(), 2u);
  CHECK_EQ(list.value_at(1u), 0xCC00u);

  std::uint64_t endless[ClPropertyList::most_list_values + 8u] = {ClPropertyList::device_handle_list};
  for (std::uint32_t index = 1u; index < ClPropertyList::most_list_values + 8u; index++) endless[index] = 0xD0u;
  CHECK(!list.read(endless));
  CHECK_EQ(list.pairs(), 0u);
  CHECK(list.invariants_hold()); }
