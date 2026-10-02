/* The templates a guest made on one context, and the blob length each one
 * accounts for (feature 047, contracts/descriptor_templates.md). The guest
 * writes the length of a WithTemplate command's blob, and a length from the
 * guest is a claim: this table is what the device checks it against, so a
 * template it does not know accepts no blob at all. */
#include "core/support/status.hpp"
#include "executor/vulkan/host_template_table.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace av {

/* The state no recorded template can leave the table in. */
struct HostTemplateTableProbe {
    static void a_slot_of_no_device(HostTemplateTable &table) { table.slots_[0].device_ = VK_NULL_HANDLE; }
    static void the_same_template_twice(HostTemplateTable &table) { table.slots_[1] = table.slots_[0]; }
    static void a_named_slot_still_reserved(HostTemplateTable &table) { table.slots_[0].reserved_ = true; }
    static void a_device_in_an_empty_slot(HostTemplateTable &table)
    {
        for (HostTemplateSlot &slot : table.slots_)
            if (slot.template_ == VK_NULL_HANDLE) {
                slot.device_ = reinterpret_cast<VkDevice>(0x7010u);
                return;
            }
    }
    static void bytes_in_an_empty_slot(HostTemplateTable &table)
    {
        for (HostTemplateSlot &slot : table.slots_)
            if (slot.template_ == VK_NULL_HANDLE) {
                slot.bytes_ = 8u;
                return;
            }
    }
};

}  // namespace av

using av::HostTemplateTable;
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
{ HostTemplateTable table;
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
{ HostTemplateTable               table;
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
{ HostTemplateTable               table;
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
{ HostTemplateTable               table;
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

TEST(host_template_table, a_table_with_no_room_refuses_rather_than_forgetting_one)
{ HostTemplateTable               table;
  VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  for (std::uint32_t index = 0u; index < HostTemplateTable::max_templates; index++) {
      CHECK_STATUS(table.reserve(device, &info), Status::ok);
      table.name(handle(0x1000u + index));
  }
  CHECK_EQ(table.live(), HostTemplateTable::max_templates);
  CHECK_STATUS(table.reserve(device, &info), Status::no_space);
  /* The one it could not take accounts for nothing; the ones it had are intact. */
  CHECK_EQ(table.bytes_of(handle(0x9999u)), 0u);
  CHECK_EQ(table.bytes_of(handle(0x1000u)), image_bytes);
  CHECK(table.invariants_hold()); }

/* A create the driver refused: the reservation is released, so the slot is not
 * a template of no handle and the next create reuses it. */
TEST(host_template_table, a_reservation_the_driver_never_answered_is_released)
{ HostTemplateTable               table;
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
  { HostTemplateTable table;
    CHECK_STATUS(table.reserve(device, &info), Status::ok);
    table.name(handle(0x41u));
    HostTemplateTableProbe::a_slot_of_no_device(table);
    CHECK(!table.invariants_hold()); }
  { HostTemplateTable table;
    CHECK_STATUS(table.reserve(device, &info), Status::ok);
    table.name(handle(0x42u));
    HostTemplateTableProbe::the_same_template_twice(table);
    CHECK(!table.invariants_hold()); }
  { HostTemplateTable table;
    HostTemplateTableProbe::bytes_in_an_empty_slot(table);
    CHECK(!table.invariants_hold()); }
  { HostTemplateTable table;
    HostTemplateTableProbe::a_device_in_an_empty_slot(table);
    CHECK(!table.invariants_hold()); }
  { HostTemplateTable table;
    CHECK_STATUS(table.reserve(device, &info), Status::ok);
    table.name(handle(0x43u));
    HostTemplateTableProbe::a_named_slot_still_reserved(table);
    CHECK(!table.invariants_hold()); } }
