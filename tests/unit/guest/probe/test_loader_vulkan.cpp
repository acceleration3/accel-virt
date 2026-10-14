/* The probe's Vulkan through a loader entry point, here the ICD's own over
 * MockKmt with the host played by ScriptedHost: vkCreateInstance comes from
 * the null instance, everything else from the instance made, the ICD's own
 * functions from a device through vkGetDeviceProcAddr, and a loader whose
 * driver names no vkAvForceContextFault is answered for. The full-stack test
 * runs every other call against a real GPU. */
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/probe/loader_vulkan.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/bound_icd_entry.hpp"
#include "harness/icd/icd_rig.hpp"
#include "harness/icd/shipped_batch.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"

#include <cstdint>
#include <cstring>

namespace av {

struct KmtTransportProbe {
    static std::uint32_t ring(KmtTransport &transport) { return transport.allocations_[0]; }
};

namespace icd {

struct ConnectionProbe {
    static KmtTransport &transport(Connection &connection) { return connection.channel_.transport(); }
};

}  // namespace icd

}  // namespace av

using av::IcdRig;
using av::KmtTransportProbe;
using av::ShippedBatch;
using av::Status;
using av::icd::ConnectionProbe;
using namespace av::vkproto;
using av::LoaderVulkan;
using av::ScriptedHost;
using Kind = av::icd::LoaderHandle::Kind;

