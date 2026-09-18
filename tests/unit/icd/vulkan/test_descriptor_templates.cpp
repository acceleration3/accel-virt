/* Descriptor update templates through the ICD (feature 047,
 * contracts/descriptor_templates.md). Nothing in
 * vkUpdateDescriptorSetWithTemplate says how long its blob is, so the ICD
 * remembers what each template reads when it is created, and the generated
 * serializer asks for that length while it writes the command. A template this
 * process did not make sizes nothing, which is what keeps a blob of bytes from
 * nowhere off the wire. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/descriptor_template_table.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

namespace av::icd {

/* The state no create can leave the table in. */
struct DescriptorTemplateTableProbe {
    static void a_slot_of_no_device(DescriptorTemplateTable &table) { table.slots_[0].device_ = VK_NULL_HANDLE; }
    static void the_same_template_twice(DescriptorTemplateTable &table)
    {
        table.slots_[1] = table.slots_[0];
    }
    static void a_device_in_an_empty_slot(DescriptorTemplateTable &table)
    {
        for (DescriptorTemplateSlot &slot : table.slots_)
            if (slot.template_ == VK_NULL_HANDLE) {
                slot.device_ = reinterpret_cast<VkDevice>(0x7010u);
                return;
            }
    }
    static void bytes_in_an_empty_slot(DescriptorTemplateTable &table)
    {
        for (DescriptorTemplateSlot &slot : table.slots_)
            if (slot.template_ == VK_NULL_HANDLE) {
                slot.bytes_ = 8u;
                return;
            }
    }
};

}  // namespace av::icd

using av::CommandHeader;
using av::IcdRig;
using av::MockKmt;
using av::ScriptedHost;
using av::WireReader;
using av::icd::DescriptorTemplateTable;
using av::icd::DescriptorTemplateTableProbe;
using namespace av::vkproto;

namespace {

using CreateTemplate = VkResult (*)(VkDevice, const VkDescriptorUpdateTemplateCreateInfo *,
                                    const VkAllocationCallbacks *, VkDescriptorUpdateTemplate *);
using DestroyTemplate = void (*)(VkDevice, VkDescriptorUpdateTemplate, const VkAllocationCallbacks *);
using UpdateWithTemplate = void (*)(VkDevice, VkDescriptorSet, VkDescriptorUpdateTemplate, const void *);

constexpr std::uint64_t image_bytes = sizeof(VkDescriptorImageInfo);

/* A device on the scripted host, and the three entry points a template needs. */
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
    VkDevice device() const { return device_; }
    /* The device destroyed while the rig lives, so a test can see what its
     * destruction took with it. */
    void     destroy_device()
    {
        rig_.entry<void (*)(VkDevice, const VkAllocationCallbacks *)>(instance_, "vkDestroyDevice")(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    /* A template of `count` image descriptors at `offset`, as the host answers
     * `handle` for it. */
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

    /* The blob length the last command carried, read back off the wire. */
    std::uint64_t blob_on_the_wire()
    {
        const std::uint32_t sent = rig_.kmt().commands() - 1u;
        CHECK_EQ(rig_.kmt().command_id(sent), static_cast<std::uint32_t>(kCmd_vkUpdateDescriptorSetWithTemplate));
        const std::uint8_t *bytes = rig_.kmt().command_bytes(sent);
        WireReader          body(bytes + CommandHeader::encoded_size,
                                 MockKmt::journal_bytes - CommandHeader::encoded_size);
        CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);   /* the device */
        static_cast<void>(body.read_u64());                   /* the descriptor set */
        static_cast<void>(body.read_u64());                   /* the template */
        std::uint64_t carried = 0u;
        CHECK(body.read_varint(carried));
        return carried;
    }

private:
    IcdRig     rig_;
    VkInstance instance_;
    VkDevice   device_;
};

}  // namespace

