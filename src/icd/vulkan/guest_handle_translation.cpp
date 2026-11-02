#include "icd/vulkan/guest_handle_translation.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/vulkan/descriptor_template_table.hpp"
#include "icd/vulkan/handle_bits.hpp"
#include "icd/vulkan/handle_registry.hpp"

namespace av::icd {

GuestHandleTranslation::GuestHandleTranslation(ThreadLock &lock, const HandleRegistry &handles,
                                               const DescriptorTemplateTable &templates)
    : lock_(lock), handles_(handles), templates_(templates)
{
    AV_SCOPE("GuestHandleTranslation::GuestHandleTranslation");
}

uint64_t GuestHandleTranslation::wire_for(vkproto::HandleKind kind, uint64_t handle)
{
    AV_SCOPE("GuestHandleTranslation::wire_for");
    LoaderHandle::Kind dispatched = LoaderHandle::Kind::instance;
    switch (kind) {
    case vkproto::HandleKind::instance: dispatched = LoaderHandle::Kind::instance; break;
    case vkproto::HandleKind::physical_device: dispatched = LoaderHandle::Kind::physical_device; break;
    case vkproto::HandleKind::device: dispatched = LoaderHandle::Kind::device; break;
    case vkproto::HandleKind::queue: dispatched = LoaderHandle::Kind::queue; break;
    case vkproto::HandleKind::command_buffer: dispatched = LoaderHandle::Kind::command_buffer; break;
    default: return handle;
    }
    const ThreadLockHold      held(lock_);
    const LoaderHandle *const made = handles_.made(HandleBits(handle).as<void *>(), dispatched);
    return made != nullptr ? made->host() : 0u;
}

bool GuestHandleTranslation::handle_for(vkproto::HandleKind, uint64_t wire, uint64_t &handle) const
{
    AV_SCOPE("GuestHandleTranslation::handle_for");
    handle = wire;
    return true;
}

uint64_t GuestHandleTranslation::template_bytes(VkDescriptorUpdateTemplate made) const
{
    AV_SCOPE("GuestHandleTranslation::template_bytes");
    return templates_.bytes_of(made);
}

}
