#include "executor/vulkan/vulkan_backend.hpp"

#include "core/log/log_line.hpp"
#include "executor/vulkan/low_priority_queues.hpp"
#include "executor/vulkan/vulkan_robust_access.hpp"
#include "executor/vulkan/vulkan_timeline_feature.hpp"
#include "executor/source_memory.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/sharing/shared_image_shape.hpp"

#include "core/log/driver_log.hpp"
#include "core/support/untestable.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/submission/blob_role.hpp"
#include "device/abi/av_executor.h"
#include "executor/host_gpu.hpp"
#include "executor/import_table.hpp"
#include "executor/owned_fd.hpp"
#include "proto/vulkan/descriptor_template_layout.hpp"
#include "proto/generated/vulkan/vk_command_extensions.h"
#include "proto/generated/vulkan/vk_command_names.h"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {
constexpr VulkanObjectKind release_order[] = {VulkanObjectKind::device, VulkanObjectKind::instance};

constexpr VulkanObjectKind object_kind(vkproto::HandleKind kind)
{
    switch (kind) {
    case vkproto::HandleKind::instance: return VulkanObjectKind::instance;
    case vkproto::HandleKind::physical_device: return VulkanObjectKind::physical_device;
    case vkproto::HandleKind::device: return VulkanObjectKind::device;
    case vkproto::HandleKind::queue: return VulkanObjectKind::queue;
    case vkproto::HandleKind::command_buffer: return VulkanObjectKind::command_buffer;
    default: return VulkanObjectKind::other;
    }
}

template <typename Slot>
uint64_t extension_mask(const char *const *names, uint32_t count, Slot slot_of)
{
    AV_SCOPE("extension_mask");
    uint64_t mask = 0u;
    for (uint32_t i = 0; names != nullptr && i < count; ++i) {
        if (names[i] == nullptr) continue;
        const uint16_t slot = slot_of(names[i]);
        if (slot != vkproto::kNoExtensionSlot) mask |= uint64_t{1} << slot;
    }
    return mask;
}

constexpr const char *const withheld_extensions[] = {
    "VK_KHR_acceleration_structure", "VK_KHR_ray_tracing_pipeline", "VK_KHR_ray_query",
    "VK_KHR_ray_tracing_maintenance1", "VK_EXT_pipeline_library_group_handles", "VK_KHR_deferred_host_operations",
    "VK_EXT_descriptor_buffer",
    "VK_EXT_device_generated_commands"};

bool names_withheld(const char *const *names, uint32_t count)
{
    AV_SCOPE("names_withheld");
    for (uint32_t index = 0u; names != nullptr && index < count; index++)
        for (const char *const withheld : withheld_extensions)
            if (names[index] != nullptr && __builtin_strcmp(names[index], withheld) == 0) return true;
    return false;
}

bool enables(const VkDeviceCreateInfo *create, const char *name)
{
    AV_SCOPE("enables");
    for (uint32_t at = 0u; at < create->enabledExtensionCount; at++) {
        const char *enabled = create->ppEnabledExtensionNames[at];
        uint32_t    index = 0u;
        while (enabled[index] != '\0' && enabled[index] == name[index]) index++;
        if (enabled[index] == name[index]) return true;
    }
    return false;
}

}

VulkanBackend::VulkanBackend(DriverLog &lines) : VulkanBackend(heap_, lines)
{
    AV_SCOPE("VulkanBackend::VulkanBackend");
}

VulkanBackend::VulkanBackend(BlockSource &blocks, DriverLog &lines)
    : heap_(), blocks_(blocks), gpu_(nullptr), source_(nullptr), listed_(), extensions_(blocks, Pool::paged),
      application_(), devices_(blocks), adopting_(nullptr), physical_(VK_NULL_HANDLE),
      memory_import_(false), handles_(blocks), buffer_device_(), imports_(nullptr), types_(), types_learnt_(false), timing_(lines),
      templates_(blocks), layouts_(blocks), families_(blocks), host_waits_(blocks), park_clock_(), parked_(blocks, *this, park_clock_), frame_(nullptr, 0u),
      frame_command_(0u), framed_(false), replaying_(false), last_submit_(VK_SUCCESS), lines_(lines), fence_watch_(), spirv_(), issued_(), copies_(blocks), vram_(blocks), progress_(blocks),
      timeline_submit_(blocks), fence_targets_(blocks), guard_(blocks, *this, progress_), robust_(), parent_(0u), created_extensions_(0u), ran_(0u), lost_(false), devices_leaked_(false)
{
    AV_SCOPE("VulkanBackend::VulkanBackend");
}

void VulkanBackend::initialize(const HostGpu &gpu, HostMemorySource *source, ImportTable &imports)
{
    AV_SCOPE("VulkanBackend::initialize");
    gpu_ = &gpu;
    source_ = source;
    imports_ = &imports;
}

void VulkanBackend::use_vram(ByteLedger *vram, ByteLedger *host)
{
    AV_SCOPE("VulkanBackend::use_vram");
    vram_.bind(vram, host);
}

VkPhysicalDevice VulkanBackend::bound_device(VkInstance instance)
{
    AV_SCOPE("VulkanBackend::bound_device");
    return gpu_ != nullptr ? listed_.find(instance, *gpu_) : VK_NULL_HANDLE;
}

VkResult VulkanBackend::on_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount,
                                                      VkPhysicalDevice *pPhysicalDevices)
{
    AV_SCOPE("VulkanBackend::on_vkEnumeratePhysicalDevices");
    const VkPhysicalDevice bound = bound_device(instance);
    const uint32_t         available = bound != VK_NULL_HANDLE ? 1u : 0u;
    if (pPhysicalDevices == nullptr) {
        *pPhysicalDeviceCount = available;
        return VK_SUCCESS;
    }
    if (*pPhysicalDeviceCount < available) return VK_INCOMPLETE;
    *pPhysicalDeviceCount = available;
    if (available != 0u) pPhysicalDevices[0] = bound;
    return VK_SUCCESS;
}

VkResult VulkanBackend::on_vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
                                                           VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
{
    AV_SCOPE("VulkanBackend::on_vkEnumeratePhysicalDeviceGroups");
    const VkPhysicalDevice bound = bound_device(instance);
    const uint32_t         available = bound != VK_NULL_HANDLE ? 1u : 0u;
    if (pPhysicalDeviceGroupProperties == nullptr) {
        *pPhysicalDeviceGroupCount = available;
        return VK_SUCCESS;
    }
    if (*pPhysicalDeviceGroupCount < available) return VK_INCOMPLETE;
    *pPhysicalDeviceGroupCount = available;
    if (available == 0u) return VK_SUCCESS;
    pPhysicalDeviceGroupProperties[0].physicalDeviceCount = 1u;
    pPhysicalDeviceGroupProperties[0].physicalDevices[0] = bound;
    pPhysicalDeviceGroupProperties[0].subsetAllocation = VK_FALSE;
    return VK_SUCCESS;
}