TEST(icd_descriptor_templates, a_template_is_remembered_and_sizes_the_blob_it_carries)
{ TemplateRig rig;
  CHECK(rig.rig().root().templates().invariants_hold());
  CHECK_EQ(rig.rig().root().templates().live(), 0u);
  const VkDescriptorUpdateTemplate made = rig.make(4u, 16u, 0x9100u);
  CHECK(made != VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().root().templates().live(), 1u);
  CHECK_EQ(rig.rig().root().templates().bytes_of(made), 16u + 4u * image_bytes);
  CHECK(rig.rig().root().templates().invariants_hold());

  /* An update through it carries exactly those bytes, and the blob the
   * application gave is what lands on the wire. */
  std::uint8_t blob[16u + 4u * image_bytes] = {};
  for (std::uint32_t at = 0u; at < sizeof blob; at++) blob[at] = static_cast<std::uint8_t>(at + 1u);
  rig.update_entry()(rig.device(), reinterpret_cast<VkDescriptorSet>(0x42u), made, blob);
  CHECK_EQ(rig.blob_on_the_wire(), 16u + 4u * image_bytes);

  /* Destroyed, it sizes nothing: an update through it would send no bytes. */
  rig.destroy_entry()(rig.device(), made, nullptr);
  CHECK_EQ(rig.rig().root().templates().live(), 0u);
  CHECK_EQ(rig.rig().root().templates().bytes_of(made), 0u);
  rig.update_entry()(rig.device(), reinterpret_cast<VkDescriptorSet>(0x42u), made, blob);
  CHECK_EQ(rig.blob_on_the_wire(), 0u);
  CHECK(rig.rig().root().templates().invariants_hold()); }

/* A null blob carries nothing whatever the template says, and a template of no
 * entries reads nothing whatever the blob is. */
TEST(icd_descriptor_templates, nothing_to_carry_carries_nothing)
{ TemplateRig rig;
  const VkDescriptorUpdateTemplate made = rig.make(2u, 0u, 0x9200u);
  CHECK(made != VK_NULL_HANDLE);
  rig.update_entry()(rig.device(), reinterpret_cast<VkDescriptorSet>(0x42u), made, nullptr);
  CHECK_EQ(rig.blob_on_the_wire(), 0u);
  const VkDescriptorUpdateTemplate empty = rig.make(0u, 0u, 0x9201u);
  CHECK(empty != VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().root().templates().bytes_of(empty), 0u);
  std::uint8_t blob[8] = {};
  rig.update_entry()(rig.device(), reinterpret_cast<VkDescriptorSet>(0x42u), empty, blob);
  CHECK_EQ(rig.blob_on_the_wire(), 0u); }

