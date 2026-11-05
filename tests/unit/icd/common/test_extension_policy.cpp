#include "harness/framework/test_registry.hpp"
#include "icd/vulkan/extension_policy.hpp"

#include <cstdint>
#include <cstring>

#include <vulkan/vulkan_core.h>

using av::icd::ExtensionPolicy;
using Row = av::icd::ExtensionPolicy::Row;

namespace {

const Row rows[] = {
    {"VK_KHR_maintenance1", 2u, "extension_policy.test"},
    {"VK_KHR_spirv_1_4", 1u, "extension_policy.test"},
    {"VK_NV_ray_tracing", 1u, "extension_policy.test"},
    {"VK_KHR_swapchain", 70u, "extension_policy.test"},
    {"VK_KHR_surface", 25u, "extension_policy.test"},
    {"VK_KHR_win32_surface", 6u, "extension_policy.test"},
    {"VK_EXT_external_memory_host", 1u, "extension_policy.test"},
};
constexpr std::uint32_t row_count = sizeof rows / sizeof rows[0];
constexpr std::uint32_t max_host = 512u;

VkExtensionProperties extension(const char *name, std::uint32_t spec_version)
{
    VkExtensionProperties made = {};
    std::strncpy(made.extensionName, name, sizeof made.extensionName);
    made.specVersion = spec_version;
    return made;
}

class HostList {
public:
    void add(const VkExtensionProperties &entry) { entries_[count_++] = entry; }
    void add(const char *name, std::uint32_t spec_version) { add(extension(name, spec_version)); }
    const VkExtensionProperties *entries() const { return entries_; }
    std::uint32_t                count() const { return count_; }

private:
    VkExtensionProperties entries_[max_host] = {};
    std::uint32_t         count_ = 0u;
};

}

