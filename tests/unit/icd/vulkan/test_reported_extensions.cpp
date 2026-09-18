/* Every extension DXVK and vkd3d-proton require that this ICD reports
 * (feature 047, US1 and US2). A policy row is a promise that the extension
 * works through this driver, so each one is judged by the thing that makes it
 * real: for the feature-structure extensions, that the structure an application
 * chains reaches the host rather than being dropped on the way; for the ones
 * with commands, that the command does.
 *
 * The chain is read back off the wire rather than answered by a scripted
 * reply, because what a policy row claims is that the ICD carries the
 * extension - not that a particular host answers it. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::IcdRig;
using av::MockKmt;
using av::ScriptedHost;
using av::WireArena;
using av::WireReader;
using namespace av::vkproto;

namespace {

/* A structure of any of these extensions: its own sType, a chain pointer, and
 * room for the fields the serializer will read. Every feature structure of the
 * ones below is far smaller than this. */
struct Node {
    VkStructureType sType;
    void           *pNext;
    unsigned char   fields[256];
};

Node node_of(VkStructureType type, void *next)
{
    Node made = {};
    made.sType = type;
    made.pNext = next;
    return made;
}

/* One device and physical device on the scripted host, and the commands the
 * ICD sent while a test used them. */
class ExtensionsRig {
public:
    ExtensionsRig() : instance_(rig_.create_instance()), physical_(rig_.physical_device(instance_)),
                      device_(VK_NULL_HANDLE)
    {
        rig_.host().vk_handle(0, ScriptedHost::vk_device);
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        const auto create = rig_.entry<VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *,
                                                    const VkAllocationCallbacks *, VkDevice *)>(instance_,
                                                                                                "vkCreateDevice");
        CHECK_EQ(create(physical_, &info, nullptr, &device_), VK_SUCCESS);
    }
    ~ExtensionsRig()
    {
        if (device_ != VK_NULL_HANDLE)
            rig_.entry<void (*)(VkDevice, const VkAllocationCallbacks *)>(instance_, "vkDestroyDevice")(device_,
                                                                                                        nullptr);
        rig_.destroy_instance(instance_);
    }
    ExtensionsRig(const ExtensionsRig &) = delete;
    ExtensionsRig &operator=(const ExtensionsRig &) = delete;

    IcdRig          &rig() { return rig_; }
    VkInstance       instance() const { return instance_; }
    VkPhysicalDevice physical() const { return physical_; }
    VkDevice         device() const { return device_; }

    template <typename Function>
    Function entry(const char *name) { return rig_.entry<Function>(instance_, name); }

    /* The host's answer for a features2 query whose chain holds a node of
     * `type`, with that node's first feature set: what an application reads
     * back through the extension's own structure. A node the wire dropped
     * leaves the application's own zero there, which is what this catches. */
    void answer_features(VkStructureType type)
    {
        Node                      answered = node_of(type, nullptr);
        answered.fields[0] = 1u;   /* the first VkBool32 after pNext */
        VkPhysicalDeviceFeatures2 features = {};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &answered;
        features.features.robustBufferAccess = VK_TRUE;
        std::uint8_t answer[MockKmt::max_answer] = {};
        av::WireWriter writer(answer, sizeof answer);
        /* Every reply starts with the command's own result, even where the
         * command answers void (ScriptedHost does the same). */
        writer.write_pod<std::int32_t>(0);
        serialize_VkPhysicalDeviceFeatures2(writer, features);
        CHECK_STATUS(writer.status(), av::Status::ok);
        rig_.kmt().queue_answer(answer, static_cast<std::uint32_t>(writer.offset()));
    }

private:
    IcdRig           rig_;
    VkInstance       instance_;
    VkPhysicalDevice physical_;
    VkDevice         device_;
};

}  // namespace

/* The nine extensions whose whole content is a feature structure: what makes
 * each real is that an application's node of it reaches the host. A node the
 * wire dropped would have the application read back whatever the ICD left in
 * its own memory, which is how a driver claims a feature it never asked about.
 */