VkResult VulkanBackend::on_vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
                                                              VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
{
    AV_SCOPE("VulkanBackend::on_vkEnumeratePhysicalDeviceGroupsKHR");
    return on_vkEnumeratePhysicalDeviceGroups(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
}

bool VulkanBackend::room_for(uint64_t handles)
{
    AV_SCOPE("VulkanBackend::room_for");
    return handles_.make_room(handles) == Status::ok;
}

void VulkanBackend::made(vkproto::HandleKind kind, uint64_t handle)
{
    AV_SCOPE("VulkanBackend::made");
    const bool extended = kind == vkproto::HandleKind::instance || kind == vkproto::HandleKind::device;
    keep(handle, object_kind(kind),
         kind == vkproto::HandleKind::command_buffer ? families_.flags_of(parent_) : extended ? created_extensions_ : 0u);
    if (extended) created_extensions_ = 0u;
}

bool VulkanBackend::handle_for(vkproto::HandleKind kind, uint64_t wire, uint64_t &handle) const
{
    AV_SCOPE("VulkanBackend::handle_for");
    const VulkanObjectKind judged = object_kind(kind);
    const bool named = judged == VulkanObjectKind::other ? knows_handle(wire) : knows_handle(wire, judged);
    if (!named) return false;
    const VulkanGuard::Needs needs = guard_.needs(kind, wire);
    if (needs.reference_) {
        const uint64_t device = needs.device_ ? device_of_buffer(wire) : 0u;
        const uint64_t parent = needs.parent_ ? handles_.parent_of(wire) : 0u;
        if (!guard_.reference(kind, wire, parent, device)) return false;
    }
    handle = wire;
    return true;
}

void VulkanBackend::keep(uint64_t raw, VulkanObjectKind kind, uint64_t bytes)
{
    AV_SCOPE("VulkanBackend::keep");
    if (raw == 0u || handles_.holds(raw)) return;
    buffer_device_ = BufferDevice{};
    if (handles_.remember(raw, kind, parent_, bytes) != Status::ok) {
        AV_UNTESTABLE("room for every handle a reply carries is made before the driver is called");
    }
}

void VulkanBackend::made_through(uint64_t parent)
{
    AV_SCOPE("VulkanBackend::made_through");
    parent_ = parent;
}

void VulkanBackend::ended(uint64_t raw)
{
    AV_SCOPE("VulkanBackend::ended");
    copies_.forget(raw);
    templates_.forget(raw);
    layouts_.forget(raw);
    handles_.forget(raw);
    buffer_device_ = BufferDevice{};
}

void VulkanBackend::children_ended(uint64_t raw)
{
    AV_SCOPE("VulkanBackend::children_ended");
    handles_.forget_children(raw);
    buffer_device_ = BufferDevice{};
}

bool VulkanBackend::knows_handle(uint64_t raw) const
{
    AV_SCOPE("VulkanBackend::knows_handle");
    return raw == 0u || handles_.holds(raw);
}

bool VulkanBackend::knows_handle(uint64_t raw, VulkanObjectKind kind) const
{
    AV_SCOPE("VulkanBackend::knows_handle");
    return handles_.find(raw, kind) != nullptr;
}

Status VulkanBackend::enable_identity(VkInstanceCreateInfo &create)
{
    AV_SCOPE("VulkanBackend::enable_identity");
    const Status taken = extensions_.take(create.ppEnabledExtensionNames, create.enabledExtensionCount, 1u);
    if (taken == Status::invalid_parameter) return Status::ok;
    if (taken != Status::ok) return taken;
    static_cast<void>(extensions_.add(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME));
    create.ppEnabledExtensionNames = extensions_.names();
    create.enabledExtensionCount = extensions_.count();
    return Status::ok;
}

void VulkanBackend::adopt_host_version(VkInstanceCreateInfo &create)
{
    AV_SCOPE("VulkanBackend::adopt_host_version");
    uint32_t host_version = lowest_api_version;
    if (::vkEnumerateInstanceVersion(&host_version) != VK_SUCCESS) return;
    application_ = create.pApplicationInfo != nullptr ? *create.pApplicationInfo : VkApplicationInfo{};
    application_.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    if (application_.apiVersion >= host_version) return;
    application_.apiVersion = host_version;
    create.pApplicationInfo = &application_;
}

Status VulkanBackend::enable_memory_import(VkDeviceCreateInfo &create, uint32_t &importing,
                                           const char *priority_extension)
{
    AV_SCOPE("VulkanBackend::enable_memory_import");
    constexpr uint32_t added = 5u;
    importing = create.enabledExtensionCount;
    const Status taken = extensions_.take(create.ppEnabledExtensionNames, create.enabledExtensionCount, added);
    if (taken == Status::invalid_parameter) return Status::ok;
    if (taken != Status::ok) return taken;
    static_cast<void>(extensions_.add(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME));
    static_cast<void>(extensions_.add(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME));
    static_cast<void>(extensions_.add(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME));
    if (priority_extension != nullptr) static_cast<void>(extensions_.add(priority_extension));
    importing = extensions_.count();
    static_cast<void>(extensions_.add(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME));
    create.ppEnabledExtensionNames = extensions_.names();
    create.enabledExtensionCount = extensions_.count();
    return Status::ok;
}

VkResult VulkanBackend::on_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *, VkInstance *pInstance)
{
    AV_SCOPE("VulkanBackend::on_vkCreateInstance");
    if (pCreateInfo->pApplicationInfo != nullptr && pCreateInfo->pApplicationInfo->apiVersion != 0u &&
        pCreateInfo->pApplicationInfo->apiVersion < lowest_api_version)
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    created_extensions_ = extension_mask(pCreateInfo->ppEnabledExtensionNames, pCreateInfo->enabledExtensionCount,
                                         [](const char *name) { return vkproto::instance_extension_slot(name); });
    VkInstanceCreateInfo create = *pCreateInfo;
    if (enable_identity(create) != Status::ok) return VK_ERROR_OUT_OF_HOST_MEMORY;
    adopt_host_version(create);
    return RealVulkanDispatch::on_vkCreateInstance(&create, nullptr, pInstance);
}

