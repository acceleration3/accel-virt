#include "core/support/extension_names.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct ExtensionNamesProbe {
    static void append(ExtensionNames &names, const char *name) { static_cast<void>(names.names_.append(name)); }
};

}

using av::ExtensionNames;
using av::MockPlatform;
using av::Pool;
using av::Status;

TEST(extension_names, takes_a_list_and_adds_each_name_once_by_its_text)
{ MockPlatform platform;
  ExtensionNames names(platform, Pool::paged);
  CHECK(names.invariants_hold());
  CHECK_EQ(names.count(), 0u);
  CHECK(names.names() == nullptr);
  const char first[] = "VK_KHR_swapchain";
  const char again[] = "VK_KHR_swapchain";
  const char *const given[] = {first, "VK_KHR_maintenance1", again};
  CHECK_STATUS(names.take(given, 3u, 2u), Status::ok);
  CHECK_EQ(names.count(), 2u);
  CHECK(names.holds("VK_KHR_swapchain"));
  CHECK(names.holds("VK_KHR_maintenance1"));
  CHECK(!names.holds("VK_KHR_other"));
  CHECK(!names.holds(nullptr));
  CHECK_EQ(std::strcmp(names.at(1u), "VK_KHR_maintenance1"), 0);
  CHECK(names.at(2u) == nullptr);
  CHECK(names.names() != nullptr);
  CHECK(names.invariants_hold());
  CHECK_STATUS(names.add("VK_KHR_swapchain"), Status::ok);
  CHECK_EQ(names.count(), 2u);
  CHECK_STATUS(names.add("VK_KHR_other"), Status::ok);
  CHECK_EQ(names.count(), 3u);
  CHECK(names.invariants_hold());
  names.clear();
  CHECK_EQ(names.count(), 0u);
  CHECK(names.names() == nullptr); }

TEST(extension_names, refuses_a_nameless_entry_and_empties_what_it_held)
{ MockPlatform platform;
  ExtensionNames names(platform, Pool::paged);
  CHECK_STATUS(names.add("VK_KHR_swapchain"), Status::ok);
  const char *const empty[] = {"VK_KHR_a", ""};
  CHECK_STATUS(names.take(empty, 2u, 0u), Status::invalid_parameter);
  CHECK_EQ(names.count(), 0u);
  const char *const missing[] = {"VK_KHR_a", nullptr};
  CHECK_STATUS(names.take(missing, 2u, 0u), Status::invalid_parameter);
  CHECK_STATUS(names.take(nullptr, 1u, 0u), Status::invalid_parameter);
  CHECK_STATUS(names.take(nullptr, 0u, 0u), Status::ok);
  CHECK_STATUS(names.add(nullptr), Status::invalid_parameter);
  CHECK_STATUS(names.add(""), Status::invalid_parameter);
  CHECK_EQ(names.count(), 0u);
  CHECK(names.invariants_hold()); }

TEST(extension_names, reserves_room_and_refuses_a_count_no_array_can_hold)
{ MockPlatform platform;
  ExtensionNames names(platform, Pool::paged);
  CHECK_STATUS(names.reserve(8u), Status::ok);
  CHECK_EQ(names.count(), 0u);
  const char *const given[] = {"VK_KHR_a"};
  CHECK_STATUS(names.take(given, 1u, 0xFFFFFFFFu), Status::no_memory);
  CHECK(names.invariants_hold()); }

TEST(extension_names, a_repeated_or_nameless_row_breaks_the_invariant)
{ MockPlatform platform;
  ExtensionNames repeated(platform, Pool::paged);
  av::ExtensionNamesProbe::append(repeated, "VK_KHR_a");
  av::ExtensionNamesProbe::append(repeated, "VK_KHR_a");
  CHECK(!repeated.invariants_hold());
  ExtensionNames nameless(platform, Pool::paged);
  av::ExtensionNamesProbe::append(nameless, "");
  CHECK(!nameless.invariants_hold()); }
