#include "device/executor/vulkan_backend.hpp"

#include "device/abi/rvk_executor.h"
#include "device/executor/imported_blob.hpp"
#include "proto/generated/vulkan/vk_command_names.h"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <string.h>

namespace av {

namespace {

/* The executor whose handles the generated decoder is about to check. One
 * context runs on one worker thread and one call at a time - the ABI says so -
 * so the current backend is the thread's, and this is the only shape a hook
 * with no user pointer can take. */
thread_local VulkanBackend *inside = nullptr;

std::uint64_t number_of(const void *handle)
{
    std::uint64_t raw = 0u;
    memcpy(&raw, &handle, sizeof handle);
    return raw;
}


/* Every other handle. The driver dereferences these too - a VkBuffer this
 * executor never made is the same hole one layer down - but null is a value the
 * API itself defines for most of them, and refusing it would refuse every
 * optional parameter. So: null passes, anything else must be ours. */
template <typename H>
bool answer_optional(const VulkanBackend *backend, uint64_t raw, H &out)
{
    if (backend == nullptr) {
        memcpy(&out, &raw, sizeof out);
        return true;
    }
    if (raw != 0u && !backend->knows_handle(raw)) return false;
    memcpy(&out, &raw, sizeof out);
    return true;
}

}  // namespace

VulkanBackend::VulkanBackend()
    : instance_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE), handles_(), import_(), ran_(0u),
      lost_(false)
{}

void VulkanBackend::made(uint64_t raw)
{
    if (!handles_.remember(raw)) lost_ = true;
}

bool VulkanBackend::knows_handle(uint64_t raw) const { return handles_.knows(raw); }

VulkanBackend *VulkanBackend::deciding() { return inside; }

VkResult VulkanBackend::on_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *, VkInstance *pInstance)
{
    const VkResult result = RealVulkanDispatch::on_vkCreateInstance(pCreateInfo, nullptr, pInstance);
    /* Kept so shut_down can destroy what a dying context never did. Which
     * handles the guest may name back is not decided here: every handle leaves
     * through handle_to_wire, and that is where it is remembered. */
    if (result == VK_SUCCESS) instance_ = *pInstance;
    return result;
}

void VulkanBackend::on_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *)
{
    handles_.forget(number_of(instance));
    if (instance == instance_) instance_ = VK_NULL_HANDLE;
    RealVulkanDispatch::on_vkDestroyInstance(instance, nullptr);
}

bool VulkanBackend::knows(uint32_t command_id) const
{
    if (command_id >> 16u != vulkan_api) return false;
    /* The generator writes one name per id it carries and "unknown" for the
     * rest, which is exactly the question being asked. Deciding it here rather
     * than by letting the dispatch fall off its switch keeps an id the profile
     * does not carry a skip instead of a fault. */
    return strcmp(vkproto::cmd_id_to_string(command_id), "unknown") != 0;
}

bool VulkanBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    inside = this;
    const bool decoded = vkproto::dispatch_command(*this, command_id, payload, reply, arena);
    inside = nullptr;
    if (!decoded) return false;
    ran_++;
    return true;
}

VkResult VulkanBackend::on_vkCreateDevice(VkPhysicalDevice physicalDevice,
                                          const VkDeviceCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *, VkDevice *pDevice)
{
    const VkResult result = RealVulkanDispatch::on_vkCreateDevice(physicalDevice, pCreateInfo,
                                                                  nullptr, pDevice);
    /* Kept because a VkDeviceMemory belongs to a device and an import arrives
     * out of band with no command to carry one. A context makes one device; a
     * second replaces the first, which is what the guest sees too. */
    if (result == VK_SUCCESS) device_ = *pDevice;
    return result;
}

void VulkanBackend::on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *)
{
    if (device == device_) device_ = VK_NULL_HANDLE;
    handles_.forget(number_of(device));
    RealVulkanDispatch::on_vkDestroyDevice(device, nullptr);
}

bool VulkanBackend::adopt(ImportedBlob &blob)
{
    /* Only a host pointer, because that is the only kind this backend offers
     * through import_kinds, and a dma-buf imported here would land in memory
     * the host cannot map on one of the two GPUs measured. And only onto a
     * device the guest has made, because device memory belongs to one. */
    if (blob.kind() != RVK_BLOB_HOST_POINTER || device_ == VK_NULL_HANDLE) return false;
    uint64_t memory = 0u;
    if (!import_.import(device_, blob.pointer(), blob.size(), memory)) return false;
    blob.set_object(memory);
    return true;
}

