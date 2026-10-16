/* The templates a guest made on one context, and the blob length each one
 * accounts for (feature 047, contracts/descriptor_templates.md). The guest
 * writes the length of a WithTemplate command's blob, and a length from the
 * guest is a claim: this table is what the device checks it against, so a
 * template it does not know accepts no blob at all. */
#include "core/support/status.hpp"
#include "executor/vulkan/host_template_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

/* The states no recorded template can leave the table in. */
struct HostTemplateTableProbe {
    static void a_template_of_no_device(HostTemplateTable &table, std::uint64_t made)
    {
        table.templates_.find(made)->device_ = VK_NULL_HANDLE;
    }
    static void bytes_reserved_for_no_device(HostTemplateTable &table) { table.reserved_.bytes_ = 8u; }
    static void a_null_handle_held(HostTemplateTable &table)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(table.templates_.insert(0u, HostTemplate{reinterpret_cast<VkDevice>(0x7010u), 8u}, slot));
    }
};

}  // namespace av

using av::HostTemplateTable;
using av::MockPlatform;
using av::HostTemplateTableProbe;
using av::Status;

namespace {

constexpr std::uint64_t image_bytes = sizeof(VkDescriptorImageInfo);
VkDevice const          device = reinterpret_cast<VkDevice>(0x7010u);
VkDevice const          other_device = reinterpret_cast<VkDevice>(0x7020u);

VkDescriptorUpdateTemplate handle(std::uint64_t value)
{
    return reinterpret_cast<VkDescriptorUpdateTemplate>(value);
}

/* A create info of `count` image descriptors at `offset`. */
VkDescriptorUpdateTemplateCreateInfo of(VkDescriptorUpdateTemplateEntry &entry, std::uint32_t count,
                                        std::size_t offset)
{
    entry = VkDescriptorUpdateTemplateEntry{};
    entry.descriptorCount = count;
    entry.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    entry.offset = offset;
    entry.stride = image_bytes;
    VkDescriptorUpdateTemplateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
    info.descriptorUpdateEntryCount = 1u;
    info.pDescriptorUpdateEntries = &entry;
    return info;
}

}  // namespace

TEST(host_template_table, a_recorded_template_accounts_for_its_own_blob)
{ MockPlatform      platform;
  HostTemplateTable table(platform);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.live(), 0u);
  VkDescriptorUpdateTemplateEntry            entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 4u, 16u);
  CHECK_STATUS(table.reserve(device, &info), Status::ok);
  table.name(handle(0x11u));
  CHECK(table.invariants_hold());
  CHECK_EQ(table.live(), 1u);
  CHECK_EQ(table.bytes_of(handle(0x11u)), 16u + 4u * image_bytes);
  /* A handle this context never made accounts for nothing, which is what
   * refuses every blob but an empty one. */
  CHECK_EQ(table.bytes_of(handle(0x12u)), 0u);
  CHECK_EQ(table.bytes_of(VK_NULL_HANDLE), 0u);
  /* Forgotten, it accounts for nothing either. */
  table.forget(handle(0x11u));
  CHECK_EQ(table.live(), 0u);
  CHECK_EQ(table.bytes_of(handle(0x11u)), 0u);
  /* Forgetting one it never had, and the null handle, is no-one's error. */
  table.forget(handle(0x11u));
  table.forget(VK_NULL_HANDLE);
  CHECK(table.invariants_hold()); }

/* Entries the guest sent are checked before the driver is asked for anything:
 * the guest is not trusted to have checked them (constitution V), and a
 * template the device cannot size is one it never lets exist. */
TEST(host_template_table, entries_the_guest_sent_are_checked_again)
{ MockPlatform                    platform;
  HostTemplateTable               table(platform);
  VkDescriptorUpdateTemplateEntry entry = {};
  VkDescriptorUpdateTemplateCreateInfo info = of(entry, 2u, 0u);
  entry.offset = ~static_cast<std::size_t>(0);
  CHECK_STATUS(table.reserve(device, &info), Status::malformed);
  CHECK_EQ(table.live(), 0u);
  entry.offset = 0u;
  entry.descriptorType = VK_DESCRIPTOR_TYPE_MUTABLE_EXT;
  CHECK_STATUS(table.reserve(device, &info), Status::malformed);
  entry.descriptorType = static_cast<VkDescriptorType>(0x7FFFFFFF);
  CHECK_STATUS(table.reserve(device, &info), Status::malformed);
  /* Nothing to reserve it for, or nothing to reserve. */
  entry.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  CHECK_STATUS(table.reserve(VK_NULL_HANDLE, &info), Status::invalid_parameter);
  CHECK_STATUS(table.reserve(device, nullptr), Status::invalid_parameter);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold()); }

/* A handle the driver hands out again is the template it is now, a device takes
 * its own templates with it, and a table with no room refuses. */
