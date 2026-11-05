#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/descriptor_template_table.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

namespace av::icd {
struct IcdRootProbe {
    static DescriptorTemplateTable &templates(IcdRoot &root) { return root.templates_; }
};

struct DescriptorTemplateTableProbe {
    static void a_slot_of_no_device(DescriptorTemplateTable &table)
    {
        table.slots_.at(table.slots_.next_live(0u))->device_ = VK_NULL_HANDLE;
    }
    static void a_slot_under_another_templates_key(DescriptorTemplateTable &table)
    {
        const uint32_t first = table.slots_.next_live(0u);
        table.slots_.at(table.slots_.next_live(first + 1u))->template_ = table.slots_.at(first)->template_;
    }
};

}

using av::CommandHeader;
using av::IcdRig;
using av::MockKmt;
using av::ScriptedHost;
using av::WireReader;
using av::icd::DescriptorTemplateTable;
using av::icd::DescriptorTemplateTableProbe;
using av::icd::IcdRootProbe;
using namespace av::vkproto;

namespace {

using CreateTemplate = VkResult (*)(VkDevice, const VkDescriptorUpdateTemplateCreateInfo *,
                                    const VkAllocationCallbacks *, VkDescriptorUpdateTemplate *);
using DestroyTemplate = void (*)(VkDevice, VkDescriptorUpdateTemplate, const VkAllocationCallbacks *);
using UpdateWithTemplate = void (*)(VkDevice, VkDescriptorSet, VkDescriptorUpdateTemplate, const void *);

constexpr std::uint64_t image_bytes = sizeof(VkDescriptorImageInfo);

class TemplateRig {
public:
    TemplateRig() : instance_(rig_.create_instance()), device_(VK_NULL_HANDLE)
    {
        const VkPhysicalDevice physical_device = rig_.physical_device(instance_);
        rig_.host().vk_handle(0, ScriptedHost::vk_device);
        const auto create = rig_.entry<VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *,
                                                    const VkAllocationCallbacks *, VkDevice *)>(instance_,
                                                                                                "vkCreateDevice");
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        CHECK_EQ(create(physical_device, &info, nullptr, &device_), VK_SUCCESS);
    }
    ~TemplateRig()
    {
        if (device_ != VK_NULL_HANDLE)
            rig_.entry<void (*)(VkDevice, const VkAllocationCallbacks *)>(instance_, "vkDestroyDevice")(device_,
                                                                                                        nullptr);
        rig_.destroy_instance(instance_);
    }
    TemplateRig(const TemplateRig &) = delete;
    TemplateRig &operator=(const TemplateRig &) = delete;

    IcdRig  &rig() { return rig_; }
    DescriptorTemplateTable &templates() { return IcdRootProbe::templates(rig_.root()); }
    VkDevice device() const { return device_; }
    void     destroy_device()
    {
        rig_.entry<void (*)(VkDevice, const VkAllocationCallbacks *)>(instance_, "vkDestroyDevice")(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    VkDescriptorUpdateTemplate make(std::uint32_t count, std::size_t offset, std::uint64_t handle)
    {
        VkDescriptorUpdateTemplateEntry entry = {};
        entry.descriptorCount = count;
        entry.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        entry.offset = offset;
        entry.stride = image_bytes;
        VkDescriptorUpdateTemplateCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
        info.descriptorUpdateEntryCount = count != 0u ? 1u : 0u;
        info.pDescriptorUpdateEntries = &entry;
        rig_.host().vk_handle(0, handle);
        VkDescriptorUpdateTemplate made = VK_NULL_HANDLE;
        const VkResult result = create_entry()(device_, &info, nullptr, &made);
        return result == VK_SUCCESS ? made : VK_NULL_HANDLE;
    }

    CreateTemplate      create_entry()
    { return rig_.entry<CreateTemplate>(instance_, "vkCreateDescriptorUpdateTemplate"); }
    DestroyTemplate     destroy_entry()
    { return rig_.entry<DestroyTemplate>(instance_, "vkDestroyDescriptorUpdateTemplate"); }
    UpdateWithTemplate  update_entry()
    { return rig_.entry<UpdateWithTemplate>(instance_, "vkUpdateDescriptorSetWithTemplate"); }

    std::uint64_t blob_on_the_wire()
    {
        const std::uint32_t sent = rig_.kmt().commands() - 1u;
        CHECK_EQ(rig_.kmt().command_id(sent), static_cast<std::uint32_t>(kCmd_vkUpdateDescriptorSetWithTemplate));
        const std::uint8_t *bytes = rig_.kmt().command_bytes(sent);
        WireReader          body(bytes + CommandHeader::encoded_size,
                                 MockKmt::journal_bytes - CommandHeader::encoded_size);
        CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);
        static_cast<void>(body.read_u64());
        static_cast<void>(body.read_u64());
        std::uint64_t carried = 0u;
        CHECK(body.read_varint(carried));
        return carried;
    }

private:
    IcdRig     rig_;
    VkInstance instance_;
    VkDevice   device_;
};

}

