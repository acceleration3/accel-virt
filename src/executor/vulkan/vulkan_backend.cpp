#include "executor/vulkan/vulkan_backend.hpp"
#include "executor/source_memory.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/sharing/shared_image_shape.hpp"

#include "core/support/untestable.hpp"
#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"
#include "executor/host_gpu.hpp"
#include "executor/imported_blob.hpp"
#include "proto/generated/vulkan/vk_command_names.h"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <string.h>
#include <unistd.h>

namespace av {

namespace {

/* The executor whose handles the generated decoder is about to check. One
 * context runs on one worker thread and one call at a time - the ABI says so -
 * so the current backend is the thread's, and this is the only shape a hook
 * with no user pointer can take. */
thread_local VulkanBackend *inside = nullptr;


/* Every other handle. The driver dereferences these too - a VkBuffer this
 * executor never made is the same hole one layer down - but null is a value the
 * API itself defines for most of them, and refusing it would refuse every
 * optional parameter. So: null passes, anything else must be ours. */
template <typename H>
bool answer_optional(const VulkanBackend *backend, uint64_t raw, H &out)
{
    AV_SCOPE("answer_optional");
    if (backend == nullptr) {
        memcpy(&out, &raw, sizeof out);
        return true;
    }
    if (raw != 0u && !backend->knows_handle(raw)) return false;
    memcpy(&out, &raw, sizeof out);
    return true;
}

}  // namespace

VulkanBackend::VulkanBackend() : VulkanBackend(heap_)
{
    AV_SCOPE("VulkanBackend::VulkanBackend");
}

/* heap_ is constructed before blocks_ is bound to it, whichever constructor
 * runs: it is declared first. */
VulkanBackend::VulkanBackend(BlockSource &blocks)
    : heap_(), blocks_(blocks), templates_(blocks), gpu_(nullptr), source_(nullptr), listed_(), extensions_(blocks),
      application_(), instance_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE), physical_(VK_NULL_HANDLE),
      memory_import_(false), handles_(blocks), adopted_(), import_(), types_(), types_learnt_(false), parent_(0u),
      ran_(0u)
{
    AV_SCOPE("VulkanBackend::VulkanBackend");
}

void VulkanBackend::initialize(const HostGpu &gpu, HostMemorySource *source)
{
    AV_SCOPE("VulkanBackend::initialize");
    gpu_ = &gpu;
    source_ = source;
}

VkPhysicalDevice VulkanBackend::bound_device(VkInstance instance)
{
    AV_SCOPE("VulkanBackend::bound_device");
    return gpu_ != nullptr ? listed_.find(instance, *gpu_) : VK_NULL_HANDLE;
}

/* Both lists answer the same way: the count alone when there is no array, and
 * otherwise as much of the one device as the room holds, VK_INCOMPLETE when
 * that was none of it. */
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
    /* A group of one is a group every physical device is. The caller's sType
     * and pNext stay as it set them. */
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

/* The decoder asked room_for every handle a reply carries before the driver
 * made any, so remembering one cannot fail from the wire. Were it to, the
 * handle is one this host refuses when the guest names it - a refused
 * command, never a pointer the guest chose. */