void VulkanBackend::abandon(ImportedBlob &blob)
{
    const uint64_t object = blob.object();
    if (object == 0u || device_ == VK_NULL_HANDLE) return;
    /* The pointer itself is the device's and stays mapped; what this made of
     * it is this library's to free. */
    import_.release(device_, object);
    blob.set_object(0u);
}

bool VulkanBackend::lost() const { return lost_; }

void VulkanBackend::shut_down()
{
    /* Device before instance, which is the order the API requires and the
     * order a guest would have used had it bothered. */
    if (device_ != VK_NULL_HANDLE) {
        ::vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        ::vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    handles_.forget_all();
    ran_ = 0u;
    lost_ = false;
}

uint64_t VulkanBackend::ran() const { return ran_; }


/* The generated decoder's inbound handle hook, defined here rather than in a
 * file of its own: the defaults are weak and live in the generated archive, and
 * a strong definition in an object nothing else references is never pulled in
 * to override them. Next to the class whose state it reads, it always is. */
namespace vkproto {

namespace {

/* Outbound: whatever the host is about to tell the guest about is a handle the
 * host made, so this is where it becomes one the host will accept back. A
 * registry that is full makes the executor lose its device rather than hand out
 * a handle it will refuse later. */
template <typename H>
uint64_t remember(VulkanBackend *backend, H value)
{
    uint64_t raw = 0u;
    static_assert(sizeof(H) <= sizeof raw);
    memcpy(&raw, &value, sizeof value);
    if (backend != nullptr) backend->made(raw);
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
bool answer(const VulkanBackend *backend, uint64_t raw, H &out)
{
    if (backend == nullptr) {
        memcpy(&out, &raw, sizeof out);
        return true;
    }
    /* Null is a legitimate value for an optional handle and never for one of
     * these five: the loader dereferences a dispatchable handle to find the
     * table to call through, so a guest naming the null instance aborts the
     * process rather than getting an error back. The fuzz target found that in
     * vkCmdResetEvent2 (feature 042). */
    if (raw == 0u) return false;
    if (!backend->knows_handle(raw)) return false;
    memcpy(&out, &raw, sizeof out);
    return true;
}

}  // namespace

bool wire_to_handle(uint64_t raw, VkInstance &out) noexcept
{
    return answer(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPhysicalDevice &out) noexcept
{
    return answer(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDevice &out) noexcept
{
    return answer(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkQueue &out) noexcept
{
    return answer(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkCommandBuffer &out) noexcept
{
    return answer(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkAccelerationStructureKHR &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkAccelerationStructureNV &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkBuffer &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkBufferView &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkCommandPool &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDeferredOperationKHR &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDescriptorPool &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDescriptorSet &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDescriptorSetLayout &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDescriptorUpdateTemplate &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDeviceMemory &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkEvent &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkFence &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkFramebuffer &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkImage &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkImageView &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkMicromapEXT &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPipeline &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPipelineBinaryKHR &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPipelineCache &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPipelineLayout &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPrivateDataSlot &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkQueryPool &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkRenderPass &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkSampler &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkSamplerYcbcrConversion &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkSemaphore &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkShaderEXT &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkShaderModule &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkSurfaceKHR &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkSwapchainKHR &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkValidationCacheEXT &out) noexcept
{
    return answer_optional(VulkanBackend::deciding(), raw, out);
}

std::uint64_t handle_to_wire(VkAccelerationStructureKHR v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkAccelerationStructureNV v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkBuffer v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkBufferView v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkCommandBuffer v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkCommandPool v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkDeferredOperationKHR v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkDescriptorPool v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkDescriptorSet v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkDescriptorSetLayout v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkDescriptorUpdateTemplate v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkDevice v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkDeviceMemory v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkEvent v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkFence v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkFramebuffer v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkImage v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkImageView v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkInstance v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkMicromapEXT v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkPhysicalDevice v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkPipeline v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkPipelineBinaryKHR v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkPipelineCache v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkPipelineLayout v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkPrivateDataSlot v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkQueryPool v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkQueue v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkRenderPass v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkSampler v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkSamplerYcbcrConversion v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkSemaphore v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkShaderEXT v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkShaderModule v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkSurfaceKHR v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkSwapchainKHR v) noexcept { return remember(VulkanBackend::deciding(), v); }

std::uint64_t handle_to_wire(VkValidationCacheEXT v) noexcept { return remember(VulkanBackend::deciding(), v); }

}  // namespace vkproto

}  // namespace av