TEST(icd_descriptor_templates, a_template_is_remembered_and_sizes_the_blob_it_carries)
{ TemplateRig rig;
  CHECK(rig.templates().invariants_hold());
  CHECK_EQ(rig.templates().live(), 0u);
  const VkDescriptorUpdateTemplate made = rig.make(4u, 16u, 0x9100u);
  CHECK(made != VK_NULL_HANDLE);
  CHECK_EQ(rig.templates().live(), 1u);
  CHECK_EQ(rig.templates().bytes_of(made), 16u + 4u * image_bytes);
  CHECK(rig.templates().invariants_hold());

  std::uint8_t blob[16u + 4u * image_bytes] = {};
  for (std::uint32_t at = 0u; at < sizeof blob; at++) blob[at] = static_cast<std::uint8_t>(at + 1u);
  rig.update_entry()(rig.device(), reinterpret_cast<VkDescriptorSet>(0x42u), made, blob);
  CHECK_EQ(rig.blob_on_the_wire(), 16u + 4u * image_bytes);

  rig.destroy_entry()(rig.device(), made, nullptr);
  CHECK_EQ(rig.templates().live(), 0u);
  CHECK_EQ(rig.templates().bytes_of(made), 0u);
  rig.update_entry()(rig.device(), reinterpret_cast<VkDescriptorSet>(0x42u), made, blob);
  CHECK_EQ(rig.blob_on_the_wire(), 0u);
  CHECK(rig.templates().invariants_hold()); }

TEST(icd_descriptor_templates, a_maintenance6_template_push_carries_the_template_s_blob)
{ TemplateRig rig;
  const VkDescriptorUpdateTemplate made = rig.make(4u, 16u, 0x9300u);
  CHECK(made != VK_NULL_HANDLE);
  std::uint8_t blob[16u + 4u * image_bytes] = {};
  for (std::uint32_t at = 0u; at < sizeof blob; at++) blob[at] = static_cast<std::uint8_t>(0xA0u + at);
  VkPushDescriptorSetWithTemplateInfo push = {};
  push.sType = VK_STRUCTURE_TYPE_PUSH_DESCRIPTOR_SET_WITH_TEMPLATE_INFO;
  push.descriptorUpdateTemplate = made;
  push.layout = reinterpret_cast<VkPipelineLayout>(0x77u);
  push.set = 3u;
  const bool cases[2] = {true, false};
  for (const bool carried : cases) {
      push.pData = carried ? blob : nullptr;
      std::uint8_t   wire[1024] = {};
      av::WireWriter writer(wire, sizeof wire);
      serialize_VkPushDescriptorSetWithTemplateInfo(writer, rig.rig().root().wire_handles(), push);
      CHECK_STATUS(writer.status(), av::Status::ok);
      WireReader    reader(wire, writer.offset());
      std::uint8_t  block[1024] = {};
      av::WireArena arena(block, sizeof block);
      VkPushDescriptorSetWithTemplateInfo decoded = {};
      if (!carried) {
          CHECK(!deserialize_VkPushDescriptorSetWithTemplateInfo(reader, rig.rig().root().wire_handles(), &decoded,
                                                                 arena));
          continue;
      }
      CHECK(deserialize_VkPushDescriptorSetWithTemplateInfo(reader, rig.rig().root().wire_handles(), &decoded,
                                                            arena));
      CHECK_EQ(reader.remaining(), 0u);
      CHECK_EQ(decoded.set, 3u);
      CHECK(decoded.layout == push.layout);
      CHECK(decoded.descriptorUpdateTemplate == made);
      CHECK(decoded.pData != nullptr);
      CHECK_EQ(std::memcmp(decoded.pData, blob, sizeof blob), 0);
  } }