namespace {

/* A driver that answers every Vulkan command and nothing of the ICD's own:
 * its devices name no vkAvForceContextFault. */
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL device_without_fault(VkDevice device, const char *name)
{
    if (std::strcmp(name, "vkAvForceContextFault") == 0) return nullptr;
    return av::bound_icd_device_proc_addr(device, name);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL without_fault(VkInstance instance, const char *name)
{
    if (std::strcmp(name, "vkGetDeviceProcAddr") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&device_without_fault);
    return av::bound_icd_instance_proc_addr(instance, name);
}

std::uint32_t last_command(const av::MockKmt &kmt) { return kmt.command_id(kmt.commands() - 1u); }

/* The structure the wrapper builds and hands on. Declared and not defined: the
 * stub below only has to have the same signature as the entry point the loader
 * resolves, and UBSan compares that signature exactly. */
}  // namespace

namespace av {
struct MemoryGetWin32HandleInfo;
}  // namespace av

namespace {

/* Feature 053. The three entry points the sharing steps resolve are driver
 * private or extension entries, so a loader may or may not have them, and the
 * probe has to survive both. Stubs here rather than the real ICD because what
 * is being judged is the wrapper - whether it calls through a null, and what it
 * makes of an answer that is not VK_SUCCESS. */
std::uint32_t stub_calls = 0u;

VKAPI_ATTR VkResult VKAPI_CALL refusing_status(VkDevice, std::uint32_t *status)
{
    stub_calls++;
    *status = 0xDEADBEEFu;   /* written and then not to be believed */
    return VK_ERROR_INITIALIZATION_FAILED;
}

VKAPI_ATTR VkResult VKAPI_CALL stub_enumerate(VkPhysicalDevice, const char *, std::uint32_t *count,
                                              VkExtensionProperties *)
{
    stub_calls++;
    *count = 3u;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL stub_memory_handle(VkDevice, const av::MemoryGetWin32HandleInfo *,
                                                  void **handle)
{
    stub_calls++;
    *handle = reinterpret_cast<void *>(0x5150u);
    return VK_SUCCESS;
}

/* A loader that has every one of them, the driver's status on its devices. */
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL device_with_sharing(VkDevice device, const char *name)
{
    if (std::strcmp(name, "vkAvLastKernelStatus") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&refusing_status);
    return av::bound_icd_device_proc_addr(device, name);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL with_sharing(VkInstance instance, const char *name)
{
    if (std::strcmp(name, "vkGetDeviceProcAddr") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&device_with_sharing);
    if (std::strcmp(name, "vkEnumerateDeviceExtensionProperties") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&stub_enumerate);
    if (std::strcmp(name, "vkGetMemoryWin32HandleKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&stub_memory_handle);
    return av::bound_icd_instance_proc_addr(instance, name);
}

/* And one that has none of them. */
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL device_without_sharing(VkDevice device, const char *name)
{
    if (std::strcmp(name, "vkAvLastKernelStatus") == 0) return nullptr;
    return av::bound_icd_device_proc_addr(device, name);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL without_sharing(VkInstance instance, const char *name)
{
    if (std::strcmp(name, "vkGetDeviceProcAddr") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&device_without_sharing);
    return av::bound_icd_instance_proc_addr(instance, name);
}

VkInstanceCreateInfo instance_info()
{
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    return info;
}

/* An instance and a device of its one physical device, made through
 * `vulkan`, so its device lookup is the one resolved. */
VkDevice loader_device(av::ScriptedHost &host, av::LoaderVulkan &vulkan, VkInstance &instance)
{
    const VkInstanceCreateInfo info = instance_info();
    host.vk_handle(0, av::ScriptedHost::vk_instance);
    CHECK_EQ(vulkan.create_instance(&info, &instance), VK_SUCCESS);
    std::uint32_t    count = 1u;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    host.vk_physical_devices(0, 1u);
    CHECK_EQ(vulkan.enumerate_physical_devices(instance, &count, &physical), VK_SUCCESS);
    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    VkDevice device = VK_NULL_HANDLE;
    host.vk_handle(0, av::ScriptedHost::vk_device);
    CHECK_EQ(vulkan.create_device(physical, &device_info, &device), VK_SUCCESS);
    return device;
}

}  // namespace

/* The instance is the ICD's, and the functions resolved through it list its
 * physical device and destroy it. */
TEST(loader_vulkan, an_instance_resolves_the_functions_it_is_called_through)
{ IcdRig rig;
  LoaderVulkan vulkan(&av::bound_icd_instance_proc_addr);
  const VkInstanceCreateInfo info = instance_info();
  VkInstance instance = VK_NULL_HANDLE;
  rig.host().vk_handle(0, ScriptedHost::vk_instance);
  CHECK_EQ(vulkan.create_instance(&info, &instance), VK_SUCCESS);
  CHECK(rig.root().connection_of(instance, Kind::instance) != nullptr);
  CHECK(rig.root().invariants_hold());
  rig.host().vk_physical_devices(0, 1u);
  std::uint32_t    count = 1u;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  CHECK_EQ(vulkan.enumerate_physical_devices(instance, &count, &physical), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(physical == rig.physical_device(instance));
  vulkan.destroy_instance(instance);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

/* A refused instance resolves nothing, and is passed on. */
TEST(loader_vulkan, a_refused_instance_is_passed_on)
{ IcdRig rig;
  LoaderVulkan vulkan(&av::bound_icd_instance_proc_addr);
  const VkInstanceCreateInfo info = instance_info();
  VkInstance instance = VK_NULL_HANDLE;
  rig.host().vk_handle(-3, 0u);
  CHECK_EQ(vulkan.create_instance(&info, &instance), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

/* The ICD's fault is found through a device and faults its context; a driver
 * without it is answered VK_ERROR_EXTENSION_NOT_PRESENT and nothing is sent,
 * and a wrapper that made no instance has no device lookup to ask. */
TEST(loader_vulkan, the_forced_fault_is_found_through_the_device_or_refused)
{ IcdRig rig;
  LoaderVulkan unmade(&av::bound_icd_instance_proc_addr);
  CHECK_EQ(unmade.force_context_fault(reinterpret_cast<VkDevice>(0x1u)), VK_ERROR_EXTENSION_NOT_PRESENT);
  LoaderVulkan bare(&without_fault);
  VkInstance   bare_instance = VK_NULL_HANDLE;
  const VkDevice bare_device = loader_device(rig.host(), bare, bare_instance);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(bare.force_context_fault(bare_device), VK_ERROR_EXTENSION_NOT_PRESENT);
  CHECK_EQ(bare.force_context_fault(VK_NULL_HANDLE), VK_ERROR_EXTENSION_NOT_PRESENT);
  CHECK_EQ(rig.kmt().commands(), sent);
  bare.destroy_device(bare_device);
  bare.destroy_instance(bare_instance);
  LoaderVulkan icd(&av::bound_icd_instance_proc_addr);
  VkInstance   instance = VK_NULL_HANDLE;
  const VkDevice device = loader_device(rig.host(), icd, instance);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(icd.force_context_fault(device), VK_ERROR_DEVICE_LOST);
  CHECK(rig.root().connection_of(instance, Kind::instance)->faulted());
  icd.destroy_device(device);
  icd.destroy_instance(instance);
  CHECK(rig.root().invariants_hold()); }

namespace {

/* A LoaderVulkan over the rig, with each call checked to have reached the host
 * as the Vulkan command of its own name. */
class ForwardingRig {
public:
    ForwardingRig() : vulkan_(&av::bound_icd_instance_proc_addr) {}

    IcdRig       &rig() { return rig_; }
    LoaderVulkan &vulkan() { return vulkan_; }
    ScriptedHost &host() { return rig_.host(); }

    /* The last command sent, which must be `id`. */
    void sent(std::uint32_t id) { CHECK_EQ(last_command(rig_.kmt()), id); }
    /* Nothing sent since the count was `before`. */
    void quiet(std::uint32_t before) { CHECK_EQ(rig_.kmt().commands(), before); }
    std::uint32_t commands() { return rig_.kmt().commands(); }

    std::uint32_t ring(VkInstance instance)
    {
        return KmtTransportProbe::ring(ConnectionProbe::transport(*rig_.root().connection_of(instance, Kind::instance)));
    }

private:
    IcdRig       rig_;
    LoaderVulkan vulkan_;
};

template <typename Handle>
Handle host_handle(std::uint64_t value) { return reinterpret_cast<Handle>(static_cast<std::uintptr_t>(value)); }

constexpr std::uint64_t made = 0x9900u;

}  // namespace

/* Every call the probe makes, in the order it makes them, arrives as its own
 * command: a function resolved under another name would send another id. */
TEST(loader_vulkan, every_call_reaches_the_host_as_its_own_command)
{ ForwardingRig rig;
  LoaderVulkan &vk = rig.vulkan();
  const VkInstanceCreateInfo info = instance_info();
  VkInstance instance = VK_NULL_HANDLE;
  rig.host().vk_handle(0, ScriptedHost::vk_instance);
  CHECK_EQ(vk.create_instance(&info, &instance), VK_SUCCESS);
  rig.sent(kCmd_vkCreateInstance);
  std::uint32_t    count = 1u;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  rig.host().vk_physical_devices(0, 1u);
  CHECK_EQ(vk.enumerate_physical_devices(instance, &count, &physical), VK_SUCCESS);
  rig.sent(kCmd_vkEnumeratePhysicalDevices);
  rig.host().vk_queue_families(1u);
  VkQueueFamilyProperties family = {};
  count = 1u;
  vk.queue_family_properties(physical, &count, &family);
  rig.sent(kCmd_vkGetPhysicalDeviceQueueFamilyProperties);
  CHECK_EQ(family.queueFlags, static_cast<VkQueueFlags>(VK_QUEUE_GRAPHICS_BIT));
  rig.host().vk_memory_properties(2u, 0x2u);
  VkPhysicalDeviceMemoryProperties memory = {};
  vk.memory_properties(physical, &memory);
  rig.sent(kCmd_vkGetPhysicalDeviceMemoryProperties);
  CHECK_EQ(memory.memoryTypeCount, 2u);
  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  VkDevice device = VK_NULL_HANDLE;
  rig.host().vk_handle(0, ScriptedHost::vk_device);
  CHECK_EQ(vk.create_device(physical, &device_info, &device), VK_SUCCESS);
  rig.sent(kCmd_vkCreateDevice);
  VkQueue queue = VK_NULL_HANDLE;
  rig.host().vk_handle(0, ScriptedHost::vk_queue);
  vk.device_queue(device, 0u, 0u, &queue);
  rig.sent(kCmd_vkGetDeviceQueue);
  CHECK(queue != VK_NULL_HANDLE);

  const VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0u, 4096u,
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr};
  VkBuffer buffer = VK_NULL_HANDLE;
  rig.host().vk_handle(0, ScriptedHost::vk_buffer);
  CHECK_EQ(vk.create_buffer(device, &buffer_info, &buffer), VK_SUCCESS);
  rig.sent(kCmd_vkCreateBuffer);
  VkMemoryRequirements requirements = {};
  rig.host().vk_memory_requirements(0x1u);
  vk.buffer_memory_requirements(device, buffer, &requirements);
  rig.sent(kCmd_vkGetBufferMemoryRequirements);
  CHECK_EQ(requirements.memoryTypeBits, 0x1u);
  const VkMemoryAllocateInfo allocation = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, 4096u, 0u};
  VkDeviceMemory device_memory = VK_NULL_HANDLE;
  /* The device learns its types before its first allocation. */
  rig.host().vk_memory_properties(2u, 0x2u);
  rig.host().vk_handle(0, ScriptedHost::vk_memory);
  CHECK_EQ(vk.allocate_memory(device, &allocation, &device_memory), VK_SUCCESS);
  rig.sent(kCmd_vkAllocateMemory);
  rig.host().vk_result(0);
  CHECK_EQ(vk.bind_buffer_memory(device, buffer, device_memory), VK_SUCCESS);
  rig.sent(kCmd_vkBindBufferMemory);
  /* A device-local type is not the process's to map: refused before any
   * command, which is how the ICD's own vkMapMemory answers. */
  std::uint32_t before = rig.commands();
  void *mapped = nullptr;
  CHECK_EQ(vk.map_memory(device, device_memory, &mapped), VK_ERROR_MEMORY_MAP_FAILED);
  rig.quiet(before);

  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  VkImage image = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 1u);
  CHECK_EQ(vk.create_image(device, &image_info, &image), VK_SUCCESS);
  rig.sent(kCmd_vkCreateImage);
  rig.host().vk_memory_requirements(0x1u);
  vk.image_memory_requirements(device, image, &requirements);
  rig.sent(kCmd_vkGetImageMemoryRequirements);
  rig.host().vk_result(0);
  CHECK_EQ(vk.bind_image_memory(device, image, device_memory), VK_SUCCESS);
  rig.sent(kCmd_vkBindImageMemory);
  VkImageViewCreateInfo view_info = {};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  VkImageView view = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 2u);
  CHECK_EQ(vk.create_image_view(device, &view_info, &view), VK_SUCCESS);
  rig.sent(kCmd_vkCreateImageView);
  const std::uint32_t words[1] = {0x07230203u};
  const VkShaderModuleCreateInfo shader_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0u,
                                                sizeof words, words};
  VkShaderModule shader = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 3u);
  CHECK_EQ(vk.create_shader_module(device, &shader_info, &shader), VK_SUCCESS);
  rig.sent(kCmd_vkCreateShaderModule);
  VkDescriptorSetLayoutCreateInfo set_layout_info = {};
  set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 4u);
  CHECK_EQ(vk.create_descriptor_set_layout(device, &set_layout_info, &set_layout), VK_SUCCESS);
  rig.sent(kCmd_vkCreateDescriptorSetLayout);
  VkPipelineLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 5u);
  CHECK_EQ(vk.create_pipeline_layout(device, &layout_info, &layout), VK_SUCCESS);
  rig.sent(kCmd_vkCreatePipelineLayout);
  VkComputePipelineCreateInfo compute_info = {};
  compute_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  compute_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  compute_info.stage.module = shader;
  compute_info.stage.pName = "main";
  compute_info.layout = layout;
  VkPipeline compute = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 6u);
  CHECK_EQ(vk.create_compute_pipeline(device, &compute_info, &compute), VK_SUCCESS);
  rig.sent(kCmd_vkCreateComputePipelines);
  CHECK(compute == host_handle<VkPipeline>(made + 6u));
  VkGraphicsPipelineCreateInfo graphics_info = {};
  graphics_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphics_info.layout = layout;
  VkPipeline graphics = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 7u);
  CHECK_EQ(vk.create_graphics_pipeline(device, &graphics_info, &graphics), VK_SUCCESS);
  rig.sent(kCmd_vkCreateGraphicsPipelines);
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 8u);
  CHECK_EQ(vk.create_descriptor_pool(device, &pool_info, &pool), VK_SUCCESS);
  rig.sent(kCmd_vkCreateDescriptorPool);
  const VkDescriptorSetAllocateInfo set_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1u,
                                                &set_layout};
  VkDescriptorSet set = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 9u);
  CHECK_EQ(vk.allocate_descriptor_set(device, &set_info, &set), VK_SUCCESS);
  rig.sent(kCmd_vkAllocateDescriptorSets);
  const VkDescriptorBufferInfo described = {buffer, 0u, 4096u};
  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.descriptorCount = 1u;
  write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  write.pBufferInfo = &described;
  vk.update_descriptor_set(device, &write);
  rig.sent(kCmd_vkUpdateDescriptorSets);
  VkRenderPassCreateInfo pass_info = {};
  pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  VkRenderPass render_pass = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 10u);
  CHECK_EQ(vk.create_render_pass(device, &pass_info, &render_pass), VK_SUCCESS);
  rig.sent(kCmd_vkCreateRenderPass);
  VkFramebufferCreateInfo framebuffer_info = {};
  framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_info.renderPass = render_pass;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 11u);
  CHECK_EQ(vk.create_framebuffer(device, &framebuffer_info, &framebuffer), VK_SUCCESS);
  rig.sent(kCmd_vkCreateFramebuffer);
  VkCommandPoolCreateInfo command_pool_info = {};
  command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 12u);
  CHECK_EQ(vk.create_command_pool(device, &command_pool_info, &command_pool), VK_SUCCESS);
  rig.sent(kCmd_vkCreateCommandPool);
  const VkCommandBufferAllocateInfo command_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
                                                    command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1u};
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 13u);
  CHECK_EQ(vk.allocate_command_buffer(device, &command_info, &command_buffer), VK_SUCCESS);
  rig.sent(kCmd_vkAllocateCommandBuffers);
  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence = VK_NULL_HANDLE;
  rig.host().vk_handle(0, made + 14u);
  CHECK_EQ(vk.create_fence(device, &fence_info, &fence), VK_SUCCESS);
  rig.sent(kCmd_vkCreateFence);

  /* The recording ships as one batch when it ends. */
  const VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0u, nullptr};
  before = rig.commands();
  CHECK_EQ(vk.begin_command_buffer(command_buffer, &begin), VK_SUCCESS);
  vk.cmd_bind_pipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute);
  vk.cmd_bind_compute_set(command_buffer, layout, set);
  vk.cmd_dispatch(command_buffer, 16u);
  VkRenderPassBeginInfo pass = {};
  pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  pass.renderPass = render_pass;
  pass.framebuffer = framebuffer;
  vk.cmd_begin_render_pass(command_buffer, &pass);
  vk.cmd_draw(command_buffer, 3u);
  vk.cmd_end_render_pass(command_buffer);
  const VkBufferImageCopy region = {};
  vk.cmd_copy_image_to_buffer(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, &region);
  rig.quiet(before);
  CHECK_EQ(vk.end_command_buffer(command_buffer), VK_SUCCESS);
  ShippedBatch shipped(rig.rig().kmt(), rig.ring(instance));
  CHECK_STATUS(shipped.read(), Status::ok);
  const std::uint32_t recorded[] = {kCmd_vkBeginCommandBuffer, kCmd_vkCmdBindPipeline, kCmd_vkCmdBindDescriptorSets,
                                    kCmd_vkCmdDispatch,        kCmd_vkCmdBeginRenderPass, kCmd_vkCmdDraw,
                                    kCmd_vkCmdEndRenderPass,   kCmd_vkCmdCopyImageToBuffer, kCmd_vkEndCommandBuffer};
  CHECK_EQ(shipped.commands(), sizeof recorded / sizeof recorded[0]);
  for (std::uint32_t index = 0u; index < shipped.commands() && index < sizeof recorded / sizeof recorded[0]; index++)
      CHECK_EQ(shipped.header(index).id(), recorded[index]);

  VkSubmitInfo submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1u;
  submit.pCommandBuffers = &command_buffer;
  CHECK_EQ(vk.queue_submit(queue, &submit, fence), VK_SUCCESS);
  rig.sent(kCmd_vkQueueSubmit);
  rig.host().vk_result(0);
  CHECK_EQ(vk.wait_for_fence(device, fence, 0u), VK_SUCCESS);
  rig.sent(kCmd_vkWaitForFences);
  /* The device waits idle on a fence of its own on its one queue, kept for
   * the next idle wait and destroyed with the device. */
  rig.host().vk_handle(0, made + 15u);
  rig.host().vk_result(0);
  CHECK_EQ(vk.device_wait_idle(device), VK_SUCCESS);
  rig.sent(kCmd_vkWaitForFences);
  /* Nothing sent since: still idle, answered without the host. */
  const std::uint32_t idle = rig.commands();
  CHECK_EQ(vk.device_wait_idle(device), VK_SUCCESS);
  rig.quiet(idle);

  vk.destroy_fence(device, fence);
  rig.sent(kCmd_vkDestroyFence);
  vk.destroy_command_pool(device, command_pool);
  rig.sent(kCmd_vkDestroyCommandPool);
  vk.destroy_framebuffer(device, framebuffer);
  rig.sent(kCmd_vkDestroyFramebuffer);
  vk.destroy_render_pass(device, render_pass);
  rig.sent(kCmd_vkDestroyRenderPass);
  vk.destroy_descriptor_pool(device, pool);
  rig.sent(kCmd_vkDestroyDescriptorPool);
  vk.destroy_pipeline(device, graphics);
  rig.sent(kCmd_vkDestroyPipeline);
  vk.destroy_pipeline_layout(device, layout);
  rig.sent(kCmd_vkDestroyPipelineLayout);
  vk.destroy_descriptor_set_layout(device, set_layout);
  rig.sent(kCmd_vkDestroyDescriptorSetLayout);
  vk.destroy_shader_module(device, shader);
  rig.sent(kCmd_vkDestroyShaderModule);
  vk.destroy_image_view(device, view);
  rig.sent(kCmd_vkDestroyImageView);
  vk.destroy_image(device, image);
  rig.sent(kCmd_vkDestroyImage);
  vk.destroy_buffer(device, buffer);
  rig.sent(kCmd_vkDestroyBuffer);
  vk.free_memory(device, device_memory);
  rig.sent(kCmd_vkFreeMemory);
  /* Last, since MockKmt carries less than a whole properties structure: the
   * answer is short, and what matters here is which command asked. */
  rig.host().vk_result(0);
  VkPhysicalDeviceProperties2 properties = {};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  vk.physical_device_properties2(physical, &properties);
  rig.sent(kCmd_vkGetPhysicalDeviceProperties2);
  vk.destroy_device(device);
  vk.destroy_instance(instance);
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.rig().kmt().open_objects(), 0u); }