VkResult VulkanBackend::on_vkCreateDescriptorUpdateTemplate(VkDevice device,
                                                            const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                                            const VkAllocationCallbacks *pAllocator,
                                                            VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
    AV_SCOPE("VulkanBackend::on_vkCreateDescriptorUpdateTemplate");
    DescriptorTemplateLayout layout;
    if (layout.take(pCreateInfo->pDescriptorUpdateEntries, pCreateInfo->descriptorUpdateEntryCount) != Status::ok)
        return VK_ERROR_INITIALIZATION_FAILED;
    const VkResult result = RealVulkanDispatch::on_vkCreateDescriptorUpdateTemplate(device, pCreateInfo, pAllocator,
                                                                                    pDescriptorUpdateTemplate);
    const uint64_t made = reinterpret_cast<uint64_t>(*pDescriptorUpdateTemplate);
    uint32_t       fixed = 0u;
    const uint32_t *const immutable = layouts_.immutable(
        pCreateInfo->templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET
            ? reinterpret_cast<uint64_t>(pCreateInfo->descriptorSetLayout)
            : layouts_.layout_of(reinterpret_cast<uint64_t>(pCreateInfo->pipelineLayout), pCreateInfo->set),
        fixed);
    if (result == VK_SUCCESS &&
        templates_.take(made, reinterpret_cast<uint64_t>(device), pCreateInfo->pDescriptorUpdateEntries,
                        pCreateInfo->descriptorUpdateEntryCount, immutable, fixed) != Status::ok) {
        RealVulkanDispatch::on_vkDestroyDescriptorUpdateTemplate(device, *pDescriptorUpdateTemplate, nullptr);
        *pDescriptorUpdateTemplate = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    handles_.forget(made);
    made_through(reinterpret_cast<uint64_t>(device));
    keep(made, VulkanObjectKind::descriptor_template, layout.bytes());
    return result;
}

VkResult VulkanBackend::on_vkCreateDescriptorUpdateTemplateKHR(VkDevice device,
                                                               const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                                               const VkAllocationCallbacks *pAllocator,
                                                               VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
    AV_SCOPE("VulkanBackend::on_vkCreateDescriptorUpdateTemplateKHR");
    return on_vkCreateDescriptorUpdateTemplate(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
}

VkResult VulkanBackend::on_vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                       const VkAllocationCallbacks *, VkDescriptorSetLayout *pSetLayout)
{
    AV_SCOPE("VulkanBackend::on_vkCreateDescriptorSetLayout");
    const VkResult made = RealVulkanDispatch::on_vkCreateDescriptorSetLayout(device, pCreateInfo, nullptr, pSetLayout);
    if (made != VK_SUCCESS || layouts_.take_layout(reinterpret_cast<uint64_t>(*pSetLayout), reinterpret_cast<uint64_t>(device),
                                                   pCreateInfo->pBindings, pCreateInfo->bindingCount) == Status::ok)
        return made;
    RealVulkanDispatch::on_vkDestroyDescriptorSetLayout(device, *pSetLayout, nullptr);
    *pSetLayout = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

VkResult VulkanBackend::on_vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                  const VkAllocationCallbacks *, VkPipelineLayout *pPipelineLayout)
{
    AV_SCOPE("VulkanBackend::on_vkCreatePipelineLayout");
    const VkResult made = RealVulkanDispatch::on_vkCreatePipelineLayout(device, pCreateInfo, nullptr, pPipelineLayout);
    if (made != VK_SUCCESS || layouts_.take_pipeline(reinterpret_cast<uint64_t>(*pPipelineLayout), reinterpret_cast<uint64_t>(device),
                                                     pCreateInfo->pSetLayouts, pCreateInfo->setLayoutCount) == Status::ok)
        return made;
    RealVulkanDispatch::on_vkDestroyPipelineLayout(device, *pPipelineLayout, nullptr);
    *pPipelineLayout = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

bool VulkanBackend::image_copy_bytes(VkImage image, const VkImageSubresourceLayers &layers, VkExtent3D extent,
                                     uint32_t row_length, uint32_t image_height, uint64_t &bytes) const
{
    AV_SCOPE("VulkanBackend::image_copy_bytes");
    return copies_.bytes(reinterpret_cast<uint64_t>(image), layers, extent, row_length, image_height, bytes);
}

uint64_t VulkanBackend::template_bytes(VkDescriptorUpdateTemplate made) const
{
    AV_SCOPE("VulkanBackend::template_bytes");
    const uint64_t *const bytes =
        handles_.find(reinterpret_cast<uint64_t>(made), VulkanObjectKind::descriptor_template);
    return bytes != nullptr ? *bytes : 0u;
}

bool VulkanBackend::knows(uint32_t command_id) const
{
    AV_SCOPE("VulkanBackend::knows");
    return vkproto::knows_command(command_id);
}

bool VulkanBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    AV_SCOPE("VulkanBackend::run");
    const char *const name = vkproto::cmd_id_to_string(command_id);
    const bool        recording = name[0] == 'v' && name[1] == 'k' && name[2] == 'C' && name[3] == 'm' && name[4] == 'd';
    if (recording && !queue_admits(command_id, payload)) {
        lines_.note_hex("a command its command buffer's queue family cannot record, id", command_id, 8u);
        lost_ = true;
        return true;
    }
    if (command_refused(command_id)) {
        lines_.note_hex("a command that hands the host driver a guest pointer or a captured address, id", command_id, 8u);
        lost_ = true;
        return true;
    }
    if (!extension_admits(command_id, payload)) {
        lines_.note_hex("a command whose extension its device or instance did not enable, id", command_id, 8u);
        lost_ = true;
        return true;
    }
    if (!recording) parked_.service();
    if (lost_) return true;
    guard_.begin_frame(recording);
    if (!recording) guard_.reap();
    frame_ = payload;
    frame_command_ = command_id;
    framed_ = true;
    const bool decoded = vkproto::dispatch_command(*this, command_id, payload, reply, arena);
    framed_ = false;
    parent_ = 0u;
    if (!decoded) return false;
    ran_++;
    return true;
}

bool VulkanBackend::queue_admits(uint32_t command_id, const WireReader &payload) const
{
    AV_SCOPE("VulkanBackend::queue_admits");
    const uint32_t queues = vkproto::command_queues(command_id);
    if (queues == 0u) return true;
    WireReader peek = payload;
    uint64_t   buffer = 0u;
    if (!peek.read_pod(buffer)) return true;
    const uint64_t *const flags = handles_.find(buffer, VulkanObjectKind::command_buffer);
    return flags == nullptr || (*flags & queues) != 0u;
}

bool VulkanBackend::command_refused(uint32_t command_id) const
{
    AV_SCOPE("VulkanBackend::command_refused");
    switch (command_id) {
    case vkproto::kCmd_vkBuildAccelerationStructuresKHR:
    case vkproto::kCmd_vkCopyAccelerationStructureKHR:
    case vkproto::kCmd_vkCopyAccelerationStructureToMemoryKHR:
    case vkproto::kCmd_vkCopyMemoryToAccelerationStructureKHR:
    case vkproto::kCmd_vkWriteAccelerationStructuresPropertiesKHR:
    case vkproto::kCmd_vkGetBufferOpaqueCaptureAddress:
    case vkproto::kCmd_vkGetBufferOpaqueCaptureAddressKHR:
    case vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddress:
    case vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddressKHR: return true;
    default: return false;
    }
}

bool VulkanBackend::extension_admits(uint32_t command_id, const WireReader &payload) const
{
    AV_SCOPE("VulkanBackend::extension_admits");
    const vkproto::CommandExtensions needed = vkproto::command_extensions(command_id);
    if (needed.owner == vkproto::CommandOwner::open) return true;
    WireReader peek = payload;
    uint64_t   at = 0u;
    if (!peek.read_pod(at)) return true;
    const VulkanObjectKind owner =
        needed.owner == vkproto::CommandOwner::device ? VulkanObjectKind::device : VulkanObjectKind::instance;
    while (at != 0u && handles_.find(at, owner) == nullptr) at = handles_.parent_of(at);
    if (at == 0u) return true;
    uint64_t wanted = 0u;
    for (uint16_t i = 0; i < needed.count; ++i) wanted |= uint64_t{1} << needed.slots[i];
    return (*handles_.find(at, owner) & wanted) != 0u;
}

bool VulkanBackend::family_flags(VkDevice device, uint32_t family, uint32_t &flags)
{
    AV_SCOPE("VulkanBackend::family_flags");
    const auto physical = reinterpret_cast<VkPhysicalDevice>(handles_.parent_of(reinterpret_cast<uint64_t>(device)));
    if (physical == VK_NULL_HANDLE) return false;
    VkQueueFamilyProperties families[max_queue_families] = {};
    uint32_t                count = max_queue_families;
    RealVulkanDispatch::on_vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families);
    if (family >= count) return false;
    flags = families[family].queueFlags;
    return true;
}