void VulkanBackend::made(uint64_t raw, HandleKind kind)
{
    AV_SCOPE("VulkanBackend::made");
    if (handles_.remember(raw, parent_, kind) != Status::ok) {
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
    handles_.forget(raw);
}

void VulkanBackend::children_ended(uint64_t raw)
{
    AV_SCOPE("VulkanBackend::children_ended");
    handles_.forget_children(raw);
}

bool VulkanBackend::knows_handle(uint64_t raw) const
{
    AV_SCOPE("VulkanBackend::knows_handle");
    return handles_.knows(raw);
}

bool VulkanBackend::knows_handle(uint64_t raw, HandleKind kind) const
{
    AV_SCOPE("VulkanBackend::knows_handle");
    return handles_.knows(raw, kind);
}

VulkanBackend *VulkanBackend::deciding()
{
    AV_SCOPE("VulkanBackend::deciding");
    return inside;
}

Status VulkanBackend::enable_identity(VkInstanceCreateInfo &create)
{
    AV_SCOPE("VulkanBackend::enable_identity");
    const Status taken = extensions_.take(create.enabledExtensionCount, create.ppEnabledExtensionNames, 1u);
    /* A list the loader will refuse anyway is left for it to refuse. */
    if (taken == Status::invalid_parameter) return Status::ok;
    if (taken != Status::ok) return taken;
    extensions_.add(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
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

/* The guest's host-visible memory is the memory source's, which reaches the
 * guest's device as a dma-buf (docs/host-memory.md). VK_KHR_external_memory
 * is core from 1.1 and still valid to enable there. The modifier extension is
 * what the ICD's swapchain images are laid out by over a window chain's
 * memory (ChainedImage, ExportableImageInfo). */
Status VulkanBackend::enable_memory_import(VkDeviceCreateInfo &create, uint32_t &importing)
{
    AV_SCOPE("VulkanBackend::enable_memory_import");
    constexpr uint32_t added = 4u;
    importing = create.enabledExtensionCount;
    const Status taken = extensions_.take(create.enabledExtensionCount, create.ppEnabledExtensionNames, added);
    if (taken == Status::invalid_parameter) return Status::ok;
    if (taken != Status::ok) return taken;
    extensions_.add(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    extensions_.add(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    extensions_.add(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
    importing = extensions_.count();
    extensions_.add(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    create.ppEnabledExtensionNames = extensions_.names();
    create.enabledExtensionCount = extensions_.count();
    return Status::ok;
}

VkResult VulkanBackend::on_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *, VkInstance *pInstance)
{
    AV_SCOPE("VulkanBackend::on_vkCreateInstance");
    /* A version below 1.0 - major zero - is refused here rather than passed
     * on. The loader only warns about it (VUID-VkApplicationInfo-apiVersion)
     * and then builds a chain the layers behind it read as one that was never
     * made: Mesa's device-select layer dereferences a table it never
     * allocated and the host process dies, which no guest may be able to do. */
    if (pCreateInfo->pApplicationInfo != nullptr && pCreateInfo->pApplicationInfo->apiVersion != 0u &&
        pCreateInfo->pApplicationInfo->apiVersion < lowest_api_version)
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    VkInstanceCreateInfo create = *pCreateInfo;
    if (enable_identity(create) != Status::ok) return VK_ERROR_OUT_OF_HOST_MEMORY;
    adopt_host_version(create);
    const VkResult result = RealVulkanDispatch::on_vkCreateInstance(&create, nullptr, pInstance);
    /* Kept so shut_down can destroy what a dying context never did. Which
     * handles the guest may name back is not decided here: every handle leaves
     * through handle_to_wire, and that is where it is remembered. */
    if (result == VK_SUCCESS) instance_ = *pInstance;
    return result;
}

void VulkanBackend::on_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyInstance");
    if (instance == instance_) instance_ = VK_NULL_HANDLE;
    RealVulkanDispatch::on_vkDestroyInstance(instance, nullptr);
}

/* A template the guest made: what it reads is remembered, because the length of
 * every blob an update through it carries is this template's and the guest's
 * claim about that length is not trusted (feature 047). A template this table
 * has no room for is refused rather than created, so no update can arrive for
 * one it cannot size. */
VkResult VulkanBackend::on_vkCreateDescriptorUpdateTemplate(VkDevice device,
                                                            const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                                            const VkAllocationCallbacks *pAllocator,
                                                            VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
    AV_SCOPE("VulkanBackend::on_vkCreateDescriptorUpdateTemplate");
    /* Sized and given a slot before the driver is asked: a guest sending more
     * templates than this context can track, or entries that cannot be sized,
     * is refused rather than handed a template no update through it could be
     * checked against (constitution V). */
    const Status ready = templates_.reserve(device, pCreateInfo);
    if (ready == Status::no_memory) return VK_ERROR_OUT_OF_HOST_MEMORY;
    if (ready != Status::ok) return VK_ERROR_INITIALIZATION_FAILED;
    const VkResult result = RealVulkanDispatch::on_vkCreateDescriptorUpdateTemplate(device, pCreateInfo, pAllocator,
                                                                                    pDescriptorUpdateTemplate);
    /* The driver's handle names the slot, and a null handle releases it.
     * Vulkan sets the handle to VK_NULL_HANDLE when a create fails, so the
     * driver's answer is read rather than branched on: there is no fault
     * injection between here and the host's own driver, and a branch no test
     * could take is a branch this does not have. */
    templates_.name(*pDescriptorUpdateTemplate);
    return result;
}

void VulkanBackend::on_vkDestroyDescriptorUpdateTemplate(VkDevice device,
                                                         VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                                         const VkAllocationCallbacks *pAllocator)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyDescriptorUpdateTemplate");
    templates_.forget(descriptorUpdateTemplate);
    RealVulkanDispatch::on_vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, pAllocator);
}

uint64_t VulkanBackend::template_bytes(VkDescriptorUpdateTemplate made) const
{
    AV_SCOPE("VulkanBackend::template_bytes");
    return templates_.bytes_of(made);
}

bool VulkanBackend::knows(uint32_t command_id) const
{
    AV_SCOPE("VulkanBackend::knows");
    /* Asked before the dispatch, so an id the profile does not carry is a
     * skip instead of a fault. */
    return vkproto::knows_command(command_id);
}

bool VulkanBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    AV_SCOPE("VulkanBackend::run");
    return run_through(*this, command_id, payload, reply, arena);
}

bool VulkanBackend::run_through(vkproto::ServerDispatch &driver, uint32_t command_id,
                                WireReader &payload, WireWriter &reply, WireArena &arena)
{
    AV_SCOPE("VulkanBackend::run_through");
    inside = this;
    const bool decoded = vkproto::dispatch_command(driver, command_id, payload, reply, arena);
    inside = nullptr;
    parent_ = 0u;
    if (!decoded) return false;
    ran_++;
    return true;
}

VkResult VulkanBackend::on_vkCreateDevice(VkPhysicalDevice physicalDevice,
                                          const VkDeviceCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *, VkDevice *pDevice)
{
    AV_SCOPE("VulkanBackend::on_vkCreateDevice");
    VkDeviceCreateInfo create = *pCreateInfo;
    uint32_t           importing = 0u;
    if (enable_memory_import(create, importing) != Status::ok) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkResult result = RealVulkanDispatch::on_vkCreateDevice(physicalDevice, &create, nullptr, pDevice);
    /* Made without what the driver lacks: listing the physical device's
     * extensions first would take a table of hundreds of names, and a refusal
     * costs one more create. A driver without the modifier extension still
     * imports guest memory, and only a swapchain over a window chain needs
     * it; one without the import extensions is made as the guest asked. A
     * guest naming one that does not exist is refused each time, the same
     * way. */
    if (result == VK_ERROR_EXTENSION_NOT_PRESENT && importing != create.enabledExtensionCount) {
        create.enabledExtensionCount = importing;
        result = RealVulkanDispatch::on_vkCreateDevice(physicalDevice, &create, nullptr, pDevice);
    }
    if (result == VK_ERROR_EXTENSION_NOT_PRESENT &&
        create.enabledExtensionCount != pCreateInfo->enabledExtensionCount)
        result = RealVulkanDispatch::on_vkCreateDevice(physicalDevice, pCreateInfo, nullptr, pDevice);
    /* Kept because a VkDeviceMemory belongs to a device and an import arrives
     * out of band with no command to carry one. A context makes one device; a
     * second replaces the first, which is what the guest sees too. */
    if (result == VK_SUCCESS) {
        device_ = *pDevice;
        physical_ = physicalDevice;
        /* Asked of the device rather than of the list: whichever create made
         * it, the loader answers the entry point only when it was enabled. */
        memory_import_ = import_.imports(device_);
        learn_types(physicalDevice);
    }
    return result;
}

/* The guest is shown one physical device, the bound GPU's, whose source this
 * is. The source's view is the one its requests are translated by, so it is
 * the guest's whenever there is one; without it the guest maps nothing. */
void VulkanBackend::learn_types(VkPhysicalDevice physical)
{
    AV_SCOPE("VulkanBackend::learn_types");
    if (types_learnt_) return;
    types_learnt_ = true;
    if (source_ != nullptr && source_->vulkan_types(types_)) return;
    VkPhysicalDeviceMemoryProperties host = {};
    RealVulkanDispatch::on_vkGetPhysicalDeviceMemoryProperties(physical, &host);
    types_.learn(host, 0u);
}

void VulkanBackend::on_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                                           VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
    AV_SCOPE("VulkanBackend::on_vkGetPhysicalDeviceMemoryProperties");
    learn_types(physicalDevice);
    types_.properties(*pMemoryProperties);
}

void VulkanBackend::on_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                                            VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
    AV_SCOPE("VulkanBackend::on_vkGetPhysicalDeviceMemoryProperties2");
    RealVulkanDispatch::on_vkGetPhysicalDeviceMemoryProperties2(physicalDevice, pMemoryProperties);
    learn_types(physicalDevice);
    types_.properties(pMemoryProperties->memoryProperties);
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
    if (!memory_import_ || device != device_ || types == 0u) return false;
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
    external.handleTypes =
        import_.buffer_external_types(physical_, create, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
    if (backs_with_source(device, create.pNext, external.sType, external.handleTypes)) create.pNext = &external;
}

/* An image whose chain names the source's handle type - the one added here,
 * or the guest's own - may be bound to the source's memory. An image laid out
 * by a DRM format modifier is not asked about: the driver's query needs the
 * modifier, and RADV dereferences the structure that would name it. */
bool VulkanBackend::for_source(VkDevice device, VkImageCreateInfo &create, VkExternalMemoryImageCreateInfo &external)
{
    AV_SCOPE("VulkanBackend::for_source");
    external = {};
    external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    external.pNext = create.pNext;
    if (create.tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
        external.handleTypes =
            import_.image_external_types(physical_, create, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
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
    VkBufferCreateInfo               create = *pCreateInfo;
    VkExternalMemoryBufferCreateInfo external;
    for_source(device, create, external);
    return RealVulkanDispatch::on_vkCreateBuffer(device, &create, nullptr, pBuffer);
}

/* What the image was made as is remembered with its handle, which is what its
 * memory requirements are answered by, and goes when the handle does. Vulkan
 * sets the handle to VK_NULL_HANDLE when a create fails, and the null handle
 * is never remembered, so the driver's answer needs no branch of its own. */
VkResult VulkanBackend::on_vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                                         const VkAllocationCallbacks *, VkImage *pImage)
{
    AV_SCOPE("VulkanBackend::on_vkCreateImage");
    VkImageCreateInfo               create = *pCreateInfo;
    VkExternalMemoryImageCreateInfo external;
    const bool                      bindable = for_source(device, create, external);
    const VkResult result = RealVulkanDispatch::on_vkCreateImage(device, &create, nullptr, pImage);
    if (bindable) {
        made_through(reinterpret_cast<uint64_t>(device));
        made(reinterpret_cast<uint64_t>(*pImage), HandleKind::external_image);
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
                   handles_.knows(reinterpret_cast<uint64_t>(image), HandleKind::external_image));
}

void VulkanBackend::on_vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo,
                                                     VkMemoryRequirements2 *pMemoryRequirements)
{
    AV_SCOPE("VulkanBackend::on_vkGetImageMemoryRequirements2");
    RealVulkanDispatch::on_vkGetImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
    VkMemoryRequirements &needs = pMemoryRequirements->memoryRequirements;
    needs.memoryTypeBits = image_bits(needs.memoryTypeBits,
                                      handles_.knows(reinterpret_cast<uint64_t>(pInfo->image),
                                                     HandleKind::external_image));
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

/* The create info answers what on_vkCreateImage would make of it. */
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

/* A linear image made for a dma-buf may be laid out for it - a driver may
 * pitch its rows as the import needs - so the layout asked of a create info
 * is the made image's. */
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

VkResult VulkanBackend::on_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits,
                                         VkFence fence)
{
    AV_SCOPE("VulkanBackend::on_vkQueueSubmit");
    if (timing_.enabled()) return timing_.submit(queue, submitCount, pSubmits, fence);
    return RealVulkanDispatch::on_vkQueueSubmit(queue, submitCount, pSubmits, fence);
}

void VulkanBackend::on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyDevice");
    timing_.forget(device);
    templates_.forget_device(device);
    fds_.forget_device(device);
    if (device == device_) device_ = VK_NULL_HANDLE;
    RealVulkanDispatch::on_vkDestroyDevice(device, nullptr);
}

