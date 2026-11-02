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

    void answer_features(VkStructureType type)
    {
        Node                      answered = node_of(type, nullptr);
        answered.fields[0] = 1u;
        VkPhysicalDeviceFeatures2 features = {};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &answered;
        features.features.robustBufferAccess = VK_TRUE;
        std::uint8_t answer[MockKmt::max_answer] = {};
        av::WireWriter writer(answer, sizeof answer);
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

}

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
      {"VK_EXT_transform_feedback", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT},
      {"VK_KHR_maintenance5", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR},
      {"VK_KHR_maintenance6", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR},
      {"VK_KHR_maintenance7", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_7_FEATURES_KHR},
      {"VK_KHR_maintenance8", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_8_FEATURES_KHR},
      {"VK_EXT_extended_dynamic_state", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT},
      {"VK_EXT_extended_dynamic_state2", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT},
      {"VK_EXT_extended_dynamic_state3", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT},
      {"VK_EXT_line_rasterization", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT},
      {"VK_EXT_conditional_rendering", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT},
      {"VK_EXT_mesh_shader", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT},
      {"VK_KHR_fragment_shading_rate", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR},
      {"VK_KHR_dynamic_rendering_local_read", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR},
      {"VK_EXT_border_color_swizzle", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BORDER_COLOR_SWIZZLE_FEATURES_EXT},
      {"VK_EXT_depth_bias_control", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT},
      {"VK_EXT_dynamic_rendering_unused_attachments", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT},
      {"VK_EXT_fragment_shader_interlock", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT},
      {"VK_EXT_non_seamless_cube_map", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT},
      {"VK_EXT_shader_module_identifier", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT},
      {"VK_KHR_shader_float_controls2", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT_CONTROLS_2_FEATURES_KHR},
      {"VK_KHR_shader_subgroup_uniform_control_flow", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_FEATURES_KHR},
      {"VK_KHR_compute_shader_derivatives", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR},
      {"VK_KHR_fragment_shader_barycentric", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR},
      {"VK_EXT_shader_image_atomic_int64", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT},
      {"VK_KHR_shader_maximal_reconvergence", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MAXIMAL_RECONVERGENCE_FEATURES_KHR},
      {"VK_KHR_shader_quad_control", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_QUAD_CONTROL_FEATURES_KHR},
      {"VK_EXT_image_sliced_view_of_3d", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_SLICED_VIEW_OF_3D_FEATURES_EXT},
      {"VK_EXT_image_compression_control", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_FEATURES_EXT},
      {"VK_AMD_device_coherent_memory", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD},
      {"VK_MESA_image_alignment_control", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ALIGNMENT_CONTROL_FEATURES_MESA},
      {"VK_KHR_cooperative_matrix", VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR}};
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
      CHECK_EQ(chained.fields[0], 1u);
      CHECK_EQ(asked.features.robustBufferAccess, VK_TRUE);
      CHECK(asked.pNext == &chained);
      CHECK(chained.pNext == nullptr);
  }
  CHECK(rig.rig().root().invariants_hold()); }

TEST(reported_extensions, an_extension_with_no_structure_of_its_own_is_enabled_on_the_host)
{ ExtensionsRig rig;
  const char *const names[] = {
                                "VK_KHR_load_store_op_none",
                                "VK_EXT_shader_stencil_export",
                                "VK_AMD_shader_fragment_mask",
                                "VK_AMD_shader_explicit_vertex_parameter",
                                "VK_EXT_conservative_rasterization",
                                "VK_EXT_sample_locations",
                                "VK_AMD_buffer_marker",
                                "VK_AMD_shader_core_properties",
                                "VK_AMD_shader_core_properties2"};
  const auto create = rig.entry<VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *,
                                             const VkAllocationCallbacks *, VkDevice *)>("vkCreateDevice");
  const auto destroy = rig.entry<void (*)(VkDevice, const VkAllocationCallbacks *)>("vkDestroyDevice");
  for (const char *const name : names) {
      VkDeviceCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      info.enabledExtensionCount = 1u;
      info.ppEnabledExtensionNames = &name;
      rig.rig().host().vk_handle(0, ScriptedHost::vk_device + 1u);
      VkDevice made = VK_NULL_HANDLE;
      CHECK_EQ(create(rig.physical(), &info, nullptr, &made), VK_SUCCESS);
      std::uint32_t sent = rig.rig().kmt().commands();
      while (sent > 0u && rig.rig().kmt().command_id(sent - 1u) != static_cast<std::uint32_t>(kCmd_vkCreateDevice))
          sent--;
      CHECK(sent > 0u);
      const std::uint8_t *bytes = rig.rig().kmt().command_bytes(sent - 1u);
      WireReader          body(bytes + CommandHeader::encoded_size,
                               MockKmt::journal_bytes - CommandHeader::encoded_size);
      static_cast<void>(body.read_u64());
      std::uint8_t block[1024] = {};
      WireArena    arena(block, sizeof block);
      VkDeviceCreateInfo decoded = {};
      CHECK(deserialize_VkDeviceCreateInfo(body, &decoded, arena));
      CHECK_EQ(decoded.enabledExtensionCount, 1u);
      CHECK(decoded.ppEnabledExtensionNames != nullptr && decoded.ppEnabledExtensionNames[0] != nullptr &&
            std::strcmp(decoded.ppEnabledExtensionNames[0], name) == 0);
      destroy(made, nullptr);
  } }

