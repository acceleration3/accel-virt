#ifndef AV_ICD_GUEST_HANDLE_TRANSLATION_HPP
#define AV_ICD_GUEST_HANDLE_TRANSLATION_HPP

#include "proto/generated/vulkan/vk_handle_translation.h"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class ThreadLock;
}

namespace av::icd {

class DescriptorTemplateTable;
class HandleRegistry;

class GuestHandleTranslation final : public vkproto::HandleTranslation {
public:
    GuestHandleTranslation(ThreadLock &lock, const HandleRegistry &handles, const DescriptorTemplateTable &templates);
    GuestHandleTranslation(const GuestHandleTranslation &) = delete;
    GuestHandleTranslation &operator=(const GuestHandleTranslation &) = delete;

    [[nodiscard]] uint64_t wire_for(vkproto::HandleKind kind, uint64_t handle) override;
    [[nodiscard]] bool     handle_for(vkproto::HandleKind kind, uint64_t wire, uint64_t &handle) const override;
    [[nodiscard]] uint64_t template_bytes(VkDescriptorUpdateTemplate made) const override;

private:
    ThreadLock                    &lock_;
    const HandleRegistry          &handles_;
    const DescriptorTemplateTable &templates_;
};

}

#endif