void VulkanBackend::on_vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkFreeMemory");
    ImportedBlob *const imported = adopted_.find(reinterpret_cast<uint64_t>(memory));
    if (imported == nullptr) {
        RealVulkanDispatch::on_vkFreeMemory(device, memory, nullptr);
        return;
    }
    /* The blob stays the device's until it releases it; only what this
     * library made of it goes, so that the release finds nothing to free. */
    abandon(*imported);
}

VkResult VulkanBackend::on_vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR *pGetFdInfo, int *pFd)
{
    AV_SCOPE("VulkanBackend::on_vkGetMemoryFdKHR");
    return remembered(device, RealVulkanDispatch::on_vkGetMemoryFdKHR(device, pGetFdInfo, pFd), pFd);
}

VkResult VulkanBackend::on_vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR *pGetFdInfo,
                                               int *pFd)
{
    AV_SCOPE("VulkanBackend::on_vkGetSemaphoreFdKHR");
    return remembered(device, RealVulkanDispatch::on_vkGetSemaphoreFdKHR(device, pGetFdInfo, pFd), pFd);
}

/* The fd is a number in this process: one this context did not export names
 * whatever else is open here, and the driver would be asked about it. */
VkResult VulkanBackend::on_vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType,
                                                      int fd, VkMemoryFdPropertiesKHR *pMemoryFdProperties)
{
    AV_SCOPE("VulkanBackend::on_vkGetMemoryFdPropertiesKHR");
    if (!fds_.holds(fd)) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    const VkResult answered =
        RealVulkanDispatch::on_vkGetMemoryFdPropertiesKHR(device, handleType, fd, pMemoryFdProperties);
    pMemoryFdProperties->memoryTypeBits = types_.guest_bits(pMemoryFdProperties->memoryTypeBits);
    return answered;
}