TEST(extension_policy, reports_only_the_host_implemented_policy_extensions_the_proto_carries)
{ ExtensionPolicy policy(rows, row_count);
  CHECK(policy.invariants_hold());
  static HostList host;
  host = HostList();
  host.add("VK_KHR_timeline_semaphore", 2u);
  host.add("VK_KHR_spirv_1_4", 1u);
  host.add("VK_NV_ray_tracing", 1u);
  host.add("VK_KHR_swapchain", 70u);
  host.add("VK_KHR_surface", 25u);
  host.add("VK_KHR_win32_surface", 6u);
  host.add("VK_EXT_external_memory_host", 1u);
  host.add("VK_KHR_maintenance1", 2u);
  std::uint32_t count = 99u;
  CHECK_EQ(policy.report(host.entries(), host.count(), &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 4u);
  VkExtensionProperties reported[5] = {};
  reported[4].specVersion = 0xC0FFEEu;
  count = 5u;
  CHECK_EQ(policy.report(host.entries(), host.count(), &count, reported), VK_SUCCESS);
  CHECK_EQ(count, 4u);
  CHECK(std::strcmp(reported[0].extensionName, "VK_KHR_maintenance1") == 0);
  CHECK_EQ(reported[0].specVersion, 2u);
  CHECK(std::strcmp(reported[1].extensionName, "VK_KHR_spirv_1_4") == 0);
  CHECK(std::strcmp(reported[2].extensionName, "VK_KHR_swapchain") == 0);
  CHECK_EQ(reported[2].specVersion, 70u);
  CHECK(std::strcmp(reported[3].extensionName, "VK_EXT_external_memory_host") == 0);
  CHECK_EQ(reported[4].specVersion, 0xC0FFEEu); }

TEST(extension_policy, the_surface_extensions_are_never_reported)
{ ExtensionPolicy policy(rows, row_count);
  const char *const withheld[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};
  for (const char *name : withheld) {
      const VkExtensionProperties host = extension(name, 1u);
      std::uint32_t count = 5u;
      CHECK_EQ(policy.report(&host, 1u, &count, nullptr), VK_SUCCESS);
      CHECK_EQ(count, 0u);
      CHECK_EQ(policy.first_refused(&name, 1u), 0u);
  }
}

TEST(extension_policy, the_hosts_spec_version_is_held_to_the_policys)
{ ExtensionPolicy policy(rows, row_count);
  struct Case {
      std::uint32_t host;
      std::uint32_t reported;
  };
  const Case cases[] = {{5u, 2u}, {2u, 2u}, {1u, 1u}, {0u, 0u}, {0xFFFFFFFFu, 2u}};
  for (const Case &check : cases) {
      const VkExtensionProperties host = extension("VK_KHR_maintenance1", check.host);
      VkExtensionProperties reported = {};
      std::uint32_t count = 1u;
      CHECK_EQ(policy.report(&host, 1u, &count, &reported), VK_SUCCESS);
      CHECK_EQ(count, 1u);
      CHECK_EQ(reported.specVersion, check.reported);
  }
}

TEST(extension_policy, room_for_fewer_than_are_reported_answers_incomplete)
{ ExtensionPolicy policy(rows, row_count);
  const VkExtensionProperties host[] = {extension("VK_KHR_spirv_1_4", 1u), extension("VK_KHR_maintenance1", 2u)};
  VkExtensionProperties reported[2] = {};
  std::uint32_t count = 1u;
  CHECK_EQ(policy.report(host, 2u, &count, reported), VK_INCOMPLETE);
  CHECK_EQ(count, 1u);
  CHECK(std::strcmp(reported[0].extensionName, "VK_KHR_maintenance1") == 0);
  CHECK_EQ(reported[1].extensionName[0], '\0');
  count = 0u;
  CHECK_EQ(policy.report(host, 2u, &count, reported), VK_INCOMPLETE);
  CHECK_EQ(count, 0u);
  count = 7u;
  CHECK_EQ(policy.report(host, 0u, &count, reported), VK_SUCCESS);
  CHECK_EQ(count, 0u); }

TEST(extension_policy, a_host_name_that_is_not_exactly_the_extension_is_not_it)
{ ExtensionPolicy policy(rows, row_count);
  VkExtensionProperties unterminated = extension("VK_KHR_maintenance1", 2u);
  std::memset(unterminated.extensionName + std::strlen("VK_KHR_maintenance1"), 'x',
              sizeof unterminated.extensionName - std::strlen("VK_KHR_maintenance1"));
  VkExtensionProperties full = {};
  std::memset(full.extensionName, 'V', sizeof full.extensionName);
  const VkExtensionProperties host[] = {
      unterminated, full, extension("VK_KHR_maintenance", 2u), extension("VK_KHR_maintenance1s", 2u),
      extension("vk_khr_maintenance1", 2u), extension("", 2u)};
  std::uint32_t count = 3u;
  CHECK_EQ(policy.report(host, sizeof host / sizeof host[0], &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 0u);
  VkExtensionProperties padded = extension("VK_KHR_maintenance1", 2u);
  std::memset(padded.extensionName + std::strlen("VK_KHR_maintenance1") + 1u, 'x',
              sizeof padded.extensionName - std::strlen("VK_KHR_maintenance1") - 1u);
  VkExtensionProperties reported = {};
  count = 1u;
  CHECK_EQ(policy.report(&padded, 1u, &count, &reported), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(std::strcmp(reported.extensionName, "VK_KHR_maintenance1") == 0);
  CHECK_EQ(reported.extensionName[VK_MAX_EXTENSION_NAME_SIZE - 1u], '\0'); }

TEST(extension_policy, a_host_listing_512_extensions_is_reported_from_whole_and_each_once)
{ ExtensionPolicy policy(rows, row_count);
  static HostList junk;
  junk = HostList();
  for (std::uint32_t index = 0u; index < max_host - 1u; index++) {
      VkExtensionProperties entry = {};
      std::memset(entry.extensionName, static_cast<int>('A' + index % 26u), sizeof entry.extensionName);
      entry.specVersion = index;
      junk.add(entry);
  }
  junk.add("VK_KHR_spirv_1_4", 1u);
  VkExtensionProperties reported[2] = {};
  std::uint32_t count = 2u;
  CHECK_EQ(policy.report(junk.entries(), junk.count(), &count, reported), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(std::strcmp(reported[0].extensionName, "VK_KHR_spirv_1_4") == 0);
  static HostList repeated;
  repeated = HostList();
  for (std::uint32_t index = 0u; index < max_host; index++) repeated.add("VK_KHR_maintenance1", index);
  count = 2u;
  CHECK_EQ(policy.report(repeated.entries(), repeated.count(), &count, reported), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK_EQ(reported[0].specVersion, 0u); }

TEST(extension_policy, a_device_may_enable_exactly_what_would_be_reported)
{ ExtensionPolicy policy(rows, row_count);
  const char *const allowed[] = {"VK_KHR_maintenance1", "VK_KHR_spirv_1_4", "VK_KHR_swapchain"};
  CHECK_EQ(policy.first_refused(allowed, 3u), 3u);
  CHECK_EQ(policy.first_refused(nullptr, 0u), 0u);
  const char *const refused[] = {"VK_NV_ray_tracing", "VK_KHR_timeline_semaphore", "VK_KHR_surface",
                                 "VK_KHR_maintenance", "", nullptr};
  for (const char *name : refused) {
      const char *const names[] = {"VK_KHR_spirv_1_4", name, "VK_KHR_maintenance1"};
      CHECK_EQ(policy.first_refused(names, 3u), 1u);
  }
  ExtensionPolicy empty(nullptr, 0u);
  CHECK(empty.invariants_hold());
  CHECK_EQ(empty.first_refused(allowed, 3u), 0u); }

TEST(extension_policy, invariants_refuse_a_row_that_names_nothing_no_test_no_version_or_a_name_twice)
{ char long_name[VK_MAX_EXTENSION_NAME_SIZE + 1u] = {};
  std::memset(long_name, 'x', VK_MAX_EXTENSION_NAME_SIZE);
  const Row broken[][1] = {
      {{nullptr, 1u, "extension_policy.test"}}, {{"", 1u, "extension_policy.test"}},
      {{long_name, 1u, "extension_policy.test"}}, {{"VK_KHR_spirv_1_4", 0u, "extension_policy.test"}},
      {{"VK_KHR_spirv_1_4", 1u, nullptr}}, {{"VK_KHR_spirv_1_4", 1u, ""}},
      {{"VK_KHR_external_memory_win32", 1u, "extension_policy.test", ""}},
      {{"KHR_spirv_1_4", 1u, "extension_policy.test"}}, {{"VK_", 1u, "extension_policy.test"}},
  };
  for (const auto &row : broken) CHECK(!ExtensionPolicy(row, 1u).invariants_hold());
  std::memcpy(long_name, "VK_", 3u);
  long_name[VK_MAX_EXTENSION_NAME_SIZE - 1u] = '\0';
  const Row longest[] = {{long_name, 1u, "extension_policy.test"}};
  CHECK(ExtensionPolicy(longest, 1u).invariants_hold());
  const Row twice[] = {rows[0], rows[1], rows[0]};
  CHECK(!ExtensionPolicy(twice, 3u).invariants_hold());
  CHECK(ExtensionPolicy(twice, 2u).invariants_hold()); }

TEST(extension_policy, a_row_is_reported_over_the_host_extension_it_names)
{ const Row translated[] = {
      {"VK_KHR_external_memory_win32", 1u, "extension_policy.test", "VK_KHR_external_memory_fd"},
      {"VK_KHR_maintenance1", 2u, "extension_policy.test"},
  };
  ExtensionPolicy policy(translated, 2u);
  CHECK(policy.invariants_hold());
  HostList host;
  host.add("VK_KHR_external_memory_fd", 7u);
  host.add("VK_KHR_maintenance1", 2u);
  std::uint32_t         count = 2u;
  VkExtensionProperties reported[2] = {};
  CHECK_EQ(policy.report(host.entries(), host.count(), &count, reported), VK_SUCCESS);
  CHECK_EQ(count, 2u);
  CHECK(std::strcmp(reported[0].extensionName, "VK_KHR_external_memory_win32") == 0);
  CHECK_EQ(reported[0].specVersion, 1u);
  CHECK(std::strcmp(reported[1].extensionName, "VK_KHR_maintenance1") == 0);
  CHECK_EQ(reported[1].specVersion, 2u);

  HostList without;
  without.add("VK_KHR_external_memory_win32", 1u);
  without.add("VK_KHR_maintenance1", 2u);
  count = 2u;
  CHECK_EQ(policy.report(without.entries(), without.count(), &count, reported), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(std::strcmp(reported[0].extensionName, "VK_KHR_maintenance1") == 0);
  const char *const asked[] = {"VK_KHR_external_memory_win32"};
  CHECK_EQ(policy.first_refused(asked, 1u), 1u); }