TEST(icd_descriptor_templates, nothing_to_carry_carries_nothing)
{ TemplateRig rig;
  const VkDescriptorUpdateTemplate made = rig.make(2u, 0u, 0x9200u);
  CHECK(made != VK_NULL_HANDLE);
  rig.update_entry()(rig.device(), reinterpret_cast<VkDescriptorSet>(0x42u), made, nullptr);
  CHECK_EQ(rig.blob_on_the_wire(), 0u);
  const VkDescriptorUpdateTemplate empty = rig.make(0u, 0u, 0x9201u);
  CHECK(empty != VK_NULL_HANDLE);
  CHECK_EQ(rig.templates().bytes_of(empty), 0u);
  std::uint8_t blob[8] = {};
  rig.update_entry()(rig.device(), reinterpret_cast<VkDescriptorSet>(0x42u), empty, blob);
  CHECK_EQ(rig.blob_on_the_wire(), 0u); }

TEST(icd_descriptor_templates, a_template_the_icd_cannot_size_is_refused)
{ TemplateRig rig;
  const std::uint32_t quiet = rig.rig().kmt().commands();
  VkDescriptorUpdateTemplateEntry overflowing = {};
  overflowing.descriptorCount = 2u;
  overflowing.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  overflowing.offset = ~static_cast<std::size_t>(0);
  overflowing.stride = image_bytes;
  VkDescriptorUpdateTemplateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
  info.descriptorUpdateEntryCount = 1u;
  info.pDescriptorUpdateEntries = &overflowing;
  VkDescriptorUpdateTemplate made = VK_NULL_HANDLE;
  CHECK_EQ(rig.create_entry()(rig.device(), &info, nullptr, &made), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(made == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), quiet);
  CHECK_EQ(rig.templates().live(), 0u);
  CHECK_EQ(rig.rig().log().holding("cannot size"), 1u);

  overflowing.offset = 0u;
  overflowing.descriptorType = VK_DESCRIPTOR_TYPE_MUTABLE_EXT;
  CHECK_EQ(rig.create_entry()(rig.device(), &info, nullptr, &made), VK_ERROR_INITIALIZATION_FAILED);

  overflowing.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  rig.rig().host().vk_handle(VK_ERROR_OUT_OF_DEVICE_MEMORY, 0u);
  CHECK(rig.create_entry()(rig.device(), &info, nullptr, &made) != VK_SUCCESS);
  CHECK_EQ(rig.templates().live(), 0u);
  CHECK_EQ(rig.create_entry()(rig.device(), nullptr, nullptr, &made), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(rig.create_entry()(rig.device(), &info, nullptr, nullptr), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(rig.templates().invariants_hold()); }

TEST(icd_descriptor_templates, a_device_takes_its_templates_with_it)
{ TemplateRig rig;
  CHECK(rig.make(2u, 0u, 0x9300u) != VK_NULL_HANDLE);
  CHECK(rig.make(1u, 64u, 0x9301u) != VK_NULL_HANDLE);
  CHECK_EQ(rig.templates().live(), 2u);
  const VkDescriptorUpdateTemplate again = rig.make(8u, 0u, 0x9300u);
  CHECK(again != VK_NULL_HANDLE);
  CHECK_EQ(rig.templates().live(), 2u);
  CHECK_EQ(rig.templates().bytes_of(again), 8u * image_bytes);
  CHECK_EQ(rig.templates().bytes_of(reinterpret_cast<VkDescriptorUpdateTemplate>(0x1234u)), 0u);
  rig.templates().forget(reinterpret_cast<VkDescriptorUpdateTemplate>(0x1234u));
  rig.templates().forget(VK_NULL_HANDLE);
  CHECK_EQ(rig.templates().live(), 2u);
  rig.destroy_device();
  CHECK_EQ(rig.templates().live(), 0u);
  CHECK(rig.templates().invariants_hold()); }

TEST(icd_descriptor_templates, a_template_the_table_cannot_grow_for_is_given_back)
{ TemplateRig rig;
  VkDescriptorUpdateTemplateEntry            entry = {};
  entry.descriptorCount = 1u;
  entry.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  entry.stride = image_bytes;
  VkDescriptorUpdateTemplateCreateInfo filling = {};
  filling.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
  filling.descriptorUpdateEntryCount = 1u;
  filling.pDescriptorUpdateEntries = &entry;
  std::uint32_t held = 0u;
  VkResult      result = rig.templates().record(rig.device(), reinterpret_cast<VkDescriptorUpdateTemplate>(0xA000u),
                                                &filling);
  CHECK_EQ(result, VK_SUCCESS);
  held++;
  while (result == VK_SUCCESS && held < 4096u) {
      rig.rig().platform().fail_allocation_at(1u);
      result = rig.templates().record(rig.device(), reinterpret_cast<VkDescriptorUpdateTemplate>(0xA000u + held),
                                      &filling);
      if (result == VK_SUCCESS) held++;
  }
  CHECK_EQ(result, VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(held > 0u);
  CHECK_EQ(rig.templates().live(), held);
  const std::uint32_t before = rig.rig().kmt().commands();
  rig.rig().platform().fail_allocation_at(0u);
  rig.rig().platform().fail_allocation_at(1u);
  CHECK(rig.make(1u, 0u, 0xB000u) == VK_NULL_HANDLE);
  rig.rig().platform().fail_allocation_at(0u);
  CHECK_EQ(rig.rig().kmt().commands(), before + 2u);
  CHECK_EQ(rig.rig().kmt().command_id(rig.rig().kmt().commands() - 1u),
           static_cast<std::uint32_t>(kCmd_vkDestroyDescriptorUpdateTemplate));
  CHECK_EQ(rig.templates().live(), held);
  CHECK(rig.templates().invariants_hold());
  CHECK(rig.make(1u, 0u, 0xB001u) != VK_NULL_HANDLE);
  CHECK_EQ(rig.templates().live(), held + 1u);
  CHECK(rig.templates().invariants_hold()); }

TEST(icd_descriptor_templates, an_invariant_refuses_each_state_it_names)
{ { TemplateRig rig;
    CHECK(rig.make(2u, 0u, 0x9400u) != VK_NULL_HANDLE);
    CHECK(rig.make(2u, 0u, 0x9401u) != VK_NULL_HANDLE);
    DescriptorTemplateTable &table = rig.templates();
    CHECK(table.invariants_hold());
    DescriptorTemplateTableProbe::a_slot_under_another_templates_key(table);
    CHECK(!table.invariants_hold()); }
  { TemplateRig rig;
    CHECK(rig.make(1u, 0u, 0x9402u) != VK_NULL_HANDLE);
    DescriptorTemplateTableProbe::a_slot_of_no_device(rig.templates());
    CHECK(!rig.templates().invariants_hold()); } }