VkResult VulkanBackend::remembered(VkDevice device, VkResult answered, int *fd)
{
    AV_SCOPE("VulkanBackend::remembered");
    /* A driver that answered VK_SUCCESS answered an fd with it, and the
     * generated server always has somewhere to put one. */
    if (answered != VK_SUCCESS) return answered;
    if (fds_.keep(device, *fd)) return VK_SUCCESS;
    /* Nowhere to remember it, so nothing could ever name it: closed here
     * rather than answered as a handle no import would be allowed to use. */
    static_cast<void>(::close(*fd));
    *fd = -1;
    return VK_ERROR_TOO_MANY_OBJECTS;
}

VkResult VulkanBackend::on_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                           const VkAllocationCallbacks *, VkDeviceMemory *pMemory)
{
    AV_SCOPE("VulkanBackend::on_vkAllocateMemory");
    int fd = -1;
    if (!exported(pAllocateInfo->pNext, fd)) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    VkMemoryAllocateInfo allocate = *pAllocateInfo;
    allocate.memoryTypeIndex = types_.host_type(pAllocateInfo->memoryTypeIndex);
    if (allocate.memoryTypeIndex == GuestMemoryTypes::no_type) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    return imported(RealVulkanDispatch::on_vkAllocateMemory(device, &allocate, nullptr, pMemory), fd);
}