TEST(host_template_table, a_handle_reused_is_the_template_it_is_now)
{ MockPlatform                    platform;
  HostTemplateTable               table(platform);
  VkDescriptorUpdateTemplateEntry entry = {};
  VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  CHECK_STATUS(table.reserve(device, &info), Status::ok);
  table.name(handle(0x31u));
  CHECK_EQ(table.bytes_of(handle(0x31u)), image_bytes);
  VkDescriptorUpdateTemplateEntry            wide_entry = {};
  const VkDescriptorUpdateTemplateCreateInfo wider = of(wide_entry, 8u, 0u);
  CHECK_STATUS(table.reserve(device, &wider), Status::ok);
  table.name(handle(0x31u));
  CHECK_EQ(table.live(), 1u);
  CHECK_EQ(table.bytes_of(handle(0x31u)), 8u * image_bytes);

  /* Two devices, and one destroyed. */
  CHECK_STATUS(table.reserve(other_device, &info), Status::ok);
  table.name(handle(0x32u));
  CHECK_EQ(table.live(), 2u);
  table.forget_device(device);
  CHECK_EQ(table.live(), 1u);
  CHECK_EQ(table.bytes_of(handle(0x32u)), image_bytes);
  table.forget_device(VK_NULL_HANDLE);
  CHECK_EQ(table.live(), 1u);
  table.forget_device(other_device);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold()); }

/* A closing context forgets every template, its devices' and a reservation
 * the driver never answered alike, and the table is whole again. */
TEST(host_template_table, forgetting_all_empties_every_slot)
{ MockPlatform                    platform;
  HostTemplateTable               table(platform);
  VkDescriptorUpdateTemplateEntry entry = {};
  VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  CHECK_STATUS(table.reserve(device, &info), Status::ok);
  table.name(handle(0x41u));
  CHECK_STATUS(table.reserve(other_device, &info), Status::ok);
  table.name(handle(0x42u));
  CHECK_STATUS(table.reserve(device, &info), Status::ok);
  table.forget_all();
  CHECK_EQ(table.live(), 0u);
  CHECK_EQ(table.bytes_of(handle(0x41u)), 0u);
  CHECK_EQ(table.bytes_of(handle(0x42u)), 0u);
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.reserve(device, &info), Status::ok);
  table.name(handle(0x41u));
  CHECK_EQ(table.bytes_of(handle(0x41u)), image_bytes); }

/* Well past the 512 a context once held, every template accounts for its own
 * blob; a table that cannot grow refuses the next before the driver is asked,
 * and forgets none it had. */
TEST(host_template_table, holds_thousands_and_refuses_only_when_it_cannot_grow)
{ MockPlatform                    platform;
  HostTemplateTable               table(platform);
  VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  constexpr std::uint32_t many = 5000u;
  for (std::uint32_t index = 0u; index < many; index++) {
      CHECK_STATUS(table.reserve(device, &info), Status::ok);
      table.name(handle(0x1000u + index));
  }
  CHECK_EQ(table.live(), many);
  for (std::uint32_t index = 0u; index < many; index++) CHECK_EQ(table.bytes_of(handle(0x1000u + index)), image_bytes);
  CHECK(table.invariants_hold());
  std::uint32_t index = many;
  platform.fail_allocation_at(1u);
  while (table.reserve(device, &info) == Status::ok) table.name(handle(0x1000u + index++));
  CHECK_EQ(table.bytes_of(handle(0x1000u + index)), 0u);
  CHECK_EQ(table.bytes_of(handle(0x1000u)), image_bytes);
  CHECK_EQ(table.live(), index);
  /* Refused, nothing is reserved: a name that follows makes no template. */
  table.name(handle(0x1000u + index));
  CHECK_EQ(table.bytes_of(handle(0x1000u + index)), 0u);
  CHECK(table.invariants_hold()); }

/* A create the driver refused: the reservation is released, so the slot is not
 * a template of no handle and the next create reuses it. */
TEST(host_template_table, a_reservation_the_driver_never_answered_is_released)
{ MockPlatform                    platform;
  HostTemplateTable               table(platform);
  VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  CHECK_STATUS(table.reserve(device, &info), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.live(), 0u);   /* reserved is not live */
  table.name(VK_NULL_HANDLE);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold());
  /* A name with nothing reserved is no-one's error. */
  table.name(handle(0x51u));
  CHECK_EQ(table.live(), 0u);
  CHECK_EQ(table.bytes_of(handle(0x51u)), 0u);
  /* A handle the driver hands out again while another slot still holds it:
   * the older slot is not a second template of the same handle. */
  CHECK_STATUS(table.reserve(device, &info), Status::ok);
  table.name(handle(0x53u));
  CHECK_STATUS(table.reserve(other_device, &info), Status::ok);
  table.name(handle(0x53u));
  CHECK_EQ(table.live(), 1u);
  CHECK(table.invariants_hold());
  table.forget(handle(0x53u));
  /* A second reserve after one the driver never answered reuses the slot
   * rather than taking another. */
  CHECK_STATUS(table.reserve(device, &info), Status::ok);
  CHECK_STATUS(table.reserve(other_device, &info), Status::ok);
  table.name(handle(0x52u));
  CHECK_EQ(table.live(), 1u);
  CHECK_EQ(table.bytes_of(handle(0x52u)), image_bytes);
  CHECK(table.invariants_hold()); }

TEST(host_template_table, an_invariant_refuses_each_state_it_names)
{ VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  MockPlatform platform;
  { HostTemplateTable table(platform);
    CHECK_STATUS(table.reserve(device, &info), Status::ok);
    table.name(handle(0x41u));
    HostTemplateTableProbe::a_template_of_no_device(table, 0x41u);
    CHECK(!table.invariants_hold()); }
  { HostTemplateTable table(platform);
    HostTemplateTableProbe::bytes_reserved_for_no_device(table);
    CHECK(!table.invariants_hold()); }
  { HostTemplateTable table(platform);
    HostTemplateTableProbe::a_null_handle_held(table);
    CHECK(!table.invariants_hold()); } }