TEST(reported_extensions, a_structure_the_wire_does_not_carry_is_not_sent)
{ ExtensionsRig rig;
  const auto    features2 =
      rig.entry<void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2 *)>("vkGetPhysicalDeviceFeatures2");
  Node                      chained = node_of(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
                                              nullptr);
  VkPhysicalDeviceFeatures2 asked = {};
  asked.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  asked.pNext = &chained;
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
  CHECK_EQ(chained.fields[0], 0u);
  CHECK_EQ(asked.features.robustBufferAccess, VK_TRUE); }

TEST(reported_extensions, a_memory_priority_and_a_transform_feedback_command_reach_the_host)
{ ExtensionsRig rig;
  const auto    set_priority =
      rig.entry<void (*)(VkDevice, VkDeviceMemory, float)>("vkSetDeviceMemoryPriorityEXT");
  CHECK(set_priority != nullptr);
  rig.rig().host().nop();
  set_priority(rig.device(), reinterpret_cast<VkDeviceMemory>(0x900u), 0.5F);
  CHECK_EQ(rig.rig().kmt().command_id(rig.rig().kmt().commands() - 1u),
           static_cast<std::uint32_t>(kCmd_vkSetDeviceMemoryPriorityEXT));

  const auto begin_query = rig.entry<void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t)>(
      "vkCmdBeginQueryIndexedEXT");
  CHECK(begin_query != nullptr); }

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
  const std::uint8_t *bytes = rig.rig().kmt().command_bytes(rig.rig().kmt().commands() - 1u);
  WireReader          body(bytes + CommandHeader::encoded_size,
                           MockKmt::journal_bytes - CommandHeader::encoded_size);
  static_cast<void>(body.read_u64());
  static_cast<void>(body.read_u64());
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

TEST(reported_extensions, an_image_s_format_list_reaches_the_host)
{ ExtensionsRig rig;
  const auto    create_image =
      rig.entry<VkResult (*)(VkDevice, const VkImageCreateInfo *, const VkAllocationCallbacks *, VkImage *)>(
          "vkCreateImage");
  CHECK(create_image != nullptr);
  const VkFormat           views[2] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
  VkImageFormatListCreateInfo formats = {};
  formats.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO;
  formats.viewFormatCount = 2u;
  formats.pViewFormats = views;
  VkImageCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  create.pNext = &formats;
  create.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
  create.imageType = VK_IMAGE_TYPE_2D;
  create.format = VK_FORMAT_R8G8B8A8_UNORM;
  create.extent = {64u, 64u, 1u};
  create.mipLevels = 1u;
  create.arrayLayers = 1u;
  create.samples = VK_SAMPLE_COUNT_1_BIT;
  create.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  VkImage made = VK_NULL_HANDLE;
  rig.rig().host().vk_handle(0, 0xB11u);
  static_cast<void>(create_image(rig.device(), &create, nullptr, &made));
  CHECK_EQ(rig.rig().kmt().command_id(rig.rig().kmt().commands() - 1u),
           static_cast<std::uint32_t>(kCmd_vkCreateImage));
  const std::uint8_t *bytes = rig.rig().kmt().command_bytes(rig.rig().kmt().commands() - 1u);
  WireReader          body(bytes + CommandHeader::encoded_size,
                           MockKmt::journal_bytes - CommandHeader::encoded_size);
  static_cast<void>(body.read_u64());
  std::uint8_t block[4096] = {};
  WireArena    arena(block, sizeof block);
  VkImageCreateInfo decoded = {};
  CHECK(deserialize_VkImageCreateInfo(body, &decoded, arena));
  const auto *list = static_cast<const VkImageFormatListCreateInfo *>(decoded.pNext);
  CHECK(list != nullptr);
  CHECK_EQ(static_cast<std::uint32_t>(list->sType),
           static_cast<std::uint32_t>(VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO));
  CHECK_EQ(list->viewFormatCount, 2u);
  CHECK_EQ(static_cast<std::uint32_t>(list->pViewFormats[0]), static_cast<std::uint32_t>(VK_FORMAT_R8G8B8A8_UNORM));
  CHECK_EQ(static_cast<std::uint32_t>(list->pViewFormats[1]), static_cast<std::uint32_t>(VK_FORMAT_R8G8B8A8_SRGB));
  CHECK(list->pNext == nullptr); }

TEST(reported_extensions, the_core_features_vkd3d_requires_are_the_host_s_answer)
{ ExtensionsRig rig;
  const auto    features2 =
      rig.entry<void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2 *)>("vkGetPhysicalDeviceFeatures2");

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
  CHECK_EQ(asked_twelve.hostQueryReset, VK_FALSE);
  CHECK_EQ(asked_thirteen.inlineUniformBlock, VK_FALSE); }

TEST(reported_extensions, a_feature_the_host_does_not_have_is_absent_here_too)
{ ExtensionsRig rig;
  const auto    features2 =
      rig.entry<void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2 *)>("vkGetPhysicalDeviceFeatures2");
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
  asked_thirteen.dynamicRendering = VK_TRUE;
  VkPhysicalDeviceFeatures2 asked = {};
  asked.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  asked.pNext = &asked_thirteen;
  features2(rig.physical(), &asked);
  CHECK_EQ(asked_thirteen.dynamicRendering, VK_FALSE);
  CHECK_EQ(asked_thirteen.synchronization2, VK_TRUE); }