VkResult VulkanBackend::on_vkImportSemaphoreFdKHR(VkDevice device,
                                                  const VkImportSemaphoreFdInfoKHR *pImportSemaphoreFdInfo)
{
    AV_SCOPE("VulkanBackend::on_vkImportSemaphoreFdKHR");
    if (!fds_.holds(pImportSemaphoreFdInfo->fd)) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    return imported(RealVulkanDispatch::on_vkImportSemaphoreFdKHR(device, pImportSemaphoreFdInfo),
                    pImportSemaphoreFdInfo->fd);
}

bool VulkanBackend::exported(const void *next, int &fd)
{
    AV_SCOPE("VulkanBackend::exported");
    for (const auto *node = static_cast<const VkBaseInStructure *>(next); node != nullptr; node = node->pNext) {
        if (node->sType != VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR) continue;
        const auto *import = reinterpret_cast<const VkImportMemoryFdInfoKHR *>(node);
        if (!fds_.holds(import->fd)) return false;
        fd = import->fd;
    }
    return true;
}

VkResult VulkanBackend::imported(VkResult answered, int fd)
{
    AV_SCOPE("VulkanBackend::imported");
    /* The driver owns the fd only where it took it; a refused import leaves it
     * this context's, to be closed with its device rather than twice. */
    if (answered == VK_SUCCESS && fd >= 0) static_cast<void>(fds_.take(fd));
    return answered;
}

/* The source's memory, by its dma-buf, as memory of the type it was
 * allocated as: the guest asked for that type (HostMemoryRequest), and only
 * onto a device the guest has made, because device memory belongs to one.
 * Vulkan owns the descriptor once the import is made. */
bool VulkanBackend::adopt(ImportedBlob &blob)
{
    AV_SCOPE("VulkanBackend::adopt");
    if (device_ == VK_NULL_HANDLE || !memory_import_) return false;
    const SourceMemory &source = blob.memory();
    const int           fd = source.share();
    uint64_t            memory = 0u;
    if (!import_.import_dmabuf(device_, fd, source.size(), source.memory_type(), memory)) {
        if (fd >= 0) close(fd);
        return false;
    }
    /* A non-dispatchable handle on the wire is the driver's own number, so the
     * memory is what the guest names it by. */
    blob.record(memory, memory);
    /* Made through the device, so destroying the device ends it as the driver
     * does. A registry with no room refuses the import rather than losing the
     * device, as a command's handle would: the guest is copied through
     * instead, and nothing it already has breaks. */
    if (adopted_.add(blob) != Status::ok ||
        handles_.remember(memory, reinterpret_cast<uint64_t>(device_)) != Status::ok) {
        abandon(blob);
        return false;
    }
    return true;
}

