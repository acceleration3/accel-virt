#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/descriptor_template_table.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/guest_handle_translation.hpp"
#include "icd/vulkan/handle_bits.hpp"
#include "icd/vulkan/handle_registry.hpp"
#include "icd/vulkan/loader_handle.hpp"

#include <cstdint>

using av::Status;
using av::icd::DescriptorTemplateTable;
using av::icd::GuestHandleTranslation;
using av::icd::HandleBits;
using av::icd::Connection;
using av::icd::HandleRegistry;
using av::icd::LoaderHandle;
using av::vkproto::HandleKind;

namespace {

int connection_token = 0;

constexpr std::uint64_t instance_host = 0x7001u;
constexpr std::uint64_t device_host = 0x7002u;

class Rig {
public:
    Rig() : lock_(trace_, 'T'), registry_(platform_), templates_(platform_, lock_), translation_(lock_, registry_, templates_) {}

    LoaderHandle *wrap(LoaderHandle::Kind kind, std::uint64_t host, LoaderHandle *parent)
    {
        LoaderHandle *made = nullptr;
        CHECK_STATUS(registry_.wrap(kind, host, reinterpret_cast<Connection *>(&connection_token), parent, made), Status::ok);
        return made;
    }

    av::RecordingThreadLock &lock() { return lock_; }
    HandleRegistry          &registry() { return registry_; }
    DescriptorTemplateTable &templates() { return templates_; }
    GuestHandleTranslation  &translation() { return translation_; }

private:
    av::MockPlatform        platform_;
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    HandleRegistry          registry_;
    DescriptorTemplateTable templates_;
    GuestHandleTranslation  translation_;
};

std::uint64_t as_number(const LoaderHandle *handle) { return reinterpret_cast<std::uintptr_t>(handle); }

}

TEST(guest_handle_translation, a_dispatchable_handle_crosses_the_wire_as_the_hosts_own_number)
{ Rig           rig;
  LoaderHandle *instance = rig.wrap(LoaderHandle::Kind::instance, instance_host, nullptr);
  LoaderHandle *device = rig.wrap(LoaderHandle::Kind::device, device_host, instance);
  const std::uint32_t before = rig.lock().acquisitions();
  CHECK_EQ(rig.translation().wire_for(HandleKind::instance, as_number(instance)), instance_host);
  CHECK(rig.lock().acquisitions() > before);
  CHECK(!rig.lock().held());
  CHECK_EQ(rig.translation().wire_for(HandleKind::device, as_number(device)), device_host); }

TEST(guest_handle_translation, a_handle_of_another_kind_or_one_never_made_crosses_as_nothing)
{ Rig           rig;
  LoaderHandle *instance = rig.wrap(LoaderHandle::Kind::instance, instance_host, nullptr);
  CHECK_EQ(rig.translation().wire_for(HandleKind::device, as_number(instance)), 0u);
  CHECK_EQ(rig.translation().wire_for(HandleKind::queue, as_number(instance)), 0u);
  CHECK_EQ(rig.translation().wire_for(HandleKind::instance, 0x1234u), 0u);
  CHECK_EQ(rig.translation().wire_for(HandleKind::instance, 0u), 0u); }

TEST(guest_handle_translation, a_handle_the_host_names_itself_crosses_unchanged_and_is_taken_as_it_came)
{ Rig rig;
  CHECK_EQ(rig.translation().wire_for(HandleKind::fence, 0x4242u), 0x4242u);
  CHECK_EQ(rig.translation().wire_for(HandleKind::semaphore, 0u), 0u);
  std::uint64_t handle = 0u;
  CHECK(rig.translation().handle_for(HandleKind::fence, 0x99u, handle));
  CHECK_EQ(handle, 0x99u);
  CHECK(rig.translation().handle_for(HandleKind::device, 0u, handle));
  CHECK_EQ(handle, 0u); }

TEST(guest_handle_translation, a_templates_length_is_the_one_its_table_recorded)
{ Rig rig;
  VkDescriptorUpdateTemplateEntry entry = {};
  entry.descriptorCount = 2u;
  entry.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  entry.offset = 8u;
  entry.stride = sizeof(VkDescriptorImageInfo);
  VkDescriptorUpdateTemplateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
  info.descriptorUpdateEntryCount = 1u;
  info.pDescriptorUpdateEntries = &entry;
  const auto made = reinterpret_cast<VkDescriptorUpdateTemplate>(0x31u);
  CHECK_EQ(rig.templates().record(reinterpret_cast<VkDevice>(0x8010u), made, &info), VK_SUCCESS);
  CHECK_EQ(rig.translation().template_bytes(made), 8u + 2u * sizeof(VkDescriptorImageInfo));
  CHECK_EQ(rig.translation().template_bytes(reinterpret_cast<VkDescriptorUpdateTemplate>(0x32u)), 0u); }
