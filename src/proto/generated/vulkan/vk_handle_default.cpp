// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m rvk_codegen --xml ... --out generated/)


#include "vk_serialize.h"

#include <cstdint>
#include <cstring>

#if defined(_WIN32)
// PE: no weak definitions. See the comment in the emitter.
#  define RVK_HOOK_DEFAULT
#else
#  define RVK_HOOK_DEFAULT [[gnu::weak]]
#endif

namespace av::vkproto {

namespace {
template<typename H>
[[nodiscard]] std::uint64_t identity(H v) noexcept {
    std::uint64_t raw = 0;
    static_assert(sizeof(H) <= sizeof(raw));
    // sizeof(H), not sizeof(raw): a 32-bit handle is 4 bytes, and
    // copying 8 out of it reads past the end of the argument.
    std::memcpy(&raw, &v, sizeof(H));
    return raw;
}
}  // namespace

RVK_HOOK_DEFAULT std::uint64_t handle_to_wire(VkInstance v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT std::uint64_t handle_to_wire(VkPhysicalDevice v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT std::uint64_t handle_to_wire(VkDevice v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT std::uint64_t handle_to_wire(VkQueue v) noexcept { return identity(v); }
RVK_HOOK_DEFAULT std::uint64_t handle_to_wire(VkCommandBuffer v) noexcept { return identity(v); }

// Identity inbound as well, which is right where the numbers are the
// caller's own. A host overrides every one of these, because there they
// are an attacker's (feature 042).
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkAccelerationStructureKHR& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkAccelerationStructureNV& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkBuffer& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkBufferView& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkCommandBuffer& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkCommandPool& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkDeferredOperationKHR& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkDescriptorPool& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkDescriptorSet& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkDescriptorSetLayout& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkDescriptorUpdateTemplate& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkDevice& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkDeviceMemory& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkEvent& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkFence& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkFramebuffer& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkImage& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkImageView& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkInstance& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkMicromapEXT& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkPhysicalDevice& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkPipeline& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkPipelineBinaryKHR& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkPipelineCache& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkPipelineLayout& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkPrivateDataSlot& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkQueryPool& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkQueue& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkRenderPass& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkSampler& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkSamplerYcbcrConversion& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkSemaphore& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkShaderEXT& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkShaderModule& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkSurfaceKHR& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkSwapchainKHR& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }
RVK_HOOK_DEFAULT bool wire_to_handle(std::uint64_t raw, VkValidationCacheEXT& out) noexcept { std::memcpy(&out, &raw, sizeof(out)); return true; }

}  // namespace av::vkproto
