#include "harness/framework/test_registry.hpp"
#include "icd/vulkan/host_extension_list.hpp"

#include <cstdint>
#include <cstring>

using av::Status;
using av::icd::HostExtensionList;

TEST(host_extension_list, the_window_system_extensions_are_not_passed_on)
{ HostExtensionList asked;
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
{ HostExtensionList asked;
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

TEST(host_extension_list, a_list_it_cannot_hold_or_cannot_read_is_refused)
{ HostExtensionList asked;
  const char *const one[] = {"VK_EXT_memory_budget"};
  CHECK_STATUS(asked.take(one, 1u), Status::ok);
  CHECK_EQ(asked.count(), 1u);
  const char *many[HostExtensionList::max_names + 1u] = {};
  for (const char *&name : many) name = "VK_EXT_memory_budget";
  CHECK_STATUS(asked.take(many, HostExtensionList::max_names + 1u), Status::too_large);
  CHECK_EQ(asked.count(), 1u);
  CHECK(asked.invariants_hold());
  CHECK_STATUS(asked.take(many, HostExtensionList::max_names), Status::ok);
  CHECK_EQ(asked.count(), HostExtensionList::max_names);
  CHECK(asked.invariants_hold());
  CHECK_STATUS(asked.take(nullptr, 1u), Status::invalid_parameter);
  const char *const unnamed[] = {"VK_EXT_memory_budget", nullptr};
  CHECK_STATUS(asked.take(unnamed, 2u), Status::invalid_parameter);
  CHECK(asked.invariants_hold()); }

TEST(host_extension_list, a_win32_external_object_extension_is_asked_for_as_an_fd_one)
{ HostExtensionList asked;
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

TEST(host_extension_list, a_name_the_driver_adds_is_added_once_and_only_while_there_is_room)
{ HostExtensionList asked;
  const char *const enabled[] = {"VK_KHR_external_memory_fd"};
  CHECK_STATUS(asked.take(enabled, 1u), Status::ok);
  CHECK(!asked.add(nullptr));
  CHECK_EQ(asked.count(), 1u);
  CHECK(asked.add("VK_KHR_external_memory_fd"));
  CHECK_EQ(asked.count(), 1u);
  CHECK(asked.add("VK_EXT_external_memory_dma_buf"));
  CHECK_EQ(asked.count(), 2u);
  CHECK(asked.invariants_hold());
  HostExtensionList full;
  const char       *many[HostExtensionList::max_names] = {};
  char              spellings[HostExtensionList::max_names][16] = {};
  for (std::uint32_t index = 0u; index < HostExtensionList::max_names; index++) {
      spellings[index][0] = 'V';
      spellings[index][1] = static_cast<char>('a' + index % 26u);
      spellings[index][2] = static_cast<char>('a' + index / 26u);
      many[index] = spellings[index];
  }
  CHECK_STATUS(full.take(many, HostExtensionList::max_names), Status::ok);
  CHECK(!full.add("VK_EXT_external_memory_dma_buf"));
  CHECK_EQ(full.count(), HostExtensionList::max_names);
  CHECK(full.invariants_hold()); }