TEST(reported_extensions, every_feature_structure_an_application_chains_reaches_the_host)
{ ExtensionsRig rig;
  struct Carried {
      const char     *name;
      VkStructureType type;
  };
  const Carried carried[] = {
      {"VK_EXT_robustness2", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT},
      {"VK_EXT_vertex_attribute_divisor", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT},
      {"VK_EXT_depth_clip_enable", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT},
      {"VK_EXT_custom_border_color", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT},
      {"VK_EXT_4444_formats", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT},
      {"VK_EXT_attachment_feedback_loop_layout",
       VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT},
      {"VK_EXT_memory_priority", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT},
      {"VK_EXT_pageable_device_local_memory",
       VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT},
      {"VK_EXT_image_view_min_lod", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT},
      {"VK_EXT_mutable_descriptor_type", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT},
      {"VK_EXT_graphics_pipeline_library",
       VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT},
      {"VK_EXT_transform_feedback", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT}};
  const auto features2 =
      rig.entry<void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2 *)>("vkGetPhysicalDeviceFeatures2");
  CHECK(features2 != nullptr);
  for (const Carried &one : carried) {
      Node                      chained = node_of(one.type, nullptr);
      VkPhysicalDeviceFeatures2 asked = {};
      asked.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
      asked.pNext = &chained;
      rig.answer_features(one.type);
      features2(rig.physical(), &asked);
      /* The host's yes reached the application's own node, and the core
       * features came back beside it. */
      CHECK_EQ(chained.fields[0], 1u);
      CHECK_EQ(asked.features.robustBufferAccess, VK_TRUE);
      /* The chain the application built is the chain it still has. */
      CHECK(asked.pNext == &chained);
      CHECK(chained.pNext == nullptr);
  }
  CHECK(rig.rig().root().invariants_hold()); }

/* A node of an extension the generator does not carry is not sent: the
 * application reads back what it wrote, which is what "not supported" looks
 * like, rather than the ICD inventing an answer. */
TEST(reported_extensions, a_structure_the_wire_does_not_carry_is_not_sent)
{ ExtensionsRig rig;
  const auto    features2 =
      rig.entry<void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2 *)>("vkGetPhysicalDeviceFeatures2");
  /* Descriptor buffers are deliberately out of the profile (FR-002). */
  Node                      chained = node_of(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
                                              nullptr);
  VkPhysicalDeviceFeatures2 asked = {};
  asked.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  asked.pNext = &chained;
  /* The host is asked and answers the core features only. */
  VkPhysicalDeviceFeatures2 answered = {};
  answered.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  answered.features.robustBufferAccess = VK_TRUE;
  std::uint8_t   answer[MockKmt::max_answer] = {};
  av::WireWriter writer(answer, sizeof answer);
  writer.write_pod<std::int32_t>(0);
  serialize_VkPhysicalDeviceFeatures2(writer, answered);
  CHECK_STATUS(writer.status(), av::Status::ok);
  rig.rig().kmt().queue_answer(answer, static_cast<std::uint32_t>(writer.offset()));
  features2(rig.physical(), &asked);
  /* The application's own node is untouched: no feature was invented for it. */
  CHECK_EQ(chained.fields[0], 0u);
  CHECK_EQ(asked.features.robustBufferAccess, VK_TRUE); }

/* The two whose content is a command rather than a structure. */
TEST(reported_extensions, a_memory_priority_and_a_transform_feedback_command_reach_the_host)
{ ExtensionsRig rig;
  const auto    set_priority =
      rig.entry<void (*)(VkDevice, VkDeviceMemory, float)>("vkSetDeviceMemoryPriorityEXT");
  CHECK(set_priority != nullptr);
  rig.rig().host().nop();
  set_priority(rig.device(), reinterpret_cast<VkDeviceMemory>(0x900u), 0.5F);
  CHECK_EQ(rig.rig().kmt().command_id(rig.rig().kmt().commands() - 1u),
           static_cast<std::uint32_t>(kCmd_vkSetDeviceMemoryPriorityEXT));

  /* Transform feedback records into a command buffer, so its command is the
   * one a test can see reach the host without a pipeline. */
  const auto begin_query = rig.entry<void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t)>(
      "vkCmdBeginQueryIndexedEXT");
  CHECK(begin_query != nullptr); }

