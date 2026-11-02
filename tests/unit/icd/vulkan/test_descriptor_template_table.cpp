#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/vulkan/descriptor_template_table.hpp"

#include <cstdint>

using av::LockTrace;
using av::RecordingThreadLock;
using av::icd::DescriptorTemplateTable;

namespace {

constexpr std::uint64_t image_bytes = sizeof(VkDescriptorImageInfo);
VkDevice const          device = reinterpret_cast<VkDevice>(0x8010u);
VkDevice const          other_device = reinterpret_cast<VkDevice>(0x8020u);

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

class TableUnderLock {
public:
    TableUnderLock() : lock_(trace_, 'T'), table_(lock_) {}
    TableUnderLock(const TableUnderLock &) = delete;
    TableUnderLock &operator=(const TableUnderLock &) = delete;

    DescriptorTemplateTable &table() { return table_; }
    RecordingThreadLock     &lock() { return lock_; }

private:
    LockTrace           trace_;
    RecordingThreadLock lock_;
    DescriptorTemplateTable table_;
};

}

TEST(descriptor_template_table, what_it_records_is_what_an_update_carries)
{ TableUnderLock                  held;
  VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 4u, 16u);
  CHECK_EQ(held.table().record(device, handle(0x11u), &info), VK_SUCCESS);
  CHECK(held.table().invariants_hold());
  CHECK_EQ(held.table().live(), 1u);
  CHECK_EQ(held.table().bytes_of(handle(0x11u)), 16u + 4u * image_bytes);
  CHECK(held.lock().held() == false);
  CHECK(held.lock().acquisitions() > 0u);
  CHECK_EQ(held.table().bytes_of(handle(0x12u)), 0u);
  CHECK_EQ(held.table().bytes_of(VK_NULL_HANDLE), 0u); }

TEST(descriptor_template_table, a_create_it_cannot_record_is_refused)
{ TableUnderLock                  held;
  VkDescriptorUpdateTemplateEntry entry = {};
  VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  CHECK_EQ(held.table().record(VK_NULL_HANDLE, handle(0x21u), &info), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(held.table().record(device, VK_NULL_HANDLE, &info), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(held.table().record(device, handle(0x21u), nullptr), VK_ERROR_INITIALIZATION_FAILED);
  entry.offset = ~static_cast<std::size_t>(0);
  CHECK_EQ(held.table().record(device, handle(0x21u), &info), VK_ERROR_INITIALIZATION_FAILED);
  entry.offset = 0u;
  entry.descriptorType = VK_DESCRIPTOR_TYPE_MUTABLE_EXT;
  CHECK_EQ(held.table().record(device, handle(0x21u), &info), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(held.table().live(), 0u);
  CHECK(held.table().invariants_hold()); }

TEST(descriptor_template_table, a_table_with_no_room_refuses)
{ TableUnderLock                  held;
  VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  for (std::uint32_t index = 0u; index < DescriptorTemplateTable::max_templates; index++)
      CHECK_EQ(held.table().record(device, handle(0x1000u + index), &info), VK_SUCCESS);
  CHECK_EQ(held.table().live(), DescriptorTemplateTable::max_templates);
  CHECK_EQ(held.table().record(device, handle(0x9999u), &info), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(held.table().bytes_of(handle(0x9999u)), 0u);
  CHECK_EQ(held.table().bytes_of(handle(0x1000u)), image_bytes);
  const VkDescriptorUpdateTemplateCreateInfo wider = of(entry, 3u, 0u);
  CHECK_EQ(held.table().record(device, handle(0x1000u), &wider), VK_SUCCESS);
  CHECK_EQ(held.table().live(), DescriptorTemplateTable::max_templates);
  CHECK_EQ(held.table().bytes_of(handle(0x1000u)), 3u * image_bytes);
  CHECK(held.table().invariants_hold()); }

TEST(descriptor_template_table, it_forgets_a_template_and_a_device_s)
{ TableUnderLock                  held;
  VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 2u, 0u);
  CHECK_EQ(held.table().record(device, handle(0x31u), &info), VK_SUCCESS);
  CHECK_EQ(held.table().record(device, handle(0x32u), &info), VK_SUCCESS);
  CHECK_EQ(held.table().record(other_device, handle(0x33u), &info), VK_SUCCESS);
  CHECK_EQ(held.table().live(), 3u);
  held.table().forget(handle(0x31u));
  CHECK_EQ(held.table().live(), 2u);
  CHECK_EQ(held.table().bytes_of(handle(0x31u)), 0u);
  held.table().forget(handle(0x31u));
  held.table().forget(VK_NULL_HANDLE);
  CHECK_EQ(held.table().live(), 2u);
  held.table().forget_device(device);
  CHECK_EQ(held.table().live(), 1u);
  CHECK_EQ(held.table().bytes_of(handle(0x33u)), 2u * image_bytes);
  held.table().forget_device(reinterpret_cast<VkDevice>(0x8099u));
  held.table().forget_device(VK_NULL_HANDLE);
  CHECK_EQ(held.table().live(), 1u);
  held.table().forget_device(other_device);
  CHECK_EQ(held.table().live(), 0u);
  CHECK(held.table().invariants_hold()); }
