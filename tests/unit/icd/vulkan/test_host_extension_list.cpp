#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/host_extension_list.hpp"

#include <cstdint>
#include <cstring>

using av::Status;
using av::icd::HostExtensionList;

namespace av {
struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_size(GrowableArray<T> &array, std::uint32_t size) { array.size_ = size; }
};

struct ExtensionNamesProbe {
    static void corrupt_size(ExtensionNames &names, std::uint32_t size) { GrowableArrayProbe::corrupt_size(names.names_, size); }
};

}

namespace av::icd {
struct HostExtensionListProbe {
    static void corrupt_names(HostExtensionList &list, std::uint32_t size) { ExtensionNamesProbe::corrupt_size(list.names_, size); }
    static void set_kept(HostExtensionList &list, std::uint32_t kept) { list.kept_ = kept; }
};

}

using av::icd::HostExtensionListProbe;

TEST(host_extension_list, the_window_system_extensions_are_not_passed_on)
{ av::MockPlatform memory;
  HostExtensionList asked(memory);
  CHECK(asked.invariants_hold());
  const char *const enabled[] = {"VK_KHR_surface", "VK_KHR_win32_surface", "VK_KHR_get_physical_device_properties2",
                                 "VK_KHR_swapchain", "VK_EXT_memory_budget"};
  CHECK_STATUS(asked.take(enabled, 5u), Status::ok);
  CHECK(asked.invariants_hold());
  CHECK_EQ(asked.count(), 2u);
  CHECK_EQ(asked.kept(), 3u);
  CHECK(std::strcmp(asked.names()[0], "VK_KHR_get_physical_device_properties2") == 0);
  CHECK(std::strcmp(asked.names()[1], "VK_EXT_memory_budget") == 0);
  const char *const nearly[] = {"VK_KHR_surface_protected_capabilities", "VK_KHR_surfac"};
  CHECK_STATUS(asked.take(nearly, 2u), Status::ok);
  CHECK_EQ(asked.count(), 2u);
  CHECK_EQ(asked.kept(), 0u);
  CHECK(asked.invariants_hold()); }

TEST(host_extension_list, a_list_of_only_this_icd_s_own_asks_the_host_for_nothing)
{ av::MockPlatform memory;
  HostExtensionList asked(memory);
  const char *const enabled[] = {"VK_KHR_swapchain", "VK_KHR_surface"};
  CHECK_STATUS(asked.take(enabled, 2u), Status::ok);
  CHECK_EQ(asked.count(), 0u);
  CHECK_EQ(asked.kept(), 2u);
  CHECK(asked.names() == nullptr);
  CHECK(asked.invariants_hold());
  CHECK_STATUS(asked.take(nullptr, 0u), Status::ok);
  CHECK_EQ(asked.count(), 0u);
  CHECK_EQ(asked.kept(), 0u);
  CHECK(asked.names() == nullptr); }

namespace {

void spell(char (*spellings)[16], const char **names, std::uint32_t count)
{
    for (std::uint32_t index = 0u; index < count; index++) {
        spellings[index][0] = 'V';
        spellings[index][1] = static_cast<char>('a' + index % 26u);
        spellings[index][2] = static_cast<char>('a' + index / 26u % 26u);
        spellings[index][3] = static_cast<char>('a' + index / 676u);
        names[index] = spellings[index];
    }
}

constexpr std::uint32_t most = 1000u;

}

TEST(host_extension_list, a_list_of_any_length_is_held_and_a_list_it_cannot_read_is_refused)
{ av::MockPlatform memory;
  HostExtensionList asked(memory);
  const char *const one[] = {"VK_EXT_memory_budget"};
  CHECK_STATUS(asked.take(one, 1u), Status::ok);
  CHECK_EQ(asked.count(), 1u);
  static const char *many[most] = {};
  static char        spellings[most][16] = {};
  spell(spellings, many, most);
  CHECK_STATUS(asked.take(many, most), Status::ok);
  CHECK_EQ(asked.count(), most);
  CHECK(asked.invariants_hold());
  CHECK(std::strcmp(asked.names()[most - 1u], many[most - 1u]) == 0);
  const char *const repeated[] = {"VK_EXT_memory_budget", "VK_EXT_memory_budget"};
  CHECK_STATUS(asked.take(repeated, 2u), Status::ok);
  CHECK_EQ(asked.count(), 1u);
  CHECK(asked.invariants_hold());
  CHECK_STATUS(asked.take(nullptr, 1u), Status::invalid_parameter);
  CHECK_EQ(asked.count(), 0u);
  const char *const unnamed[] = {"VK_EXT_memory_budget", nullptr};
  CHECK_STATUS(asked.take(unnamed, 2u), Status::invalid_parameter);
  CHECK_EQ(asked.count(), 0u);
  CHECK(asked.invariants_hold()); }

TEST(host_extension_list, a_list_it_cannot_make_room_for_leaves_nothing_held)
{ av::MockPlatform memory;
  HostExtensionList asked(memory);
  const char *const one[] = {"VK_EXT_memory_budget"};
  CHECK_STATUS(asked.take(one, 1u), Status::ok);
  memory.fail_allocation_at(1u);
  static const char *many[most] = {};
  static char        spellings[most][16] = {};
  spell(spellings, many, most);
  CHECK_STATUS(asked.take(many, most), Status::no_memory);
  CHECK_EQ(asked.count(), 0u);
  CHECK_EQ(asked.kept(), 0u);
  CHECK(asked.invariants_hold());
  memory.fail_allocation_at(0u);
  CHECK_STATUS(asked.take(many, most), Status::ok);
  CHECK_EQ(asked.count(), most); }

