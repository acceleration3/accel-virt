// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m rvk_codegen --xml ... --out generated/)


#include "vk_serialize.h"

#include <cstdint>

#if defined(_WIN32)
// PE: no weak definitions. See the comment in the emitter.
#  define RVK_HOOK_DEFAULT
#else
#  define RVK_HOOK_DEFAULT [[gnu::weak]]
#endif

namespace av::vkproto {

// Default: no image registry here, so no pixels are sent.
// The ICD overrides this; see icd/host_image_copy.cpp.
RVK_HOOK_DEFAULT std::uint64_t host_image_copy_bytes(VkImage, const VkMemoryToImageCopy&) noexcept { return 0; }

}  // namespace av::vkproto
