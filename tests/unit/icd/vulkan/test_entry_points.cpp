/* Resolving a Vulkan entry point by name: a local row wins over the generated
 * wrapper of the same name, a generated name answers its wrapper, and anything
 * else - an unknown name, an empty or null one - answers null. find_local never
 * falls back to a wrapper; find_physical_device_level answers only a row the
 * loader dispatches on a physical device. A table naming an entry point twice
 * breaks the invariant. */
#include "harness/framework/test_registry.hpp"
#include "icd/vulkan/entry_points.hpp"
#include "proto/generated/vulkan/vk_icd_entries.h"
#include "proto/generated/vulkan/vk_supported_extensions.h"

#include <cstdint>
#include <cstring>

using av::icd::EntryPoints;
using Entry = av::vkicd::generated::Entry;
using Level = av::vkicd::generated::DispatchLevel;

namespace {

/* Distinct functions to stand for local implementations: only their addresses
 * are compared. */
VKAPI_ATTR void VKAPI_CALL local_destroy_device(VkDevice, const VkAllocationCallbacks *) {}
VKAPI_ATTR VkResult VKAPI_CALL local_instance_version(std::uint32_t *) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL local_instance_extensions(const char *, std::uint32_t *, VkExtensionProperties *)
{
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL local_features(VkPhysicalDevice, VkPhysicalDeviceFeatures *) {}

PFN_vkVoidFunction as_entry(VKAPI_ATTR void (VKAPI_CALL *function)(VkDevice, const VkAllocationCallbacks *))
{
    return reinterpret_cast<PFN_vkVoidFunction>(function);
}

const Entry rows[] = {
    {"vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(&local_destroy_device), Level::device},
    {"vkEnumerateInstanceVersion", reinterpret_cast<PFN_vkVoidFunction>(&local_instance_version), Level::global},
    {"vkEnumerateInstanceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(&local_instance_extensions),
     Level::global},
    {"vkGetPhysicalDeviceFeatures", reinterpret_cast<PFN_vkVoidFunction>(&local_features), Level::physical_device},
};
constexpr std::uint32_t row_count = sizeof rows / sizeof rows[0];

}  // namespace

TEST(entry_points, a_local_row_overrides_the_generated_wrapper_of_its_name)
{ EntryPoints entry_points(rows, row_count);
  CHECK(entry_points.invariants_hold());
  const PFN_vkVoidFunction wrapper = reinterpret_cast<PFN_vkVoidFunction>(&av::vkicd::generated::Wrap_vkDestroyDevice);
  CHECK(entry_points.find("vkDestroyDevice") == as_entry(&local_destroy_device));
  CHECK(entry_points.find("vkDestroyDevice") != wrapper);
  CHECK(entry_points.find_local("vkDestroyDevice") == as_entry(&local_destroy_device)); }

TEST(entry_points, a_generated_name_resolves_to_its_wrapper_but_not_locally)
{ EntryPoints entry_points(rows, row_count);
  CHECK(entry_points.find("vkCreateBuffer") ==
        reinterpret_cast<PFN_vkVoidFunction>(&av::vkicd::generated::Wrap_vkCreateBuffer));
  CHECK(entry_points.find_local("vkCreateBuffer") == nullptr);
  /* The last row of the generated table resolves too: the search does not stop short. */
  CHECK(entry_points.find("vkUnmapMemory2KHR") ==
        reinterpret_cast<PFN_vkVoidFunction>(&av::vkicd::generated::Wrap_vkUnmapMemory2KHR)); }

TEST(entry_points, every_generated_row_resolves_to_its_own_wrapper_with_no_local_rows)
{ EntryPoints entry_points(nullptr, 0u);
  CHECK(entry_points.invariants_hold());
  std::uint32_t resolved = 0u;
  for (const Entry &row : av::vkicd::generated::wrapper_entries()) {
      CHECK(entry_points.find(row.name) == row.fn);
      resolved++;
  }
  CHECK_EQ(resolved, av::vkicd::generated::wrapper_entries().size()); }

TEST(entry_points, every_manual_entry_point_implemented_locally_resolves_to_its_local_row)
{ EntryPoints entry_points(rows, row_count);
  std::uint32_t local = 0u;
  for (const char *name : av::vkproto::kManualEntryPoints) {
      const PFN_vkVoidFunction found = entry_points.find(name);
      bool is_local_row = false;
      for (const Entry &row : rows) is_local_row = is_local_row || (std::strcmp(row.name, name) == 0 && row.fn == found);
      /* A manual entry point has no wrapper: found is a local row or nothing. */
      CHECK(found == nullptr || is_local_row);
      if (found != nullptr) local++;
  }
  CHECK_EQ(local, 2u);
  CHECK(entry_points.find("vkEnumerateInstanceVersion") == reinterpret_cast<PFN_vkVoidFunction>(&local_instance_version));
  CHECK(entry_points.find("vkEnumerateInstanceExtensionProperties") ==
        reinterpret_cast<PFN_vkVoidFunction>(&local_instance_extensions)); }

TEST(entry_points, an_unknown_empty_or_null_name_resolves_to_nothing)
{ EntryPoints entry_points(rows, row_count);
  CHECK(entry_points.find("vkNotAnEntryPoint") == nullptr);
  CHECK(entry_points.find("vkDestroyDevic") == nullptr);
  CHECK(entry_points.find("vkDestroyDevice2") == nullptr);
  CHECK(entry_points.find("") == nullptr);
  CHECK(entry_points.find(nullptr) == nullptr);
  CHECK(entry_points.find_local("vkNotAnEntryPoint") == nullptr);
  CHECK(entry_points.find_local("") == nullptr);
  CHECK(entry_points.find_local(nullptr) == nullptr); }

TEST(entry_points, invariants_refuse_a_table_with_a_hole_or_a_name_twice)
{ const Entry twice[] = {rows[0], rows[1], rows[0]};
  CHECK(!EntryPoints(twice, 3u).invariants_hold());
  CHECK(EntryPoints(twice, 2u).invariants_hold());
  const Entry unnamed[] = {rows[0], {nullptr, rows[1].fn, Level::global}};
  CHECK(!EntryPoints(unnamed, 2u).invariants_hold());
  const Entry empty[] = {rows[0], {"vkEnumerateInstanceVersion", nullptr, Level::global}};
  CHECK(!EntryPoints(empty, 2u).invariants_hold()); }

TEST(entry_points, a_physical_device_level_name_answers_its_local_row_or_its_wrapper)
{ EntryPoints entry_points(rows, row_count);
  CHECK(entry_points.find_physical_device_level("vkGetPhysicalDeviceFeatures") ==
        reinterpret_cast<PFN_vkVoidFunction>(&local_features));
  CHECK(entry_points.find_physical_device_level("vkGetPhysicalDeviceFeatures2") ==
        reinterpret_cast<PFN_vkVoidFunction>(&av::vkicd::generated::Wrap_vkGetPhysicalDeviceFeatures2));
  CHECK(entry_points.find_physical_device_level("vkCreateDevice") ==
        reinterpret_cast<PFN_vkVoidFunction>(&av::vkicd::generated::Wrap_vkCreateDevice));
  CHECK(entry_points.find_physical_device_level("vkEnumerateDeviceExtensionProperties") ==
        reinterpret_cast<PFN_vkVoidFunction>(&av::vkicd::generated::Wrap_vkEnumerateDeviceExtensionProperties)); }

/* What the loader would dispatch on the wrong object: a device, queue or
 * command buffer command, an instance command, and one needing no object. */
TEST(entry_points, any_other_level_answers_no_physical_device_entry_point)
{ EntryPoints entry_points(rows, row_count);
  const char *const not_physical_device_level[] = {
      "vkDestroyDevice", "vkCreateBuffer", "vkQueueSubmit", "vkCmdDraw", "vkEnumeratePhysicalDevices",
      "vkEnumeratePhysicalDeviceGroups", "vkDestroyInstance", "vkCreateInstance", "vkEnumerateInstanceVersion",
      "vkNotAnEntryPoint", "", nullptr};
  for (const char *name : not_physical_device_level)
      CHECK(entry_points.find_physical_device_level(name) == nullptr);
  CHECK(entry_points.find("vkDestroyDevice") != nullptr);
  CHECK(entry_points.find("vkCreateBuffer") != nullptr); }

/* A local row's level is its own: one naming a generated physical-device
 * command at another level is not answered, and the wrapper is not reached. */
TEST(entry_points, a_local_rows_level_overrides_the_wrappers)
{ const Entry relevelled[] = {
      {"vkGetPhysicalDeviceFeatures2", reinterpret_cast<PFN_vkVoidFunction>(&local_features), Level::device}};
  EntryPoints entry_points(relevelled, 1u);
  CHECK(entry_points.find_physical_device_level("vkGetPhysicalDeviceFeatures2") == nullptr);
  CHECK(entry_points.find("vkGetPhysicalDeviceFeatures2") == reinterpret_cast<PFN_vkVoidFunction>(&local_features)); }

/* The generated table's levels as vk.xml's first parameters give them: every
 * physical-device query and vkCreateDevice dispatch on a physical device, every
 * recorded command on its device. */
TEST(entry_points, every_generated_row_at_physical_device_level_is_answered_and_no_other)
{ EntryPoints entry_points(nullptr, 0u);
  std::uint32_t answered = 0u;
  for (const Entry &row : av::vkicd::generated::wrapper_entries()) {
      const PFN_vkVoidFunction found = entry_points.find_physical_device_level(row.name);
      CHECK(found == (row.level == Level::physical_device ? row.fn : nullptr));
      if (found != nullptr) answered++;
      if (std::strncmp(row.name, "vkGetPhysicalDevice", 19) == 0) CHECK(row.level == Level::physical_device);
      if (std::strncmp(row.name, "vkCmd", 5) == 0) CHECK(row.level == Level::device);
  }
  CHECK(answered >= 17u); }