VkResult VulkanBackend::on_vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *, VkCommandPool *pCommandPool)
{
    AV_SCOPE("VulkanBackend::on_vkCreateCommandPool");
    uint32_t flags = 0u;
    if (!family_flags(device, pCreateInfo->queueFamilyIndex, flags)) return VK_ERROR_INITIALIZATION_FAILED;
    const VkResult made = RealVulkanDispatch::on_vkCreateCommandPool(device, pCreateInfo, nullptr, pCommandPool);
    if (made != VK_SUCCESS) return made;
    if (families_.record(reinterpret_cast<uint64_t>(*pCommandPool), flags) == Status::ok) return made;
    RealVulkanDispatch::on_vkDestroyCommandPool(device, *pCommandPool, nullptr);
    *pCommandPool = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

VkResult VulkanBackend::create_host_device(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
                                           const char *priority_extension, VkDevice *pDevice)
{
    AV_SCOPE("VulkanBackend::create_host_device");
    VkDeviceCreateInfo create = *pCreateInfo;
    uint32_t           importing = 0u;
    if (enable_memory_import(create, importing, priority_extension) != Status::ok) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkResult result = RealVulkanDispatch::on_vkCreateDevice(physicalDevice, &create, nullptr, pDevice);
    if (result == VK_ERROR_EXTENSION_NOT_PRESENT && importing != create.enabledExtensionCount) {
        create.enabledExtensionCount = importing;
        result = RealVulkanDispatch::on_vkCreateDevice(physicalDevice, &create, nullptr, pDevice);
    }
    if (result == VK_ERROR_EXTENSION_NOT_PRESENT && priority_extension == nullptr &&
        create.enabledExtensionCount != pCreateInfo->enabledExtensionCount)
        result = RealVulkanDispatch::on_vkCreateDevice(physicalDevice, pCreateInfo, nullptr, pDevice);
    return result;
}

VkResult VulkanBackend::create_lowered(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *base, VkDevice *pDevice)
{
    AV_SCOPE("VulkanBackend::create_lowered");
    LowPriorityQueues lowered;
    VkDeviceCreateInfo prioritised = *base;
    VkResult           result = VK_ERROR_INITIALIZATION_FAILED;
    const bool         lowers = lowered.offered_by(physicalDevice) && lowered.apply(prioritised);
    if (lowers) result = create_host_device(physicalDevice, &prioritised, lowered.extension(), pDevice);
    if (!lowers || result != VK_SUCCESS) result = create_host_device(physicalDevice, base, nullptr, pDevice);
    return result;
}

void VulkanBackend::on_vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *pFeatures)
{
    AV_SCOPE("VulkanBackend::on_vkGetPhysicalDeviceFeatures2");
    RealVulkanDispatch::on_vkGetPhysicalDeviceFeatures2(physicalDevice, pFeatures);
    robust_.hide_capture_replay(pFeatures->pNext);
}

void VulkanBackend::on_vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *pFeatures)
{
    AV_SCOPE("VulkanBackend::on_vkGetPhysicalDeviceFeatures2KHR");
    RealVulkanDispatch::on_vkGetPhysicalDeviceFeatures2KHR(physicalDevice, pFeatures);
    robust_.hide_capture_replay(pFeatures->pNext);
}

VkResult VulkanBackend::on_vkCreateDevice(VkPhysicalDevice physicalDevice,
                                          const VkDeviceCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *, VkDevice *pDevice)
{
    AV_SCOPE("VulkanBackend::on_vkCreateDevice");
    if (names_withheld(pCreateInfo->ppEnabledExtensionNames, pCreateInfo->enabledExtensionCount)) {
        lines_.write("a device named an extension that hands the host driver guest-supplied addresses or binaries; refused");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    created_extensions_ = extension_mask(pCreateInfo->ppEnabledExtensionNames, pCreateInfo->enabledExtensionCount,
                                         [](const char *name) { return vkproto::device_extension_slot(name); });
    VkDeviceCreateInfo    guarded = *pCreateInfo;
    static_cast<void>(robust_.apply(physicalDevice, guarded));
    pCreateInfo = &guarded;
    VulkanTimelineFeature timeline;
    VkDeviceCreateInfo    timed = *pCreateInfo;
    bool                  timelines = timeline.offered_by(physicalDevice) && timeline.apply(timed);
    VkResult              result = VK_ERROR_INITIALIZATION_FAILED;
    if (timelines) result = create_lowered(physicalDevice, &timed, pDevice);
    if (!timelines || result != VK_SUCCESS) {
        timelines = false;
        result = create_lowered(physicalDevice, pCreateInfo, pDevice);
    }
    if (result != VK_SUCCESS) return result;
    if (timelines && !progress_.enable_timelines(*pDevice)) timelines = false;
    GuestDevice *made = nullptr;
    if (devices_.create(made) != Status::ok) {
        RealVulkanDispatch::on_vkDestroyDevice(*pDevice, nullptr);
        *pDevice = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    physical_ = physicalDevice;
    const auto instance = reinterpret_cast<VkInstance>(handles_.parent_of(reinterpret_cast<uint64_t>(physicalDevice)));
    if (made->load(instance, physicalDevice, *pDevice)) {
        adopting_ = made;
        made->note_invalid_shader_extension(enables(pCreateInfo, VK_NV_GLSL_SHADER_EXTENSION_NAME));
        memory_import_ = made->memory().imports();
    } else {
        static_cast<void>(devices_.destroy(made));
    }
    learn_types(physicalDevice);
    return result;
}

void VulkanBackend::learn_types(VkPhysicalDevice physical)
{
    AV_SCOPE("VulkanBackend::learn_types");
    if (types_learnt_) return;
    types_learnt_ = true;
    VkPhysicalDeviceMemoryProperties learnt = {};
    if (source_ != nullptr && source_->vulkan_types(types_)) {
        types_.properties(learnt);
        vram_.learn(learnt);
        return;
    }
    VkPhysicalDeviceMemoryProperties host = {};
    RealVulkanDispatch::on_vkGetPhysicalDeviceMemoryProperties(physical, &host);
    types_.learn(host, 0u);
    vram_.learn(host);
}

void VulkanBackend::on_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                                           VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
    AV_SCOPE("VulkanBackend::on_vkGetPhysicalDeviceMemoryProperties");
    learn_types(physicalDevice);
    types_.properties(*pMemoryProperties);
    vram_.clamp(*pMemoryProperties);
}

void VulkanBackend::on_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                                            VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
    AV_SCOPE("VulkanBackend::on_vkGetPhysicalDeviceMemoryProperties2");
    RealVulkanDispatch::on_vkGetPhysicalDeviceMemoryProperties2(physicalDevice, pMemoryProperties);
    learn_types(physicalDevice);
    types_.properties(pMemoryProperties->memoryProperties);
    vram_.clamp(pMemoryProperties->memoryProperties);
    for (auto *link = static_cast<VkBaseOutStructure *>(pMemoryProperties->pNext); link != nullptr; link = link->pNext)
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT)
            vram_.report(*reinterpret_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT *>(link));
}