uint32_t VulkanBackend::first_memory_type(uint32_t mask) const
{
    AV_SCOPE("VulkanBackend::first_memory_type");
    for (uint32_t type = 0u; type < max_memory_types; type++)
        if ((mask & (1u << type)) != 0u) return type;
    return max_memory_types;
}

bool VulkanBackend::export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind, int &fd)
{
    AV_SCOPE("VulkanBackend::export_object");
    handle_kind = 0u;
    fd = -1;
    if (device_ == VK_NULL_HANDLE || wire_handle == 0u) return false;
    if (!handles_.knows(wire_handle)) return false;
    if (kind == AV_SHARED_MONITORED_FENCE || kind == AV_SHARED_NATIVE_FENCE) {
        if (!import_.export_semaphore(device_, wire_handle, fd)) return false;
        handle_kind = AV_SHARED_HANDLE_OPAQUE_FD;
        return true;
    }
    if (kind != AV_SHARED_MEMORY) return false;
    /* Memory is shared as a dma-buf and nothing else. An opaque fd carries no
     * memory type with it, and Vulkan requires an import to name the type the
     * allocation was made with - which is the exporting guest's number, on the
     * exporting guest's device, and nothing on this side of the wire has it.
     * A GPU that imports the host pointer therefore shares no memory at all,
     * and the guest is refused rather than handed something it cannot bind. */
    if (!import_.export_memory(device_, wire_handle, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd))
        return false;
    handle_kind = AV_SHARED_HANDLE_DMA_BUF;
    return true;
}

bool VulkanBackend::import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                  const SharedImageShape &shape,
                                  uint64_t &wire_handle, uint64_t &detail)
{
    AV_SCOPE("VulkanBackend::import_object");
    static_cast<void>(shape);   /* memory alone: no image of this API is made over it */
    wire_handle = 0u;
    detail = 0u;
    if (device_ == VK_NULL_HANDLE || fd < 0) return false;
    if (kind == AV_SHARED_MONITORED_FENCE || kind == AV_SHARED_NATIVE_FENCE) {
        if (handle_kind != AV_SHARED_HANDLE_OPAQUE_FD) return false;
        if (!import_.import_shared_semaphore(device_, fd, wire_handle)) return false;
        made(wire_handle);
        return true;
    }
    if (kind != AV_SHARED_MEMORY || size == 0u) return false;
    if (handle_kind != AV_SHARED_HANDLE_DMA_BUF) return false;
    const uint32_t memory_type =
        first_memory_type(import_.dmabuf_types(device_, fd));
    if (memory_type == max_memory_types) return false;
    const bool imported =
        import_.import_shared_memory(device_, fd, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, size,
                                     memory_type, wire_handle);
    /* The host driver declining an import the backend asked for correctly.
     * No driver reachable from a test on this machine ever does: radv is the
     * only one here that takes a dma-buf at all, and it takes one of the wrong
     * size, one whose device never enabled the extension, and a descriptor
     * number that names nothing (research.md §19). */
    if (!imported) { AV_UNTESTABLE("no host driver here refuses an import"); return false; }
    made(wire_handle);
    return true;
}

/* The guest may have freed the memory itself, with the commands it frees any
 * by: then the registry no longer knows it and there is nothing to do. */
void VulkanBackend::release_object(uint32_t kind, uint64_t wire_handle, uint64_t)
{
    AV_SCOPE("VulkanBackend::release_object");
    if (wire_handle == 0u || device_ == VK_NULL_HANDLE || !handles_.knows(wire_handle)) return;
    handles_.forget(wire_handle);
    if (kind == AV_SHARED_MEMORY) {
        import_.release(device_, wire_handle);
        return;
    }
    import_.destroy_semaphore(device_, wire_handle);
}

void VulkanBackend::abandon(ImportedBlob &blob)
{
    AV_SCOPE("VulkanBackend::abandon");
    adopted_.remove(blob);
    const uint64_t object = blob.object();
    if (object == 0u || device_ == VK_NULL_HANDLE) return;
    /* The pointer itself is the device's and stays mapped; what this made of
     * it is this library's to free, and no longer a handle the guest names. */
    handles_.forget(object);
    import_.release(device_, object);
    blob.record(0u, 0u);
}

