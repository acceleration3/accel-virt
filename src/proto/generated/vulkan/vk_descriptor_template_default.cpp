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

// Default: no template table here, so no descriptors are sent.
AV_HOOK_DEFAULT uint64_t descriptor_template_bytes(VkDescriptorUpdateTemplate) noexcept { return 0; }

}  // namespace av::vkproto