void VulkanBackend::on_vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice,
                                                               VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
    AV_SCOPE("VulkanBackend::on_vkGetPhysicalDeviceMemoryProperties2KHR");
    on_vkGetPhysicalDeviceMemoryProperties2(physicalDevice, pMemoryProperties);
}

bool VulkanBackend::backs_with_source(VkDevice device, const void *next, VkStructureType external,
                                      VkExternalMemoryHandleTypeFlags types) const
{
    AV_SCOPE("VulkanBackend::backs_with_source");
    if (!memory_import_ || adopting_ == nullptr || device != adopting_->device() || types == 0u) return false;
    for (auto *structure = static_cast<const VkBaseInStructure *>(next); structure != nullptr;
         structure = structure->pNext) {
        if (structure->sType == external) return false;
    }
    return true;
}

void VulkanBackend::for_source(VkDevice device, VkBufferCreateInfo &create, VkExternalMemoryBufferCreateInfo &external)
{
    AV_SCOPE("VulkanBackend::for_source");
    external = {};
    external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    external.pNext = create.pNext;
    if (adopting_ != nullptr)
        external.handleTypes = adopting_->memory().buffer_external_types(physical_, create,
                                                                         VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
    if (backs_with_source(device, create.pNext, external.sType, external.handleTypes)) create.pNext = &external;
}

bool VulkanBackend::for_source(VkDevice device, VkImageCreateInfo &create, VkExternalMemoryImageCreateInfo &external)
{
    AV_SCOPE("VulkanBackend::for_source");
    external = {};
    external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    external.pNext = create.pNext;
    if (adopting_ != nullptr && create.tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
        external.handleTypes = adopting_->memory().image_external_types(physical_, create,
                                                                        VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
    if (backs_with_source(device, create.pNext, external.sType, external.handleTypes)) create.pNext = &external;
    for (auto *node = static_cast<const VkBaseInStructure *>(create.pNext); node != nullptr; node = node->pNext) {
        if (node->sType != external.sType) continue;
        const auto *named = reinterpret_cast<const VkExternalMemoryImageCreateInfo *>(node);
        return (named->handleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT) != 0u;
    }
    return false;
}

VkResult VulkanBackend::on_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *, VkBuffer *pBuffer)
{
    AV_SCOPE("VulkanBackend::on_vkCreateBuffer");
    if ((pCreateInfo->flags & VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT) != 0u) {
        lines_.write("a buffer asked to be placed at a captured address; refused");
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    VkBufferCreateInfo               create = *pCreateInfo;
    VkExternalMemoryBufferCreateInfo external;
    for_source(device, create, external);
    return RealVulkanDispatch::on_vkCreateBuffer(device, &create, nullptr, pBuffer);
}

VkResult VulkanBackend::on_vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                                         const VkAllocationCallbacks *, VkImage *pImage)
{
    AV_SCOPE("VulkanBackend::on_vkCreateImage");
    VkImageCreateInfo               create = *pCreateInfo;
    VkExternalMemoryImageCreateInfo external;
    const bool                      bindable = for_source(device, create, external);
    const VkResult result = RealVulkanDispatch::on_vkCreateImage(device, &create, nullptr, pImage);
    if (result == VK_SUCCESS && !copies_.record(reinterpret_cast<uint64_t>(*pImage), reinterpret_cast<uint64_t>(device), create)) {
        RealVulkanDispatch::on_vkDestroyImage(device, *pImage, nullptr);
        *pImage = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    if (bindable) {
        made_through(reinterpret_cast<uint64_t>(device));
        keep(reinterpret_cast<uint64_t>(*pImage), VulkanObjectKind::external_image, 0u);
    }
    return result;
}

uint32_t VulkanBackend::image_bits(uint32_t host_bits, bool external) const
{
    AV_SCOPE("VulkanBackend::image_bits");
    const uint32_t bits = types_.guest_bits(host_bits);
    return external ? bits : types_.unmapped(bits);
}

void VulkanBackend::on_vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer,
                                                     VkMemoryRequirements *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetBufferMemoryRequirements");
    RealVulkanDispatch::on_vkGetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
    pMemoryRequirements->memoryTypeBits = types_.guest_bits(pMemoryRequirements->memoryTypeBits);
}

void VulkanBackend::on_vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo,
                                                      VkMemoryRequirements2 *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetBufferMemoryRequirements2");
    RealVulkanDispatch::on_vkGetBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
    VkMemoryRequirements &needs = pMemoryRequirements->memoryRequirements;
    needs.memoryTypeBits = types_.guest_bits(needs.memoryTypeBits);
}

void VulkanBackend::on_vkGetBufferMemoryRequirements2KHR(VkDevice device,
                                                         const VkBufferMemoryRequirementsInfo2 *pInfo,
                                                         VkMemoryRequirements2 *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetBufferMemoryRequirements2KHR");
    on_vkGetBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
}

void VulkanBackend::on_vkGetImageMemoryRequirements(VkDevice device, VkImage image,
                                                    VkMemoryRequirements *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetImageMemoryRequirements");
    RealVulkanDispatch::on_vkGetImageMemoryRequirements(device, image, pMemoryRequirements);
    pMemoryRequirements->memoryTypeBits =
        image_bits(pMemoryRequirements->memoryTypeBits,
                   knows_handle(reinterpret_cast<uint64_t>(image), VulkanObjectKind::external_image));
}

void VulkanBackend::on_vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo,
                                                     VkMemoryRequirements2 *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetImageMemoryRequirements2");
    RealVulkanDispatch::on_vkGetImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
    VkMemoryRequirements &needs = pMemoryRequirements->memoryRequirements;
    needs.memoryTypeBits = image_bits(needs.memoryTypeBits,
                                      knows_handle(reinterpret_cast<uint64_t>(pInfo->image),
                                                   VulkanObjectKind::external_image));
}

