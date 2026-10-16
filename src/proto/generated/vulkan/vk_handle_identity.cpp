// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m av_codegen --xml ... --out generated/)


#include "vk_handle_translation.h"

#include <stdint.h>

namespace av::vkproto {

uint64_t HandleIdentity::wire_for(HandleKind, uint64_t handle) {
    return handle;
}

bool HandleIdentity::handle_for(HandleKind, uint64_t wire, uint64_t& handle) const {
    handle = wire;
    return true;
}

uint64_t HandleIdentity::template_bytes(VkDescriptorUpdateTemplate) const {
    return 0;
}

}  // namespace av::vkproto