bool VulkanBackend::poll()
{
    AV_SCOPE("VulkanBackend::poll");
    return false;
}

void VulkanBackend::bind_signal(void (*)(void *context), void *)
{
    AV_SCOPE("VulkanBackend::bind_signal");
}

void VulkanBackend::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("VulkanBackend::gpu_progress");
    marked = 0u;
    reached = 0u;
}

void VulkanBackend::end_batch()
{
    AV_SCOPE("VulkanBackend::end_batch");
}

bool VulkanBackend::lose_device()
{
    AV_SCOPE("VulkanBackend::lose_device");
    if (device_ == VK_NULL_HANDLE) return false;
    const auto lose = reinterpret_cast<void (*)(VkDevice)>(::vkGetDeviceProcAddr(device_, "vkAvLoseDeviceAV"));
    if (lose == nullptr) return false;
    lose(device_);
    return true;
}

/* A Vulkan device's loss reaches the guest from the driver itself, as
 * VK_ERROR_DEVICE_LOST answering its next command. */
bool VulkanBackend::lost() const
{
    AV_SCOPE("VulkanBackend::lost");
    return false;
}

void VulkanBackend::shut_down()
{
    AV_SCOPE("VulkanBackend::shut_down");
    /* Device before instance, which is the order the API requires and the
     * order a guest would have used had it bothered - and through the guest's
     * own destroy, which forgets what the device exported. The backend
     * outlives the context in its slot: an fd left listed here is one the
     * next context could import, another context's memory. A device the
     * guest replaced and never destroyed is not this backend's to name, so
     * the tables are emptied whichever device filled them. */
    if (device_ != VK_NULL_HANDLE) on_vkDestroyDevice(device_, nullptr);
    fds_.forget_all();
    templates_.forget_all();
    if (instance_ != VK_NULL_HANDLE) {
        ::vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    handles_.forget_all();
    adopted_.clear();
    /* The types were learnt of a physical device of the instance that just
     * died. */
    types_learnt_ = false;
    gpu_ = nullptr;
    ran_ = 0u;
}

uint64_t VulkanBackend::ran() const
{
    AV_SCOPE("VulkanBackend::ran");
    return ran_;
}


/* The generated decoder's inbound handle hook, defined here rather than in a
 * file of its own: the defaults are weak and live in the generated archive, and
 * a strong definition in an object nothing else references is never pulled in
 * to override them. Next to the class whose state it reads, it always is. */
namespace vkproto {

namespace {

/* Outbound: whatever the host is about to tell the guest about is a handle the
 * host made, so this is where it becomes one the host will accept back, in
 * the room handles_room made for it before the driver was called. */
template <typename H>
uint64_t remember(VulkanBackend *backend, H value, HandleKind kind = HandleKind::other)
{
    AV_SCOPE("remember");
    uint64_t raw = 0u;
    static_assert(sizeof(H) <= sizeof raw);
    memcpy(&raw, &value, sizeof value);
    if (backend != nullptr) backend->made(raw, kind);
    return raw;
}

/* Nobody deciding means nobody is being defended against: the numbers are this
 * process's own. That is the guest side, where an application's handles arrive
 * through its own loader and identity is the whole of the mapping.
 *
 * On the host a backend is always deciding, because VulkanBackend::run sets it
 * around every dispatch and a decode happens nowhere else. So this is not a way
 * to lose the checking by accident - it is what makes one archive serve both
 * sides without the guest's ICD having every handle it decodes refused
 * (feature 042). */
/* A handle the loader dispatches on. */
template <typename H>
bool answer(const VulkanBackend *backend, uint64_t raw, H &out, HandleKind kind)
{
    AV_SCOPE("answer");
    if (backend == nullptr) {
        memcpy(&out, &raw, sizeof out);
        return true;
    }
    /* Null is a legitimate value for an optional handle and never for one of
     * these five, and no kind of handle: the loader dereferences a
     * dispatchable handle to find the table to call through, so a guest naming
     * the null instance aborts the process rather than getting an error back.
     * The fuzz target found that in vkCmdResetEvent2 (feature 042). And it must
     * be this kind of handle: the table a queue is dereferenced through is not
     * a device's, so naming one where the other belongs aborts the process the
     * same way (T064). */
    if (!backend->knows_handle(raw, kind)) return false;
    memcpy(&out, &raw, sizeof out);
    return true;
}

}  // namespace

/* Nobody deciding is the guest side again, where nothing is remembered and so
 * nothing is forgotten, and there is always room. */
bool handles_room(uint64_t handles) noexcept
{
    AV_SCOPE("handles_room");
    VulkanBackend *const backend = VulkanBackend::deciding();
    return backend == nullptr || backend->room_for(handles);
}

void handles_made_through(uint64_t parent) noexcept
{
    AV_SCOPE("handles_made_through");
    if (VulkanBackend *backend = VulkanBackend::deciding()) backend->made_through(parent);
}

void handle_ended(uint64_t handle) noexcept
{
    AV_SCOPE("handle_ended");
    if (VulkanBackend *backend = VulkanBackend::deciding()) backend->ended(handle);
}

void handle_children_ended(uint64_t handle) noexcept
{
    AV_SCOPE("handle_children_ended");
    if (VulkanBackend *backend = VulkanBackend::deciding()) backend->children_ended(handle);
}

/* The length of a WithTemplate blob, for the generated decoder: the template's,
 * as this context recorded it when the guest created it. A context that is not
 * deciding - a loopback test's decode with no backend - answers 0, which
 * accepts only an empty blob. */
uint64_t descriptor_template_bytes(VkDescriptorUpdateTemplate t) noexcept
{
    AV_SCOPE("descriptor_template_bytes");
    const VulkanBackend *const backend = VulkanBackend::deciding();
    return backend != nullptr ? backend->template_bytes(t) : 0u;
}

bool wire_to_handle(uint64_t raw, VkInstance &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer(VulkanBackend::deciding(), raw, out, HandleKind::instance);
}

bool wire_to_handle(uint64_t raw, VkPhysicalDevice &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer(VulkanBackend::deciding(), raw, out, HandleKind::physical_device);
}

bool wire_to_handle(uint64_t raw, VkDevice &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer(VulkanBackend::deciding(), raw, out, HandleKind::device);
}

bool wire_to_handle(uint64_t raw, VkQueue &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer(VulkanBackend::deciding(), raw, out, HandleKind::queue);
}

bool wire_to_handle(uint64_t raw, VkCommandBuffer &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer(VulkanBackend::deciding(), raw, out, HandleKind::command_buffer);
}

bool wire_to_handle(uint64_t raw, VkAccelerationStructureKHR &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkAccelerationStructureNV &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkBuffer &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkBufferView &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkCommandPool &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDeferredOperationKHR &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDescriptorPool &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDescriptorSet &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDescriptorSetLayout &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDescriptorUpdateTemplate &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDeviceMemory &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkEvent &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkFence &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkFramebuffer &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkImage &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkImageView &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkMicromapEXT &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPipeline &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPipelineBinaryKHR &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPipelineCache &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPipelineLayout &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPrivateDataSlot &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkQueryPool &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkRenderPass &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkSampler &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkSamplerYcbcrConversion &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkSemaphore &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkShaderEXT &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkShaderModule &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkSurfaceKHR &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkSwapchainKHR &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkValidationCacheEXT &out) noexcept
{
    AV_SCOPE("wire_to_handle");
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

uint64_t handle_to_wire(VkAccelerationStructureKHR v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkAccelerationStructureNV v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkBuffer v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkBufferView v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkCommandBuffer v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v, HandleKind::command_buffer);
}

uint64_t handle_to_wire(VkCommandPool v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkDeferredOperationKHR v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkDescriptorPool v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkDescriptorSet v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkDescriptorSetLayout v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkDescriptorUpdateTemplate v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkDevice v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v, HandleKind::device);
}

uint64_t handle_to_wire(VkDeviceMemory v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkEvent v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkFence v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkFramebuffer v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkImage v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkImageView v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkInstance v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v, HandleKind::instance);
}

uint64_t handle_to_wire(VkMicromapEXT v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkPhysicalDevice v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v, HandleKind::physical_device);
}

uint64_t handle_to_wire(VkPipeline v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkPipelineBinaryKHR v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkPipelineCache v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkPipelineLayout v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkPrivateDataSlot v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkQueryPool v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkQueue v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v, HandleKind::queue);
}

uint64_t handle_to_wire(VkRenderPass v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkSampler v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkSamplerYcbcrConversion v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkSemaphore v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkShaderEXT v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkShaderModule v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkSurfaceKHR v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkSwapchainKHR v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

uint64_t handle_to_wire(VkValidationCacheEXT v) noexcept
{
    AV_SCOPE("handle_to_wire");
    return remember(VulkanBackend::deciding(), v);
}

}  // namespace vkproto

}  // namespace av