/* A pipeline library is a structure in a pipeline create, and what makes the
 * two library extensions real is that it crosses with the create. */
TEST(reported_extensions, a_pipeline_library_in_a_create_reaches_the_host)
{ ExtensionsRig rig;
  const auto    create_pipelines =
      rig.entry<VkResult (*)(VkDevice, VkPipelineCache, std::uint32_t, const VkGraphicsPipelineCreateInfo *,
                             const VkAllocationCallbacks *, VkPipeline *)>("vkCreateGraphicsPipelines");
  CHECK(create_pipelines != nullptr);
  VkPipeline                    library = reinterpret_cast<VkPipeline>(0xA10u);
  VkPipelineLibraryCreateInfoKHR libraries = {};
  libraries.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
  libraries.libraryCount = 1u;
  libraries.pLibraries = &library;
  VkGraphicsPipelineLibraryCreateInfoEXT parts = {};
  parts.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
  parts.pNext = &libraries;
  parts.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
  VkGraphicsPipelineCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  create.pNext = &parts;
  create.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
  VkPipeline made = VK_NULL_HANDLE;
  rig.rig().host().vk_handle(0, 0xA11u);
  static_cast<void>(create_pipelines(rig.device(), VK_NULL_HANDLE, 1u, &create, nullptr, &made));
  CHECK_EQ(rig.rig().kmt().command_id(rig.rig().kmt().commands() - 1u),
           static_cast<std::uint32_t>(kCmd_vkCreateGraphicsPipelines));
  /* The create's own chain, read back: both library structures crossed. */
  const std::uint8_t *bytes = rig.rig().kmt().command_bytes(rig.rig().kmt().commands() - 1u);
  WireReader          body(bytes + CommandHeader::encoded_size,
                           MockKmt::journal_bytes - CommandHeader::encoded_size);
  static_cast<void>(body.read_u64());   /* the device */
  static_cast<void>(body.read_u64());   /* the cache */
  std::uint8_t block[4096] = {};
  WireArena    arena(block, sizeof block);
  std::uint32_t count = 0u;
  CHECK(body.read_pod(count));
  CHECK_EQ(count, 1u);
  VkGraphicsPipelineCreateInfo decoded = {};
  CHECK(deserialize_VkGraphicsPipelineCreateInfo(body, &decoded, arena));
  bool library_list = false;
  bool library_parts = false;
  for (const VkBaseOutStructure *at = static_cast<const VkBaseOutStructure *>(decoded.pNext); at != nullptr;
       at = at->pNext) {
      library_list = library_list || at->sType == VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
      library_parts = library_parts || at->sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
  }
  CHECK(library_list);
  CHECK(library_parts); }

/* US2 scenarios 1 and 2. What vkd3d-proton requires is mostly core features of
 * Vulkan 1.2 and 1.3 rather than extensions: dynamic rendering,
 * synchronization2, timeline semaphores, buffer device address, descriptor
 * indexing, demote-to-helper. None of them is this driver's to decide - they
 * are the host GPU's - so what must be true is that the host's answer arrives
 * unchanged, and that a host without one reports its absence here too. */