TEST(host_extension_list, a_win32_external_object_extension_is_asked_for_as_an_fd_one)
{ av::MockPlatform memory;
  HostExtensionList asked(memory);
  const char *const enabled[] = {"VK_KHR_external_memory_win32", "VK_EXT_memory_budget",
                                 "VK_KHR_external_semaphore_win32"};
  CHECK_STATUS(asked.take(enabled, 3u), Status::ok);
  CHECK(asked.invariants_hold());
  CHECK_EQ(asked.count(), 3u);
  CHECK_EQ(asked.kept(), 0u);
  CHECK(std::strcmp(asked.names()[0], "VK_KHR_external_memory_fd") == 0);
  CHECK(std::strcmp(asked.names()[1], "VK_EXT_memory_budget") == 0);
  CHECK(std::strcmp(asked.names()[2], "VK_KHR_external_semaphore_fd") == 0);
  const char *const nearly[] = {"VK_KHR_external_memory_win32_and_more", "VK_KHR_external_memory"};
  CHECK_STATUS(asked.take(nearly, 2u), Status::ok);
  CHECK(std::strcmp(asked.names()[0], "VK_KHR_external_memory_win32_and_more") == 0);
  CHECK(std::strcmp(asked.names()[1], "VK_KHR_external_memory") == 0);
  CHECK(asked.invariants_hold()); }

TEST(host_extension_list, a_name_the_driver_adds_is_added_once_and_is_refused_only_when_there_is_no_room)
{ av::MockPlatform memory;
  HostExtensionList asked(memory);
  const char *const enabled[] = {"VK_KHR_external_memory_fd"};
  CHECK_STATUS(asked.take(enabled, 1u), Status::ok);
  CHECK_STATUS(asked.add(nullptr), Status::invalid_parameter);
  CHECK_EQ(asked.count(), 1u);
  CHECK_STATUS(asked.add("VK_KHR_external_memory_fd"), Status::ok);
  CHECK_EQ(asked.count(), 1u);
  CHECK_STATUS(asked.add("VK_EXT_external_memory_dma_buf"), Status::ok);
  CHECK_EQ(asked.count(), 2u);
  CHECK(asked.invariants_hold());
  static const char *many[most] = {};
  static char        spellings[most][16] = {};
  spell(spellings, many, most);
  for (std::uint32_t index = 0u; index < most; index++) CHECK_STATUS(asked.add(many[index]), Status::ok);
  CHECK_EQ(asked.count(), 2u + most);
  CHECK(asked.invariants_hold());
  memory.fail_allocation_at(1u);
  const std::uint32_t held = asked.count();
  static const char  *later[4u * most] = {};
  static char         later_spellings[4u * most][16] = {};
  spell(later_spellings, later, 4u * most);
  for (std::uint32_t index = 0u; index < 4u * most; index++) later_spellings[index][0] = 'W';
  Status grown = Status::ok;
  for (std::uint32_t index = 0u; grown == Status::ok && index < 4u * most; index++) grown = asked.add(later[index]);
  CHECK_EQ(grown, Status::no_memory);
  CHECK(asked.count() >= held);
  CHECK(asked.invariants_hold()); }

TEST(host_extension_list, the_names_it_was_given_are_remembered_for_asking_about)
{ av::MockPlatform memory;
  HostExtensionList asked(memory);
  const char *const enabled[] = {"VK_KHR_swapchain", "VK_EXT_memory_budget"};
  CHECK_STATUS(asked.take(enabled, 2u), Status::ok);
  CHECK(asked.named("VK_KHR_swapchain"));
  CHECK(asked.named("VK_EXT_memory_budget"));
  CHECK(!asked.named("VK_KHR_surface"));
  CHECK(!asked.named("VK_EXT_memory_budge")); }

TEST(host_extension_list, a_count_no_array_can_hold_is_refused_before_any_name_is_read)
{ av::MockPlatform memory;
  HostExtensionList asked(memory);
  const char *const one[] = {"VK_EXT_memory_budget"};
  CHECK_STATUS(asked.take(one, 1u), Status::ok);
  CHECK_STATUS(asked.take(one, ~std::uint32_t{0}), Status::no_memory);
  CHECK_EQ(asked.count(), 0u);
  CHECK_EQ(asked.kept(), 0u);
  CHECK(asked.invariants_hold()); }

TEST(host_extension_list, its_invariants_catch_a_name_table_that_lost_count_and_more_kept_than_were_given)
{ av::MockPlatform memory;
  HostExtensionList asked(memory);
  const char *const enabled[] = {"VK_EXT_memory_budget", "VK_KHR_swapchain"};
  CHECK_STATUS(asked.take(enabled, 2u), Status::ok);
  CHECK(asked.invariants_hold());
  HostExtensionListProbe::corrupt_names(asked, 50u);
  CHECK(!asked.invariants_hold());
  HostExtensionListProbe::corrupt_names(asked, 1u);
  CHECK(asked.invariants_hold());
  HostExtensionListProbe::set_kept(asked, 5u);
  CHECK(!asked.invariants_hold());
  HostExtensionListProbe::set_kept(asked, 1u);
  CHECK(asked.invariants_hold()); }
