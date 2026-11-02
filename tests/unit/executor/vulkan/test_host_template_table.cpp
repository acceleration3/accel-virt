#include "core/support/status.hpp"
#include "executor/vulkan/host_template_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {
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

}

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

}

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
  CHECK_EQ(table.bytes_of(handle(0x12u)), 0u);
  CHECK_EQ(table.bytes_of(VK_NULL_HANDLE), 0u);
  table.forget(handle(0x11u));
  CHECK_EQ(table.live(), 0u);
  CHECK_EQ(table.bytes_of(handle(0x11u)), 0u);
  table.forget(handle(0x11u));
  table.forget(VK_NULL_HANDLE);
  CHECK(table.invariants_hold()); }

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
  entry.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  CHECK_STATUS(table.reserve(VK_NULL_HANDLE, &info), Status::invalid_parameter);
  CHECK_STATUS(table.reserve(device, nullptr), Status::invalid_parameter);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold()); }

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
  table.name(handle(0x1000u + index));
  CHECK_EQ(table.bytes_of(handle(0x1000u + index)), 0u);
  CHECK(table.invariants_hold()); }

TEST(host_template_table, a_reservation_the_driver_never_answered_is_released)
{ MockPlatform                    platform;
  HostTemplateTable               table(platform);
  VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  CHECK_STATUS(table.reserve(device, &info), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.live(), 0u);
  table.name(VK_NULL_HANDLE);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold());
  table.name(handle(0x51u));
  CHECK_EQ(table.live(), 0u);
  CHECK_EQ(table.bytes_of(handle(0x51u)), 0u);
  CHECK_STATUS(table.reserve(device, &info), Status::ok);
  table.name(handle(0x53u));
  CHECK_STATUS(table.reserve(other_device, &info), Status::ok);
  table.name(handle(0x53u));
  CHECK_EQ(table.live(), 1u);
  CHECK(table.invariants_hold());
  table.forget(handle(0x53u));
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