/* The window-system commands the loader resolves are the ICD's own (feature
 * 046): a surface for a window the ICD's window surface knows, what that
 * surface offers, and the refusals a swapchain and a present answer without
 * the host being asked anything. */
TEST(loader_vulkan, the_window_system_commands_answer_through_the_instance)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const VkPhysicalDevice physical = rig.physical_device(instance);
  LoaderVulkan vulkan(&av::bound_icd_instance_proc_addr);
  /* create_instance resolves the table this test calls through. */
  VkInstance own = VK_NULL_HANDLE;
  const VkInstanceCreateInfo info = instance_info();
  rig.host().vk_handle(0, ScriptedHost::vk_instance);
  CHECK_EQ(vulkan.create_instance(&info, &own), VK_SUCCESS);
  void *const window = reinterpret_cast<void *>(0x7770u);
  rig.windows().add_dc(reinterpret_cast<void *>(0x7771u), window, 8u, 4u);
  const std::uint32_t sent = rig.kmt().commands();
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  CHECK_EQ(vulkan.create_win32_surface(own, reinterpret_cast<void *>(0x7772u), window, &surface), VK_SUCCESS);
  CHECK(surface != VK_NULL_HANDLE);
  VkSurfaceCapabilitiesKHR capabilities = {};
  CHECK_EQ(vulkan.surface_capabilities(physical, surface, &capabilities), VK_SUCCESS);
  CHECK_EQ(capabilities.currentExtent.width, 8u);
  CHECK_EQ(capabilities.currentExtent.height, 4u);
  std::uint32_t      count = 0u;
  CHECK_EQ(vulkan.surface_formats(physical, surface, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 4u);
  VkSurfaceFormatKHR formats[4] = {};
  CHECK_EQ(vulkan.surface_formats(physical, surface, &count, formats), VK_SUCCESS);
  CHECK_EQ(formats[0].format, VK_FORMAT_B8G8R8A8_UNORM);
  VkPresentModeKHR modes[2] = {};
  count = 2u;
  CHECK_EQ(vulkan.surface_present_modes(physical, surface, &count, modes), VK_SUCCESS);
  CHECK_EQ(modes[0], VK_PRESENT_MODE_FIFO_KHR);

  /* A swapchain on a device this ICD did not make, a present of a swapchain it
   * did not make, and the images and acquire of one: each refused here, so
   * nothing of this reaches the host. */
  VkSwapchainCreateInfoKHR create = {};
  create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create.surface = surface;
  create.minImageCount = 2u;
  create.imageExtent = {8u, 4u};
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  int            stack = 0;
  CHECK_EQ(vulkan.create_swapchain(reinterpret_cast<VkDevice>(&stack), &create, &swapchain),
           VK_ERROR_INITIALIZATION_FAILED);
  const VkSwapchainKHR nowhere = reinterpret_cast<VkSwapchainKHR>(0x50u);
  std::uint32_t        images = 1u;
  VkImage              image = VK_NULL_HANDLE;
  CHECK_EQ(vulkan.swapchain_images(reinterpret_cast<VkDevice>(&stack), nowhere, &images, &image),
           VK_ERROR_INITIALIZATION_FAILED);
  std::uint32_t index = 0u;
  CHECK_EQ(vulkan.acquire_next_image(reinterpret_cast<VkDevice>(&stack), nowhere, 0u, &index),
           VK_ERROR_OUT_OF_DATE_KHR);
  VkPresentInfoKHR present = {};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.swapchainCount = 1u;
  present.pSwapchains = &nowhere;
  present.pImageIndices = &index;
  CHECK_EQ(vulkan.queue_present(reinterpret_cast<VkQueue>(&stack), &present), VK_ERROR_OUT_OF_DATE_KHR);
  vulkan.destroy_swapchain(reinterpret_cast<VkDevice>(&stack), nowhere);
  CHECK_EQ(rig.kmt().commands(), sent);

  vulkan.destroy_surface(own, surface);
  CHECK_EQ(vulkan.surface_capabilities(physical, surface, &capabilities), VK_ERROR_SURFACE_LOST_KHR);
  vulkan.destroy_instance(own);
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

/* Feature 053: the export the shared-object probe asks for, resolved through
 * the loader. A loader whose driver has no such function answers
 * VK_ERROR_EXTENSION_NOT_PRESENT and sends nothing - which is what a host with
 * no fd sharing looks like from the guest, and is not a crash. */
TEST(loader_vulkan, the_win32_memory_handle_is_found_through_the_loader_or_refused)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  LoaderVulkan bare(&without_fault);
  const VkPhysicalDevice physical = rig.physical_device(instance);
  const std::uint32_t sent = rig.kmt().commands();
  void *handle = reinterpret_cast<void *>(0x1234u);
  CHECK_EQ(bare.memory_win32_handle(reinterpret_cast<VkDevice>(0x1u),
                                    reinterpret_cast<VkDeviceMemory>(0x2u), &handle),
           VK_ERROR_EXTENSION_NOT_PRESENT);
  CHECK_EQ(rig.kmt().commands(), sent);
  /* And the extension list the probe reads to decide whether to ask at all,
   * which the same loader answers the same way. */
  std::uint32_t count = 9u;
  CHECK_EQ(bare.enumerate_device_extensions(physical, &count, nullptr), VK_ERROR_EXTENSION_NOT_PRESENT);
  CHECK_EQ(rig.kmt().commands(), sent);
  /* And the driver's own NTSTATUS, which a probe reports because its own Kmt
   * never made the call: 0 from a loader that has no such function, and the
   * ICD's own once it does. */
  CHECK_EQ(bare.last_kernel_status(VK_NULL_HANDLE), 0u);
  LoaderVulkan icd(&av::bound_icd_instance_proc_addr);
  VkInstance   own = VK_NULL_HANDLE;
  const VkDevice device = loader_device(rig.host(), icd, own);
  CHECK_EQ(icd.last_kernel_status(device), rig.kmt().last_code());
  icd.destroy_device(device);
  icd.destroy_instance(own);
  rig.destroy_instance(instance); }

