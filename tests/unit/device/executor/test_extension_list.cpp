/* The extension names a guest's create info carried, with what the host needs
 * added. What is judged is the list the driver is handed: the guest's names in
 * its order, an added name once and only where there is room, and a list the
 * loader would refuse left for the loader to refuse. */
#include "core/support/status.hpp"
#include "device/executor/extension_list.hpp"
#include "harness/executor/extension_list_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::ExtensionList;
using av::ExtensionListProbe;
using av::Status;

namespace {

constexpr const char *surface = "VK_KHR_surface";
constexpr const char *host = "VK_EXT_external_memory_host";
constexpr const char *external = "VK_KHR_external_memory";

}  // namespace

TEST(extension_list, keeps_the_guest_s_names_in_order_and_adds_one_after_them)
{ ExtensionList list;
  const char *const names[] = {surface, external};
  CHECK_STATUS(list.take(2u, names), Status::ok);
  CHECK(list.invariants_hold());
  list.add(host);
  CHECK(list.invariants_hold());
  CHECK_EQ(list.count(), 3u);
  CHECK(std::strcmp(list.names()[0], surface) == 0);
  CHECK(std::strcmp(list.names()[1], external) == 0);
  CHECK(std::strcmp(list.names()[2], host) == 0); }

/* A name the guest already enabled is not enabled twice - compared by its
 * characters, because the guest's copy is a different pointer. */
TEST(extension_list, a_name_already_there_is_not_added_again)
{ ExtensionList list;
  char copy[64] = {};
  std::strcpy(copy, host);
  const char *const names[] = {copy};
  CHECK_STATUS(list.take(1u, names), Status::ok);
  list.add(host);
  list.add(host);
  CHECK_EQ(list.count(), 1u);
  CHECK(list.names()[0] == copy);
  CHECK(list.invariants_hold()); }

/* No names at all is a list, and what is added is the whole of it. */
TEST(extension_list, an_empty_list_takes_what_is_added)
{ ExtensionList list;
  CHECK_STATUS(list.take(0u, nullptr), Status::ok);
  CHECK_EQ(list.count(), 0u);
  list.add(external);
  list.add(host);
  CHECK_EQ(list.count(), 2u);
  CHECK(std::strcmp(list.names()[0], external) == 0);
  CHECK(std::strcmp(list.names()[1], host) == 0);
  CHECK(list.invariants_hold()); }

/* A full list is handed over as the guest sent it, and one a name short takes
 * the first name added and no other. */
TEST(extension_list, adds_only_what_there_is_room_for)
{ static const char *many[ExtensionList::capacity];
  for (const char *&name : many) name = surface;
  ExtensionList full;
  CHECK_STATUS(full.take(ExtensionList::capacity, many), Status::ok);
  full.add(host);
  CHECK_EQ(full.count(), ExtensionList::capacity);
  CHECK(full.invariants_hold());

  ExtensionList short_one;
  CHECK_STATUS(short_one.take(ExtensionList::capacity - 1u, many), Status::ok);
  short_one.add(external);
  short_one.add(host);
  CHECK_EQ(short_one.count(), ExtensionList::capacity);
  CHECK(std::strcmp(short_one.names()[ExtensionList::capacity - 1u], external) == 0);
  CHECK(short_one.invariants_hold()); }

/* What the list cannot be: more names than it holds, a count with no names -
 * which the loader refuses on its own - or a name that is not there. Each
 * leaves the list empty rather than half-taken, so a later add cannot extend
 * what a previous create left behind. */
TEST(extension_list, a_list_it_cannot_hold_is_refused_and_leaves_it_empty)
{ static const char *many[ExtensionList::capacity + 1u];
  for (const char *&name : many) name = surface;
  const char *const holed[] = {surface, nullptr, host};
  ExtensionList list;

  CHECK_STATUS(list.take(2u, many), Status::ok);
  CHECK_STATUS(list.take(ExtensionList::capacity + 1u, many), Status::no_space);
  CHECK_EQ(list.count(), 0u);
  CHECK(list.invariants_hold());

  CHECK_STATUS(list.take(2u, many), Status::ok);
  CHECK_STATUS(list.take(3u, nullptr), Status::invalid_parameter);
  CHECK_EQ(list.count(), 0u);

  CHECK_STATUS(list.take(2u, many), Status::ok);
  CHECK_STATUS(list.take(3u, holed), Status::invalid_parameter);
  CHECK_EQ(list.count(), 0u);
  CHECK(list.invariants_hold()); }

/* The count past the room, or a name slot the count covers that is empty, is a
 * list the driver would read past or dereference null in. */
TEST(extension_list_invariants, a_count_past_the_room_breaks_them)
{ ExtensionList list;
  ExtensionListProbe(list).set_count(ExtensionList::capacity + 1u);
  CHECK(!list.invariants_hold()); }

TEST(extension_list_invariants, an_empty_name_inside_the_count_breaks_them)
{ ExtensionList list;
  CHECK_STATUS(list.take(0u, nullptr), Status::ok);
  list.add(host);
  ExtensionListProbe(list).set_count(2u);
  CHECK(!list.invariants_hold()); }