void VulkanBackend::on_vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo,
                                                        VkMemoryRequirements2 *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetImageMemoryRequirements2KHR");
    on_vkGetImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
}

void VulkanBackend::on_vkGetDeviceBufferMemoryRequirements(VkDevice device,
                                                           const VkDeviceBufferMemoryRequirements *pInfo,
                                                           VkMemoryRequirements2 *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetDeviceBufferMemoryRequirements");
    VkDeviceBufferMemoryRequirements asked = *pInfo;
    VkBufferCreateInfo               create = *pInfo->pCreateInfo;
    VkExternalMemoryBufferCreateInfo external;
    for_source(device, create, external);
    asked.pCreateInfo = &create;
    RealVulkanDispatch::on_vkGetDeviceBufferMemoryRequirements(device, &asked, pMemoryRequirements);
    VkMemoryRequirements &needs = pMemoryRequirements->memoryRequirements;
    needs.memoryTypeBits = types_.guest_bits(needs.memoryTypeBits);
}

void VulkanBackend::on_vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device,
                                                              const VkDeviceBufferMemoryRequirements *pInfo,
                                                              VkMemoryRequirements2 *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetDeviceBufferMemoryRequirementsKHR");
    on_vkGetDeviceBufferMemoryRequirements(device, pInfo, pMemoryRequirements);
}

void VulkanBackend::on_vkGetDeviceImageMemoryRequirements(VkDevice device,
                                                          const VkDeviceImageMemoryRequirements *pInfo,
                                                          VkMemoryRequirements2 *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetDeviceImageMemoryRequirements");
    VkDeviceImageMemoryRequirements asked = *pInfo;
    VkImageCreateInfo               create = *pInfo->pCreateInfo;
    VkExternalMemoryImageCreateInfo external;
    const bool                      bindable = for_source(device, create, external);
    asked.pCreateInfo = &create;
    RealVulkanDispatch::on_vkGetDeviceImageMemoryRequirements(device, &asked, pMemoryRequirements);
    VkMemoryRequirements &needs = pMemoryRequirements->memoryRequirements;
    needs.memoryTypeBits = image_bits(needs.memoryTypeBits, bindable);
}

void VulkanBackend::on_vkGetDeviceImageSparseMemoryRequirements(
    VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, uint32_t *pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetDeviceImageSparseMemoryRequirements");
    VkDeviceImageMemoryRequirements asked = *pInfo;
    VkImageCreateInfo               create = *pInfo->pCreateInfo;
    VkExternalMemoryImageCreateInfo external;
    static_cast<void>(for_source(device, create, external));
    asked.pCreateInfo = &create;
    RealVulkanDispatch::on_vkGetDeviceImageSparseMemoryRequirements(device, &asked, pSparseMemoryRequirementCount,
                                                                    pSparseMemoryRequirements);
}

void VulkanBackend::on_vkGetDeviceImageSparseMemoryRequirementsKHR(
    VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, uint32_t *pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetDeviceImageSparseMemoryRequirementsKHR");
    on_vkGetDeviceImageSparseMemoryRequirements(device, pInfo, pSparseMemoryRequirementCount,
                                                pSparseMemoryRequirements);
}

void VulkanBackend::on_vkGetDeviceImageSubresourceLayoutKHR(VkDevice device, const VkDeviceImageSubresourceInfo *pInfo,
                                                            VkSubresourceLayout2 *pLayout)
{
    AV_SCOPE("VulkanBackend::on_vkGetDeviceImageSubresourceLayoutKHR");
    VkDeviceImageSubresourceInfo    asked = *pInfo;
    VkImageCreateInfo               create = *pInfo->pCreateInfo;
    VkExternalMemoryImageCreateInfo external;
    static_cast<void>(for_source(device, create, external));
    asked.pCreateInfo = &create;
    RealVulkanDispatch::on_vkGetDeviceImageSubresourceLayoutKHR(device, &asked, pLayout);
}

void VulkanBackend::on_vkGetDeviceImageMemoryRequirementsKHR(VkDevice device,
                                                             const VkDeviceImageMemoryRequirements *pInfo,
                                                             VkMemoryRequirements2 *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetDeviceImageMemoryRequirementsKHR");
    on_vkGetDeviceImageMemoryRequirements(device, pInfo, pMemoryRequirements);
}

void VulkanBackend::on_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex,
                                        VkQueue *pQueue)
{
    AV_SCOPE("VulkanBackend::on_vkGetDeviceQueue");
    RealVulkanDispatch::on_vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    if (timing_.enabled() && pQueue != nullptr) timing_.queue_made(physical_, device, queueFamilyIndex, *pQueue);
}

void VulkanBackend::on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyDevice");
    fence_watch_.settle();
    fence_targets_.clear();
    parked_.drop_device(reinterpret_cast<uint64_t>(device));
    const bool idle = guard_.release_all(reinterpret_cast<uint64_t>(device));
    templates_.forget_device(reinterpret_cast<uint64_t>(device));
    copies_.forget_device(reinterpret_cast<uint64_t>(device));
    layouts_.forget_device(reinterpret_cast<uint64_t>(device));
    fence_watch_.forget(device);
    progress_.forget(device);
    timing_.forget(device);
    GuestDevice *const ending = device_of(reinterpret_cast<uint64_t>(device));
    if (ending != nullptr && ending == adopting_) {
        adopting_ = nullptr;
        memory_import_ = false;
    }
    if (ending != nullptr) static_cast<void>(devices_.destroy(ending));
    if (idle) RealVulkanDispatch::on_vkDestroyDevice(device, nullptr);
    else devices_leaked_ = true;
}

void VulkanBackend::on_vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkFreeMemory");
    const uint64_t            handle = reinterpret_cast<uint64_t>(memory);
    const SourceMemory *const imported = imports_ != nullptr ? imports_->named(handle) : nullptr;
    if (imported == nullptr) {
        guard_.destroy(VulkanDestroyKind::memory, reinterpret_cast<uint64_t>(device), 0u, handle);
        return;
    }
    guard_.destroy(VulkanDestroyKind::imported_memory, reinterpret_cast<uint64_t>(device), 0u, handle);
    imports_->unname(handle);
}