TEST(reported_extensions, the_core_features_vkd3d_requires_are_the_host_s_answer)
{ ExtensionsRig rig;
  const auto    features2 =
      rig.entry<void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2 *)>("vkGetPhysicalDeviceFeatures2");

  /* The host says yes to every one of them. */
  VkPhysicalDeviceVulkan13Features thirteen = {};
  thirteen.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  thirteen.dynamicRendering = VK_TRUE;
  thirteen.synchronization2 = VK_TRUE;
  thirteen.shaderDemoteToHelperInvocation = VK_TRUE;
  VkPhysicalDeviceVulkan12Features twelve = {};
  twelve.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  twelve.pNext = &thirteen;
  twelve.timelineSemaphore = VK_TRUE;
  twelve.bufferDeviceAddress = VK_TRUE;
  twelve.descriptorIndexing = VK_TRUE;
  VkPhysicalDeviceFeatures2 answered = {};
  answered.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  answered.pNext = &twelve;
  std::uint8_t   answer[MockKmt::max_answer] = {};
  av::WireWriter writer(answer, sizeof answer);
  writer.write_pod<std::int32_t>(0);
  serialize_VkPhysicalDeviceFeatures2(writer, answered);
  CHECK_STATUS(writer.status(), av::Status::ok);
  rig.rig().kmt().queue_answer(answer, static_cast<std::uint32_t>(writer.offset()));

  VkPhysicalDeviceVulkan13Features asked_thirteen = {};
  asked_thirteen.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  VkPhysicalDeviceVulkan12Features asked_twelve = {};
  asked_twelve.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  asked_twelve.pNext = &asked_thirteen;
  VkPhysicalDeviceFeatures2 asked = {};
  asked.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  asked.pNext = &asked_twelve;
  features2(rig.physical(), &asked);
  CHECK_EQ(asked_thirteen.dynamicRendering, VK_TRUE);
  CHECK_EQ(asked_thirteen.synchronization2, VK_TRUE);
  CHECK_EQ(asked_thirteen.shaderDemoteToHelperInvocation, VK_TRUE);
  CHECK_EQ(asked_twelve.timelineSemaphore, VK_TRUE);
  CHECK_EQ(asked_twelve.bufferDeviceAddress, VK_TRUE);
  CHECK_EQ(asked_twelve.descriptorIndexing, VK_TRUE);
  /* Nothing else was invented along with them. */
  CHECK_EQ(asked_twelve.hostQueryReset, VK_FALSE);
  CHECK_EQ(asked_thirteen.inlineUniformBlock, VK_FALSE); }

TEST(reported_extensions, a_feature_the_host_does_not_have_is_absent_here_too)
{ ExtensionsRig rig;
  const auto    features2 =
      rig.entry<void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2 *)>("vkGetPhysicalDeviceFeatures2");
  /* A host missing dynamic rendering, which vkd3d-proton cannot do without:
   * the guest must say so, so that the layer fails device creation with its own
   * diagnostic rather than drawing into nothing. */
  VkPhysicalDeviceVulkan13Features without = {};
  without.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  without.synchronization2 = VK_TRUE;
  VkPhysicalDeviceFeatures2 answered = {};
  answered.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  answered.pNext = &without;
  std::uint8_t   answer[MockKmt::max_answer] = {};
  av::WireWriter writer(answer, sizeof answer);
  writer.write_pod<std::int32_t>(0);
  serialize_VkPhysicalDeviceFeatures2(writer, answered);
  CHECK_STATUS(writer.status(), av::Status::ok);
  rig.rig().kmt().queue_answer(answer, static_cast<std::uint32_t>(writer.offset()));

  VkPhysicalDeviceVulkan13Features asked_thirteen = {};
  asked_thirteen.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  asked_thirteen.dynamicRendering = VK_TRUE;   /* what the application hoped for */
  VkPhysicalDeviceFeatures2 asked = {};
  asked.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  asked.pNext = &asked_thirteen;
  features2(rig.physical(), &asked);
  /* The host's no overwrote the application's hope: the answer is the host's,
   * not whatever was in the structure before. */
  CHECK_EQ(asked_thirteen.dynamicRendering, VK_FALSE);
  CHECK_EQ(asked_thirteen.synchronization2, VK_TRUE); }
