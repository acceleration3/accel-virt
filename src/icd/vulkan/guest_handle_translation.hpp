/* What this ICD's handles cross the wire as (vk_handle_translation.h), for the
 * generated encoders and decoders.
 *
 * A handle the loader dispatches on is the address of the LoaderHandle this
 * ICD gave the application, and crosses as the host's number behind it - 0
 * for an address that is not one, which the host refuses. Every other handle
 * is already the host's own number, and so is every handle a reply carries:
 * the wrappers wrap those themselves. A WithTemplate blob is as long as the
 * template this process made says (feature 047). */
#ifndef AV_ICD_GUEST_HANDLE_TRANSLATION_HPP
#define AV_ICD_GUEST_HANDLE_TRANSLATION_HPP

#include "proto/generated/vulkan/vk_handle_translation.h"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class ThreadLock;
}  // namespace av

namespace av::icd {

class DescriptorTemplateTable;
class HandleRegistry;

class GuestHandleTranslation final : public vkproto::HandleTranslation {
public:
    /* `lock` is the root's, which serializes `handles`. */
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

}  // namespace av::icd

#endif  // AV_ICD_GUEST_HANDLE_TRANSLATION_HPP
