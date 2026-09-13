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

}  // namespace av::vkproto