/* The sharing steps' three entry points through a loader that has them and one
 * that does not. A driver's NTSTATUS is the thing a guest report turns on, so a
 * loader that answers no such function and one whose function fails must both
 * read as zero rather than as whatever was left in the caller's variable
 * (feature 053). */
TEST(loader_vulkan, the_sharing_entry_points_are_answered_or_absent_and_never_called_through_null)
{ IcdRig rig;
  const VkInstance           instance = rig.create_instance();
  const VkPhysicalDevice     physical = rig.physical_device(instance);

  /* No such function: zero, and nothing called. */
  stub_calls = 0u;
  LoaderVulkan absent(&without_sharing);
  VkInstance   bare_instance = VK_NULL_HANDLE;
  const VkDevice bare_device = loader_device(rig.host(), absent, bare_instance);
  CHECK_EQ(absent.last_kernel_status(bare_device), 0u);
  CHECK_EQ(stub_calls, 0u);
  absent.destroy_device(bare_device);
  absent.destroy_instance(bare_instance);

  /* A function that fails: still zero, because a status the call refused to
   * stand behind is not one a report may carry. The create is what resolves
   * the table the two calls below go through. */
  LoaderVulkan present(&with_sharing);
  VkInstance   own = VK_NULL_HANDLE;
  const VkDevice own_device = loader_device(rig.host(), present, own);
  CHECK_EQ(present.last_kernel_status(own_device), 0u);
  CHECK_EQ(stub_calls, 1u);

  /* And the two the probe resolves by name, which the wrapper must call rather
   * than answer EXTENSION_NOT_PRESENT for. */
  const std::uint32_t sent = rig.kmt().commands();
  std::uint32_t       count = 0u;
  CHECK_EQ(present.enumerate_device_extensions(physical, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 3u);
  CHECK_EQ(stub_calls, 2u);
  void *handle = nullptr;
  CHECK_EQ(present.memory_win32_handle(reinterpret_cast<VkDevice>(0x1u),
                                       reinterpret_cast<VkDeviceMemory>(0x2u), &handle),
           VK_SUCCESS);
  CHECK(handle == reinterpret_cast<void *>(0x5150u));
  CHECK_EQ(stub_calls, 3u);
  /* Neither stub went near the kernel. */
  CHECK_EQ(rig.kmt().commands(), sent);
  present.destroy_device(own_device);
  present.destroy_instance(own);
  rig.destroy_instance(instance); }
