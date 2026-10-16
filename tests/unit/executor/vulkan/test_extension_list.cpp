/* The extension names a guest's create info carried, with what the host needs
 * added. What is judged is the list the driver is handed: the guest's names in
 * its order, an added name once and only where there is room, and a list the
 * loader would refuse left for the loader to refuse. */
#include "core/support/status.hpp"
#include "executor/vulkan/extension_list.hpp"
#include "harness/executor/extension_list_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <cstring>

using av::ExtensionList;
using av::ExtensionListProbe;
using av::MockPlatform;
using av::Status;

namespace {

constexpr const char *surface = "VK_KHR_surface";
constexpr const char *host = "VK_EXT_external_memory_host";
constexpr const char *external = "VK_KHR_external_memory";

}  // namespace

TEST(extension_list, keeps_the_guest_s_names_in_order_and_adds_one_after_them)
{ MockPlatform  platform;
  ExtensionList list(platform);
  const char *const names[] = {surface, external};
  CHECK_STATUS(list.take(2u, names, 1u), Status::ok);
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
{ MockPlatform  platform;
  ExtensionList list(platform);
  char copy[64] = {};
  std::strcpy(copy, host);
  const char *const names[] = {copy};
  CHECK_STATUS(list.take(1u, names, 2u), Status::ok);
  list.add(host);
  list.add(host);
  CHECK_EQ(list.count(), 1u);
  CHECK(list.names()[0] == copy);
  CHECK(list.invariants_hold()); }

/* No names at all is a list, and what is added is the whole of it. */
TEST(extension_list, an_empty_list_takes_what_is_added)
{ MockPlatform  platform;
  ExtensionList list(platform);
  CHECK_STATUS(list.take(0u, nullptr, 2u), Status::ok);
  CHECK_EQ(list.count(), 0u);
  list.add(external);
  list.add(host);
  CHECK_EQ(list.count(), 2u);
  CHECK(std::strcmp(list.names()[0], external) == 0);
  CHECK(std::strcmp(list.names()[1], host) == 0);
  CHECK(list.invariants_hold()); }

/* However many names the guest enabled - well past the 256 a list once held -
 * the list holds them all, and adds the host's into the room it was told to
 * make: no more than that. */
TEST(extension_list, holds_every_name_and_adds_only_into_the_room_made)
{ static const char *many[1000];
  for (const char *&name : many) name = surface;
  MockPlatform  platform;
  ExtensionList list(platform);
  CHECK_STATUS(list.take(1000u, many, 1u), Status::ok);
  list.add(external);
  list.add(host);
  CHECK_EQ(list.count(), 1001u);
  CHECK(std::strcmp(list.names()[999], surface) == 0);
  CHECK(std::strcmp(list.names()[1000], external) == 0);
  CHECK(list.invariants_hold()); }

/* What the list cannot be: a count with no names - which the loader refuses on
 * its own - a name that is not there, or more than memory holds. Each leaves
 * the list empty rather than half-taken, so a later add cannot extend what a
 * previous create left behind. */
TEST(extension_list, a_list_it_cannot_take_is_refused_and_leaves_it_empty)
{ static const char *many[64];
  for (const char *&name : many) name = surface;
  const char *const holed[] = {surface, nullptr, host};
  MockPlatform  platform;
  ExtensionList list(platform);

  CHECK_STATUS(list.take(2u, many, 0u), Status::ok);
  CHECK_STATUS(list.take(3u, nullptr, 0u), Status::invalid_parameter);
  CHECK_EQ(list.count(), 0u);

  CHECK_STATUS(list.take(2u, many, 0u), Status::ok);
  CHECK_STATUS(list.take(3u, holed, 0u), Status::invalid_parameter);
  CHECK_EQ(list.count(), 0u);

  CHECK_STATUS(list.take(2u, many, 0u), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(list.take(64u, many, 0u), Status::no_memory);
  CHECK_EQ(list.count(), 0u);
  CHECK_STATUS(list.take(2u, many, 0xFFFFFFFFu), Status::no_memory);
  CHECK_EQ(list.count(), 0u);
  list.add(host);
  CHECK_EQ(list.count(), 1u);
  CHECK(list.invariants_hold()); }

/* A name slot the count covers that is empty is a list the driver would
 * dereference null in. */
TEST(extension_list_invariants, an_empty_name_inside_the_count_breaks_them)
{ MockPlatform  platform;
  ExtensionList list(platform);
  CHECK_STATUS(list.take(0u, nullptr, 2u), Status::ok);
  list.add(host);
  list.add(external);
  ExtensionListProbe(list).set_name(1u, nullptr);
  CHECK(!list.invariants_hold()); }
