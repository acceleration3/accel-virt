// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m rvk_codegen --xml ... --out generated/)


#include "vk_serialize.h"

#include <stdint.h>

#if defined(_WIN32)
// PE: no weak definitions. See the comment in the emitter.
#  define RVK_HOOK_DEFAULT
#else
#  define RVK_HOOK_DEFAULT [[gnu::weak]]
#endif

namespace av::vkproto {

namespace {
template<typename H>
[[nodiscard]] uint64_t identity(H v) noexcept {
    uint64_t raw = 0;
    static_assert(sizeof(H) <= sizeof(raw));
    // sizeof(H), not sizeof(raw): a 32-bit handle is 4 bytes, and
    // copying 8 out of it reads past the end of the argument.
    __builtin_memcpy(&raw, &v, sizeof(H));
    return raw;
}
}  // namespace

RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkAccelerationStructureKHR v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkAccelerationStructureNV v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkBuffer v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkBufferView v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkCommandBuffer v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkCommandPool v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkDeferredOperationKHR v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkDescriptorPool v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkDescriptorSet v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkDescriptorSetLayout v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkDescriptorUpdateTemplate v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkDevice v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkDeviceMemory v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkEvent v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkFence v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkFramebuffer v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkImage v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkImageView v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkInstance v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkMicromapEXT v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkPhysicalDevice v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkPipeline v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkPipelineBinaryKHR v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkPipelineCache v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkPipelineLayout v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkPrivateDataSlot v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkQueryPool v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkQueue v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkRenderPass v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkSampler v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkSamplerYcbcrConversion v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkSemaphore v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkShaderEXT v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkShaderModule v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkSurfaceKHR v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkSwapchainKHR v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT uint64_t handle_to_wire(VkValidationCacheEXT v) noexcept { return identity(v); }

// Identity inbound as well, which is right where the numbers are the
// caller's own. A host overrides every one of these, because there they
// are an attacker's (feature 042).
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkAccelerationStructureKHR& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkAccelerationStructureNV& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkBuffer& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkBufferView& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkCommandBuffer& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkCommandPool& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkDeferredOperationKHR& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkDescriptorPool& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkDescriptorSet& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkDescriptorSetLayout& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkDescriptorUpdateTemplate& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkDevice& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkDeviceMemory& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkEvent& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkFence& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkFramebuffer& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkImage& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkImageView& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkInstance& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkMicromapEXT& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkPhysicalDevice& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkPipeline& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkPipelineBinaryKHR& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkPipelineCache& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkPipelineLayout& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkPrivateDataSlot& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkQueryPool& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkQueue& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkRenderPass& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkSampler& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkSamplerYcbcrConversion& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkSemaphore& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkShaderEXT& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkShaderModule& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkSurfaceKHR& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkSwapchainKHR& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(uint64_t raw, VkValidationCacheEXT& out) noexcept { __builtin_memcpy(&out, &raw, sizeof(out)); return true; }

}  // namespace av::vkproto
