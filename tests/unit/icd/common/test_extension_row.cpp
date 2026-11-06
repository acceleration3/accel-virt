#include "harness/framework/test_registry.hpp"
#include "icd/common/extension_row.hpp"
#include "icd/common/extension_rows.hpp"

#include <cstdint>

using av::icd::ExtensionRow;
using av::icd::ExtensionRows;

namespace {

const ExtensionRow rows[] = {
    {"VK_KHR_swapchain", 70u, "swapchain"},
    {"VK_KHR_external_memory", 1u, "external", "VK_KHR_external_memory_fd"},
    {"VK_EXT_debug_utils", 2u, "debug"},
};
constexpr std::uint32_t count = sizeof rows / sizeof rows[0];
const char *const       none[] = {nullptr};
const char *const       withheld[] = {"VK_EXT_debug_utils"};
const char *const       carried[] = {"VK_KHR_swapchain", "VK_KHR_external_memory"};

}

TEST(extension_row, a_row_without_a_host_name_answers_to_its_own_and_one_with_a_host_name_to_that)
{ const ExtensionRows table(rows, count, "VK_", ExtensionRows::Names(none, 0u));
  CHECK_EQ(table.count(), count);
  CHECK(table.invariants_hold());
  CHECK(table.row(0u).host_ == nullptr);
  CHECK(table.row(1u).host_ != nullptr);
  CHECK(table.host_name(0u) == rows[0].name_);
  CHECK(table.host_name(1u) == rows[1].host_);
  CHECK_EQ(table.answering("VK_KHR_external_memory_fd", 0u), 1u);
  CHECK_EQ(table.answering("VK_KHR_external_memory", 0u), count);
  CHECK_EQ(table.answering("VK_KHR_swapchain", 1u), count);
  CHECK_EQ(table.answering(nullptr, 0u), count); }

TEST(extension_row, the_reported_version_is_the_hosts_when_lower_and_a_renamed_rows_own_always)
{ const ExtensionRows table(rows, count, "VK_", ExtensionRows::Names(none, 0u));
  CHECK_EQ(table.version(0u, 50u), 50u);
  CHECK_EQ(table.version(0u, 99u), 70u);
  CHECK_EQ(table.version(1u, 5u), 1u); }

TEST(extension_row, a_withheld_row_is_never_reportable_and_a_carried_list_limits_the_rest)
{ const ExtensionRows own(rows, count, "VK_", ExtensionRows::Names(withheld, 1u));
  CHECK(own.reportable(0u));
  CHECK(!own.reportable(2u));
  CHECK_EQ(own.named("VK_EXT_debug_utils"), count);
  CHECK_EQ(own.named("VK_KHR_swapchain"), 0u);
  CHECK_EQ(own.named(nullptr), count);
  const char *const only[] = {"VK_KHR_swapchain"};
  const ExtensionRows limited(rows, count, "VK_", ExtensionRows::Names(only, 1u), ExtensionRows::Names(none, 0u));
  CHECK(limited.reportable(0u));
  CHECK(!limited.reportable(1u));
  CHECK_EQ(limited.named("VK_KHR_external_memory"), count);
  const ExtensionRows both(rows, count, "VK_", ExtensionRows::Names(carried, 2u), ExtensionRows::Names(withheld, 1u));
  CHECK(both.reportable(1u));
  CHECK(!both.reportable(2u)); }

TEST(extension_row, a_table_with_a_row_outside_its_prefix_a_zero_version_or_a_repeat_is_unsound)
{ const ExtensionRow foreign[] = {{"GL_ARB_thing", 1u, "t"}};
  CHECK(!ExtensionRows(foreign, 1u, "VK_", ExtensionRows::Names(none, 0u)).invariants_hold());
  const ExtensionRow unversioned[] = {{"VK_KHR_a", 0u, "t"}};
  CHECK(!ExtensionRows(unversioned, 1u, "VK_", ExtensionRows::Names(none, 0u)).invariants_hold());
  const ExtensionRow untested[] = {{"VK_KHR_a", 1u, ""}};
  CHECK(!ExtensionRows(untested, 1u, "VK_", ExtensionRows::Names(none, 0u)).invariants_hold());
  const ExtensionRow repeated[] = {{"VK_KHR_a", 1u, "t"}, {"VK_KHR_a", 2u, "t"}};
  CHECK(!ExtensionRows(repeated, 2u, "VK_", ExtensionRows::Names(none, 0u)).invariants_hold());
  const ExtensionRow emptyhost[] = {{"VK_KHR_a", 1u, "t", ""}};
  CHECK(!ExtensionRows(emptyhost, 1u, "VK_", ExtensionRows::Names(none, 0u)).invariants_hold());
  CHECK(ExtensionRows(rows, count, "VK_", ExtensionRows::Names(none, 0u)).invariants_hold()); }
