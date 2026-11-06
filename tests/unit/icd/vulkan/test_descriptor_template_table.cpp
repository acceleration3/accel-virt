#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/descriptor_template_table.hpp"

#include <cstdint>

using av::LockTrace;
using av::RecordingThreadLock;
using av::icd::DescriptorTemplateSlot;
using av::icd::DescriptorTemplateTable;

namespace av {
struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_free_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.free_count_ = count; }
};

}

namespace av::icd {

struct DescriptorTemplateTableProbe {
    static DescriptorTemplateSlot *edit(DescriptorTemplateTable &table, VkDescriptorUpdateTemplate made)
    {
        return table.slots_.find(table.key_of(made));
    }
    static void miscount(DescriptorTemplateTable &table, std::uint32_t count)
    {
        KeyedSlotsProbe::corrupt_free_count(table.slots_, count);
    }
    static const DescriptorTemplateSlot *slot(const DescriptorTemplateTable &table, VkDescriptorUpdateTemplate made)
    {
        return table.slots_.find(table.key_of(made));
    }
};

}

using av::icd::DescriptorTemplateTableProbe;

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
    TableUnderLock() : lock_(trace_, 'T'), table_(memory_, lock_) {}
    TableUnderLock(const TableUnderLock &) = delete;
    TableUnderLock &operator=(const TableUnderLock &) = delete;

    DescriptorTemplateTable &table() { return table_; }
    RecordingThreadLock     &lock() { return lock_; }
    av::MockPlatform        &memory() { return memory_; }

private:
    av::MockPlatform    memory_;
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

TEST(descriptor_template_table, a_table_grows_and_refuses_only_when_no_memory_is_left_to_grow_into)
{ TableUnderLock                  held;
  VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  constexpr std::uint32_t many = 2000u;
  for (std::uint32_t index = 0u; index < many; index++)
      CHECK_EQ(held.table().record(device, handle(0x1000u + index), &info), VK_SUCCESS);
  CHECK_EQ(held.table().live(), many);
  CHECK(held.table().invariants_hold());
  held.memory().fail_allocation_at(1u);
  std::uint32_t added = 0u;
  VkResult      result = VK_SUCCESS;
  while (result == VK_SUCCESS && added < 8u * many) {
      result = held.table().record(device, handle(0x100000u + added), &info);
      if (result == VK_SUCCESS) added++;
  }
  CHECK_EQ(result, VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(held.table().live(), many + added);
  CHECK_EQ(held.table().bytes_of(handle(0x100000u + added)), 0u);
  CHECK_EQ(held.table().bytes_of(handle(0x1000u)), image_bytes);
  const VkDescriptorUpdateTemplateCreateInfo wider = of(entry, 3u, 0u);
  CHECK_EQ(held.table().record(device, handle(0x1000u), &wider), VK_SUCCESS);
  CHECK_EQ(held.table().live(), many + added);
  CHECK_EQ(held.table().bytes_of(handle(0x1000u)), 3u * image_bytes);
  CHECK(held.table().invariants_hold());
  held.memory().fail_allocation_at(0u);
  CHECK_EQ(held.table().record(device, handle(0x100000u + added), &info), VK_SUCCESS);
  CHECK_EQ(held.table().live(), many + added + 1u);
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

TEST(descriptor_template_table, a_slot_names_the_device_the_template_and_the_bytes_of_one_update)
{ TableUnderLock                  held;
  VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 3u, 8u);
  CHECK(DescriptorTemplateTableProbe::slot(held.table(), handle(0x41u)) == nullptr);
  CHECK_EQ(held.table().record(device, handle(0x41u), &info), VK_SUCCESS);
  CHECK_EQ(held.table().record(other_device, handle(0x42u), &info), VK_SUCCESS);
  const DescriptorTemplateSlot *const slot = DescriptorTemplateTableProbe::slot(held.table(), handle(0x41u));
  CHECK(slot != nullptr);
  CHECK(slot->device_ == device);
  CHECK(slot->template_ == handle(0x41u));
  CHECK_EQ(slot->bytes_, 8u + 3u * image_bytes);
  CHECK(DescriptorTemplateTableProbe::slot(held.table(), handle(0x42u))->device_ == other_device);
  held.table().forget_device(device);
  CHECK(DescriptorTemplateTableProbe::slot(held.table(), handle(0x41u)) == nullptr);
  CHECK(DescriptorTemplateTableProbe::slot(held.table(), handle(0x42u)) != nullptr);
  CHECK(held.table().invariants_hold()); }

TEST(descriptor_template_table, its_invariants_catch_a_slot_with_no_device_a_wrong_key_and_a_miscounted_table)
{ TableUnderLock                  held;
  VkDescriptorUpdateTemplateEntry entry = {};
  const VkDescriptorUpdateTemplateCreateInfo info = of(entry, 1u, 0u);
  CHECK_EQ(held.table().record(device, handle(0x31u), &info), VK_SUCCESS);
  CHECK_EQ(held.table().record(device, handle(0x32u), &info), VK_SUCCESS);
  CHECK(held.table().invariants_hold());
  DescriptorTemplateSlot *const first = DescriptorTemplateTableProbe::edit(held.table(), handle(0x31u));
  DescriptorTemplateSlot *const second = DescriptorTemplateTableProbe::edit(held.table(), handle(0x32u));
  first->device_ = VK_NULL_HANDLE;
  CHECK(!held.table().invariants_hold());
  first->device_ = device;
  second->template_ = handle(0x99u);
  CHECK(!held.table().invariants_hold());
  second->template_ = handle(0x32u);
  CHECK(held.table().invariants_hold());
  DescriptorTemplateTableProbe::miscount(held.table(), 5u);
  CHECK(!held.table().invariants_hold());
  DescriptorTemplateTableProbe::miscount(held.table(), 0u);
  CHECK(held.table().invariants_hold()); }
