// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m rvk_codegen --xml ... --out generated/)


#include "vk_dispatch_server.h"

#include <stdint.h>

#if defined(_WIN32)
#  define RVK_HOOK_DEFAULT
#else
#  define RVK_HOOK_DEFAULT [[gnu::weak]]
#endif

namespace av::vkproto {

RVK_HOOK_DEFAULT void handles_made_through(uint64_t) noexcept {}
RVK_HOOK_DEFAULT void handle_ended(uint64_t) noexcept {}
RVK_HOOK_DEFAULT void handle_children_ended(uint64_t) noexcept {}

}  // namespace av::vkproto
