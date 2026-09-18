/* The extensions the host is asked for (feature 046): what an application
 * enabled, less the window-system ones this ICD answers itself. The host is a
 * Vulkan driver on the host's platform - it has no VK_KHR_win32_surface, and
 * the swapchain is this ICD's own - so passing those on is how a real
 * vulkaninfo was refused with VK_ERROR_EXTENSION_NOT_PRESENT. */
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
  /* A name that only begins like one of this ICD's own is the host's. */
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
  /* Vulkan reads no array when the count is zero, and this hands out none. */
  CHECK(asked.names() == nullptr);
  CHECK(asked.invariants_hold());
  /* An application enabling nothing is the same list. */
  CHECK_STATUS(asked.take(nullptr, 0u), Status::ok);
  CHECK_EQ(asked.count(), 0u);
  CHECK_EQ(asked.kept(), 0u);
  CHECK(asked.names() == nullptr); }

TEST(host_extension_list, a_list_it_cannot_hold_or_cannot_read_is_refused)
{ HostExtensionList asked;
  const char *const one[] = {"VK_EXT_memory_budget"};
  CHECK_STATUS(asked.take(one, 1u), Status::ok);
  CHECK_EQ(asked.count(), 1u);
  /* A count past what it holds leaves what it had rather than writing past it. */
  const char *many[HostExtensionList::max_names + 1u] = {};
  for (const char *&name : many) name = "VK_EXT_memory_budget";
  CHECK_STATUS(asked.take(many, HostExtensionList::max_names + 1u), Status::too_large);
  CHECK_EQ(asked.count(), 1u);
  CHECK(asked.invariants_hold());
  /* Exactly what it holds is held. */
  CHECK_STATUS(asked.take(many, HostExtensionList::max_names), Status::ok);
  CHECK_EQ(asked.count(), HostExtensionList::max_names);
  CHECK(asked.invariants_hold());
  /* A count with no array, and a name the application did not give. */
  CHECK_STATUS(asked.take(nullptr, 1u), Status::invalid_parameter);
  const char *const unnamed[] = {"VK_EXT_memory_budget", nullptr};
  CHECK_STATUS(asked.take(unnamed, 2u), Status::invalid_parameter);
  CHECK(asked.invariants_hold()); }

/* The two Win32 external-object extensions are asked for in the host's own
 * spelling (feature 047, US3): no Linux driver has a Win32 handle to give, and
 * what the ICD answers them over is the host's fd extensions, so the host's
 * device is made with those names in their place. */
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
  /* A name that only begins like one of them is passed on as it is. */
  const char *const nearly[] = {"VK_KHR_external_memory_win32_and_more", "VK_KHR_external_memory"};
  CHECK_STATUS(asked.take(nearly, 2u), Status::ok);
  CHECK(std::strcmp(asked.names()[0], "VK_KHR_external_memory_win32_and_more") == 0);
  CHECK(std::strcmp(asked.names()[1], "VK_KHR_external_memory") == 0);
  CHECK(asked.invariants_hold()); }