VkResult VulkanBackend::on_vkGetMemoryFdKHR(VkDevice, const VkMemoryGetFdInfoKHR *, int *pFd)
{
    AV_SCOPE("VulkanBackend::on_vkGetMemoryFdKHR");
    *pFd = -1;
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VkResult VulkanBackend::on_vkGetSemaphoreFdKHR(VkDevice, const VkSemaphoreGetFdInfoKHR *, int *pFd)
{
    AV_SCOPE("VulkanBackend::on_vkGetSemaphoreFdKHR");
    *pFd = -1;
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VkResult VulkanBackend::on_vkGetFenceFdKHR(VkDevice, const VkFenceGetFdInfoKHR *, int *pFd)
{
    AV_SCOPE("VulkanBackend::on_vkGetFenceFdKHR");
    *pFd = -1;
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VkResult VulkanBackend::on_vkGetMemoryFdPropertiesKHR(VkDevice, VkExternalMemoryHandleTypeFlagBits, int,
                                                      VkMemoryFdPropertiesKHR *)
{
    AV_SCOPE("VulkanBackend::on_vkGetMemoryFdPropertiesKHR");
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VkResult VulkanBackend::on_vkImportSemaphoreFdKHR(VkDevice, const VkImportSemaphoreFdInfoKHR *)
{
    AV_SCOPE("VulkanBackend::on_vkImportSemaphoreFdKHR");
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VkResult VulkanBackend::on_vkImportFenceFdKHR(VkDevice, const VkImportFenceFdInfoKHR *)
{
    AV_SCOPE("VulkanBackend::on_vkImportFenceFdKHR");
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VkResult VulkanBackend::on_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                           const VkAllocationCallbacks *, VkDeviceMemory *pMemory)
{
    AV_SCOPE("VulkanBackend::on_vkAllocateMemory");
    if (!imports_nothing(pAllocateInfo->pNext)) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    for (auto *node = static_cast<const VkBaseInStructure *>(pAllocateInfo->pNext); node != nullptr; node = node->pNext) {
        const bool replays =
            (node->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO &&
             (reinterpret_cast<const VkMemoryAllocateFlagsInfo *>(node)->flags &
              VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT) != 0u) ||
            (node->sType == VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO &&
             reinterpret_cast<const VkMemoryOpaqueCaptureAddressAllocateInfo *>(node)->opaqueCaptureAddress != 0u);
        if (!replays) continue;
        lines_.write("a memory allocation asked to be placed at a captured address; refused");
        return VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS;
    }
    VkMemoryAllocateInfo allocate = *pAllocateInfo;
    allocate.memoryTypeIndex = types_.host_type(pAllocateInfo->memoryTypeIndex);
    if (allocate.memoryTypeIndex == GuestMemoryTypes::no_type) {
        LogLine(lines_).text("a memory allocation names guest memory type ").number(pAllocateInfo->memoryTypeIndex).text(" the host has no type for").write();
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    const uint32_t heap = types_.heap_of(pAllocateInfo->memoryTypeIndex);
    if (!vram_.admit(heap, pAllocateInfo->allocationSize)) {
        LogLine(lines_).text("a memory allocation of ").number(pAllocateInfo->allocationSize).text(" bytes in heap ").number(heap).text(" refused by the ").text(vram_.system(heap) ? "host memory" : "video memory").text(" budget").write();
        return vram_.system(heap) ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    const VkResult made = RealVulkanDispatch::on_vkAllocateMemory(device, &allocate, nullptr, pMemory);
    if (made != VK_SUCCESS) {
        LogLine(lines_).text("the host driver refused an allocation of ").number(pAllocateInfo->allocationSize).text(" bytes in host type ").number(allocate.memoryTypeIndex).text(": ").number(static_cast<uint64_t>(static_cast<int64_t>(made))).write();
        vram_.refund(heap, pAllocateInfo->allocationSize);
        return made;
    }
    if (vram_.commit(heap, reinterpret_cast<uint64_t>(*pMemory), pAllocateInfo->allocationSize) == Status::ok) return made;
    vram_.refund(heap, pAllocateInfo->allocationSize);
    RealVulkanDispatch::on_vkFreeMemory(device, *pMemory, nullptr);
    *pMemory = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

bool VulkanBackend::imports_nothing(const void *next) const
{
    AV_SCOPE("VulkanBackend::imports_nothing");
    for (const auto *node = static_cast<const VkBaseInStructure *>(next); node != nullptr; node = node->pNext)
        if (node->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR ||
            node->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT)
            return false;
    return true;
}

uint32_t VulkanBackend::import_type(const SourceMemory &memory, int fd) const
{
    AV_SCOPE("VulkanBackend::import_type");
    const uint32_t reported = adopting_->memory().dmabuf_types(fd);
    if (reported == 0u) return memory.memory_type();
    if (memory.memory_type() < max_memory_types && (reported & (1u << memory.memory_type())) != 0u)
        return memory.memory_type();
    return first_memory_type(reported);
}

bool VulkanBackend::adopt(SourceMemory &memory, const BlobRole &role, uint64_t &handle)
{
    AV_SCOPE("VulkanBackend::adopt");
    handle = 0u;
    if (role.is_arena() || adopting_ == nullptr || !memory_import_) return false;
    uint64_t made = 0u;
    uint32_t preferred = max_memory_types;
    uint32_t listed = 0u;
    {
        OwnedFd probe;
        if (!memory.share(probe)) return false;
        preferred = import_type(memory, probe.fd());
        listed = adopting_->memory().dmabuf_types(probe.fd());
    }
    bool imported = false;
    for (uint32_t pass = 0u; pass < 3u && !imported; pass++) {
        for (uint32_t type = 0u; type < max_memory_types && !imported; type++) {
            const bool wanted = pass == 0u ? type == preferred : pass == 1u ? (listed & (1u << type)) != 0u : true;
            if (!wanted || (pass != 0u && type == preferred)) continue;
            OwnedFd descriptor;
            if (!memory.share(descriptor)) return false;
            imported = adopting_->memory().import_dmabuf(descriptor, memory.size(), type, made);
        }
    }
    if (!imported) return false;
    if (!kept_on(*adopting_, made)) {
        adopting_->memory().release(made);
        return false;
    }
    handle = made;
    return true;
}

uint32_t VulkanBackend::first_memory_type(uint32_t mask) const
{
    AV_SCOPE("VulkanBackend::first_memory_type");
    for (uint32_t type = 0u; type < max_memory_types; type++)
        if ((mask & (1u << type)) != 0u) return type;
    return max_memory_types;
}

bool VulkanBackend::export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind, OwnedFd &descriptor)
{
    AV_SCOPE("VulkanBackend::export_object");
    handle_kind = 0u;
    if (wire_handle == 0u || !handles_.holds(wire_handle)) return false;
    GuestDevice *const owner = owner_of(wire_handle);
    if (owner == nullptr) return false;
    if (kind == AV_SHARED_MONITORED_FENCE || kind == AV_SHARED_NATIVE_FENCE) {
        if (!owner->memory().export_semaphore(wire_handle, descriptor)) return false;
        handle_kind = AV_SHARED_HANDLE_OPAQUE_FD;
        return true;
    }
    if (kind != AV_SHARED_MEMORY) return false;
    if (!owner->memory().export_memory(wire_handle, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, descriptor))
        return false;
    handle_kind = AV_SHARED_HANDLE_DMA_BUF;
    return true;
}

bool VulkanBackend::import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                  const SharedImageShape &shape, uint64_t into,
                                  uint64_t &wire_handle, uint64_t &detail)
{
    AV_SCOPE("VulkanBackend::import_object");
    static_cast<void>(shape);
    wire_handle = 0u;
    detail = 0u;
    if (fd < 0 || into == 0u) return false;
    if (kind == AV_SHARED_MONITORED_FENCE || kind == AV_SHARED_NATIVE_FENCE)
        return handle_kind == AV_SHARED_HANDLE_OPAQUE_FD && import_payload(fd, into, wire_handle, detail);
    if (kind != AV_SHARED_MEMORY || size == 0u || handle_kind != AV_SHARED_HANDLE_DMA_BUF) return false;
    GuestDevice *const device = device_of(into);
    return device != nullptr && import_memory(*device, fd, size, wire_handle);
}

bool VulkanBackend::import_memory(GuestDevice &device, int fd, uint64_t size, uint64_t &wire_handle)
{
    AV_SCOPE("VulkanBackend::import_memory");
    const uint32_t listed = device.memory().dmabuf_types(fd);
    bool           imported = false;
    for (uint32_t pass = 0u; pass < 2u && !imported; pass++) {
        for (uint32_t type = 0u; type < max_memory_types && !imported; type++) {
            const bool is_listed = (listed & (1u << type)) != 0u;
            if (is_listed != (pass == 0u)) continue;
            imported = device.memory().import_shared_memory(fd, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, size,
                                                            type, wire_handle);
        }
    }
    if (!imported) { AV_UNTESTABLE("no host driver here refuses an import"); return false; }
    if (kept_on(device, wire_handle)) return true;
    device.memory().release(wire_handle);
    wire_handle = 0u;
    return false;
}

bool VulkanBackend::import_payload(int fd, uint64_t into, uint64_t &wire_handle, uint64_t &detail)
{
    AV_SCOPE("VulkanBackend::import_payload");
    if (!handles_.holds(into)) return false;
    GuestDevice *const owner = owner_of(into);
    if (owner == nullptr || !owner->memory().import_semaphore_payload(fd, into)) return false;
    wire_handle = into;
    detail = guest_semaphore;
    return true;
}

GuestDevice *VulkanBackend::device_of(uint64_t device)
{
    AV_SCOPE("VulkanBackend::device_of");
    if (device == 0u) return nullptr;
    return devices_.find_if(
        [device](const GuestDevice &made) { return reinterpret_cast<uint64_t>(made.device()) == device; });
}

GuestDevice *VulkanBackend::owner_of(uint64_t made)
{
    AV_SCOPE("VulkanBackend::owner_of");
    return handles_.holds(made) ? device_of(handles_.parent_of(made)) : nullptr;
}

bool VulkanBackend::kept_on(const GuestDevice &device, uint64_t made)
{
    AV_SCOPE("VulkanBackend::kept_on");
    return handles_.remember(made, VulkanObjectKind::other, reinterpret_cast<uint64_t>(device.device()), 0u) ==
           Status::ok;
}

GuestDevice *VulkanBackend::forgot(uint64_t made)
{
    AV_SCOPE("VulkanBackend::forgot");
    GuestDevice *const owner = owner_of(made);
    if (handles_.holds(made)) handles_.forget(made);
    buffer_device_ = BufferDevice{};
    return owner;
}

void VulkanBackend::release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail)
{
    AV_SCOPE("VulkanBackend::release_object");
    if (detail == guest_semaphore) return;
    const uint64_t device = handles_.holds(wire_handle) ? handles_.parent_of(wire_handle) : 0u;
    GuestDevice *const owner = forgot(wire_handle);
    if (owner == nullptr) return;
    guard_.destroy(kind == AV_SHARED_MEMORY ? VulkanDestroyKind::imported_memory : VulkanDestroyKind::imported_semaphore,
                   device, 0u, wire_handle);
}

void VulkanBackend::abandon(const SourceMemory &, uint64_t handle)
{
    AV_SCOPE("VulkanBackend::abandon");
    const uint64_t device = handles_.holds(handle) ? handles_.parent_of(handle) : 0u;
    GuestDevice *const owner = forgot(handle);
    if (owner != nullptr) guard_.destroy(VulkanDestroyKind::imported_memory, device, 0u, handle);
}

void VulkanBackend::unbind_arena(const SourceMemory &)
{
    AV_SCOPE("VulkanBackend::unbind_arena");
}

void VulkanBackend::retire_arena(const SourceMemory &)
{
    AV_SCOPE("VulkanBackend::retire_arena");
}

bool VulkanBackend::poll()
{
    AV_SCOPE("VulkanBackend::poll");
    parked_.service();
    return parked_.pending() || lost_;
}

void VulkanBackend::bind_signal(void (*signal)(void *context), void *context)
{
    AV_SCOPE("VulkanBackend::bind_signal");
    if (signal != nullptr) fence_watch_.bind(signal, context);
    else fence_watch_.unbind();
}

void VulkanBackend::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("VulkanBackend::gpu_progress");
    marked = 0u;
    reached = 0u;
}

bool VulkanBackend::drain(TeardownBudget &budget)
{
    AV_SCOPE("VulkanBackend::drain");
    parked_.drop_all();
    return guard_.drain(budget);
}

void VulkanBackend::end_batch()
{
    AV_SCOPE("VulkanBackend::end_batch");
    parked_.service();
    guard_.end_batch();
}

bool VulkanBackend::lose_device()
{
    AV_SCOPE("VulkanBackend::lose_device");
    return adopting_ != nullptr && adopting_->table().lose();
}

bool VulkanBackend::lost() const
{
    AV_SCOPE("VulkanBackend::lost");
    return lost_ || guard_.lost();
}

void VulkanBackend::let_go(uint64_t raw, VulkanObjectKind kind)
{
    AV_SCOPE("VulkanBackend::let_go");
    if (kind == VulkanObjectKind::device) {
        on_vkDestroyDevice(reinterpret_cast<VkDevice>(raw), nullptr);
        return;
    }
    if (!devices_leaked_) ::vkDestroyInstance(reinterpret_cast<VkInstance>(raw), nullptr);
}

void VulkanBackend::let_go_of_thread()
{
    AV_SCOPE("VulkanBackend::let_go_of_thread");
}

void VulkanBackend::use_host_ram(ByteLedger *ledger, uint64_t limit)
{
    AV_SCOPE("VulkanBackend::use_host_ram");
    heap_.bind(ledger, limit);
}

void VulkanBackend::shut_down()
{
    AV_SCOPE("VulkanBackend::shut_down");
    fence_watch_.unbind();
    parked_.drop_all();
    host_waits_.clear();
    buffer_device_ = BufferDevice{};
    handles_.release_all(release_order, sizeof release_order / sizeof release_order[0],
                         [this](uint64_t raw, VulkanObjectKind kind, uint64_t &, uint32_t) { let_go(raw, kind); });
    vram_.close();
    templates_.clear();
    layouts_.clear();
    families_.clear();
    lost_ = false;
    devices_leaked_ = false;
    imports_ = nullptr;
    types_learnt_ = false;
    gpu_ = nullptr;
    ran_ = 0u;
}

uint64_t VulkanBackend::ran() const
{
    AV_SCOPE("VulkanBackend::ran");
    return ran_;
}

}
