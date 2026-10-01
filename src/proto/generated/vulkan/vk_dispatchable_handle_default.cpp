// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m av_codegen --xml ... --out generated/)


#include "vk_serialize.h"

#include <stdint.h>

#if defined(_WIN32)
// PE: no weak definitions. See the comment in the emitter.
#  define AV_HOOK_DEFAULT
#else
#  define AV_HOOK_DEFAULT [[gnu::weak]]
#endif

namespace av::vkproto {

AV_HOOK_DEFAULT uint64_t handle_to_wire(VkCommandBuffer v) noexcept { uint64_t raw = 0; __builtin_memcpy(&raw, &v, sizeof(v)); return raw; }
AV_HOOK_DEFAULT uint64_t handle_to_wire(VkDevice v) noexcept { uint64_t raw = 0; __builtin_memcpy(&raw, &v, sizeof(v)); return raw; }
AV_HOOK_DEFAULT uint64_t handle_to_wire(VkInstance v) noexcept { uint64_t raw = 0; __builtin_memcpy(&raw, &v, sizeof(v)); return raw; }
AV_HOOK_DEFAULT uint64_t handle_to_wire(VkPhysicalDevice v) noexcept { uint64_t raw = 0; __builtin_memcpy(&raw, &v, sizeof(v)); return raw; }
AV_HOOK_DEFAULT uint64_t handle_to_wire(VkQueue v) noexcept { uint64_t raw = 0; __builtin_memcpy(&raw, &v, sizeof(v)); return raw; }

}  // namespace av::vkproto