/* A create whose entries cannot be sized is refused before the host is asked,
 * and a create the host refused is not remembered. */
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
  CHECK_EQ(rig.rig().kmt().commands(), quiet);   /* the host was never asked */
  CHECK_EQ(rig.rig().root().templates().live(), 0u);
  CHECK_EQ(rig.rig().log().holding("cannot size"), 1u);

  /* A descriptor type a template may not write is the same refusal. */
  overflowing.offset = 0u;
  overflowing.descriptorType = VK_DESCRIPTOR_TYPE_MUTABLE_EXT;
  CHECK_EQ(rig.create_entry()(rig.device(), &info, nullptr, &made), VK_ERROR_INITIALIZATION_FAILED);

  /* A host that refuses the create leaves nothing remembered. */
  overflowing.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  rig.rig().host().vk_handle(VK_ERROR_OUT_OF_DEVICE_MEMORY, 0u);
  CHECK(rig.create_entry()(rig.device(), &info, nullptr, &made) != VK_SUCCESS);
  CHECK_EQ(rig.rig().root().templates().live(), 0u);
  /* A create with no arguments at all is refused rather than dereferenced. */
  CHECK_EQ(rig.create_entry()(rig.device(), nullptr, nullptr, &made), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(rig.create_entry()(rig.device(), &info, nullptr, nullptr), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(rig.rig().root().templates().invariants_hold()); }

/* Every template of a device goes when the device does, and a handle a driver
 * hands out again is the template it is now. */
TEST(icd_descriptor_templates, a_device_takes_its_templates_with_it)
{ TemplateRig rig;
  CHECK(rig.make(2u, 0u, 0x9300u) != VK_NULL_HANDLE);
  CHECK(rig.make(1u, 64u, 0x9301u) != VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().root().templates().live(), 2u);
  /* The same handle again: the second template is what it means now. */
  const VkDescriptorUpdateTemplate again = rig.make(8u, 0u, 0x9300u);
  CHECK(again != VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().root().templates().live(), 2u);
  CHECK_EQ(rig.rig().root().templates().bytes_of(again), 8u * image_bytes);
  /* A handle nothing made sizes nothing, and forgetting one is no-one's error. */
  CHECK_EQ(rig.rig().root().templates().bytes_of(reinterpret_cast<VkDescriptorUpdateTemplate>(0x1234u)), 0u);
  rig.rig().root().templates().forget(reinterpret_cast<VkDescriptorUpdateTemplate>(0x1234u));
  rig.rig().root().templates().forget(VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().root().templates().live(), 2u);
  /* The device destroyed takes both with it, which is what the table's
   * forget_device is for. */
  rig.destroy_device();
  CHECK_EQ(rig.rig().root().templates().live(), 0u);
  CHECK(rig.rig().root().templates().invariants_hold()); }

/* The ICD's table full: the create is refused and the template the host made is
 * destroyed again, because a template the ICD cannot size is one no update
 * through it could carry its descriptors. */
TEST(icd_descriptor_templates, a_template_the_table_has_no_room_for_is_given_back)
{ TemplateRig rig;
  /* Filled through the table rather than through 512 creates: the scripted host
   * holds far fewer objects than the table holds templates, and what is being
   * judged here is the root's answer when the table is full. */
  VkDescriptorUpdateTemplateEntry            entry = {};
  entry.descriptorCount = 1u;
  entry.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  entry.stride = image_bytes;
  VkDescriptorUpdateTemplateCreateInfo filling = {};
  filling.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
  filling.descriptorUpdateEntryCount = 1u;
  filling.pDescriptorUpdateEntries = &entry;
  for (std::uint32_t index = 0u; index < av::icd::DescriptorTemplateTable::max_templates; index++)
      CHECK_EQ(rig.rig().root().templates().record(
                   rig.device(), reinterpret_cast<VkDescriptorUpdateTemplate>(0xA000u + index), &filling),
               VK_SUCCESS);
  CHECK_EQ(rig.rig().root().templates().live(), av::icd::DescriptorTemplateTable::max_templates);
  const std::uint32_t before = rig.rig().kmt().commands();
  CHECK(rig.make(1u, 0u, 0xB000u) == VK_NULL_HANDLE);
  /* Two commands: the create the host answered, and the destroy that gave it
   * back. */
  CHECK_EQ(rig.rig().kmt().commands(), before + 2u);
  CHECK_EQ(rig.rig().kmt().command_id(rig.rig().kmt().commands() - 1u),
           static_cast<std::uint32_t>(kCmd_vkDestroyDescriptorUpdateTemplate));
  CHECK_EQ(rig.rig().root().templates().live(), av::icd::DescriptorTemplateTable::max_templates);
  CHECK(rig.rig().root().templates().invariants_hold()); }

/* Every clause of the invariant refuses the state it names. One rig at a time:
 * the ICD's root is the process's, so two would unbind each other. */
TEST(icd_descriptor_templates, an_invariant_refuses_each_state_it_names)
{ { TemplateRig rig;
    CHECK(rig.make(2u, 0u, 0x9400u) != VK_NULL_HANDLE);
    DescriptorTemplateTable &table = rig.rig().root().templates();
    CHECK(table.invariants_hold());
    DescriptorTemplateTableProbe::the_same_template_twice(table);
    CHECK(!table.invariants_hold()); }
  { TemplateRig rig;
    CHECK(rig.make(1u, 0u, 0x9401u) != VK_NULL_HANDLE);
    DescriptorTemplateTableProbe::a_slot_of_no_device(rig.rig().root().templates());
    CHECK(!rig.rig().root().templates().invariants_hold()); }
  { TemplateRig rig;
    DescriptorTemplateTableProbe::bytes_in_an_empty_slot(rig.rig().root().templates());
    CHECK(!rig.rig().root().templates().invariants_hold()); }
  { TemplateRig rig;
    DescriptorTemplateTableProbe::a_device_in_an_empty_slot(rig.rig().root().templates());
    CHECK(!rig.rig().root().templates().invariants_hold()); } }
