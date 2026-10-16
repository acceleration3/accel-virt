#include "executor/vulkan/host_template_table.hpp"

#include "core/support/untestable.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

uint64_t key_of(VkDescriptorUpdateTemplate made)
{
    AV_SCOPE("key_of");
    return reinterpret_cast<uint64_t>(made);
}

}  // namespace

HostTemplateTable::HostTemplateTable(BlockSource &blocks)
    : templates_(blocks, Pool::paged), reserved_{VK_NULL_HANDLE, 0u}
{
    AV_SCOPE("HostTemplateTable::HostTemplateTable");
}

Status HostTemplateTable::reserve(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *info)
{
    AV_SCOPE("HostTemplateTable::reserve");
    if (device == VK_NULL_HANDLE || info == nullptr) return Status::invalid_parameter;
    DescriptorTemplateLayout layout;
    if (layout.take(info->pDescriptorUpdateEntries, info->descriptorUpdateEntryCount) != Status::ok)
        return Status::malformed;
    const Status room = templates_.reserve(templates_.count() + 1u);
    if (room != Status::ok) return room;
    reserved_ = HostTemplate{device, layout.bytes()};
    return Status::ok;
}

void HostTemplateTable::name(VkDescriptorUpdateTemplate made)
{
    AV_SCOPE("HostTemplateTable::name");
    const HostTemplate taken = reserved_;
    reserved_ = HostTemplate{VK_NULL_HANDLE, 0u};
    /* The driver answered no handle, or nothing was reserved: no template. */
    if (made == VK_NULL_HANDLE || taken.device_ == VK_NULL_HANDLE) return;
    templates_.erase(key_of(made));
    uint32_t slot = 0u;
    if (templates_.insert(key_of(made), taken, slot) != Status::ok) {
        AV_UNTESTABLE("reserve made room for the template before the driver was asked");
    }
}

void HostTemplateTable::forget(VkDescriptorUpdateTemplate made)
{
    AV_SCOPE("HostTemplateTable::forget");
    templates_.erase(key_of(made));
}

void HostTemplateTable::forget_device(VkDevice device)
{
    AV_SCOPE("HostTemplateTable::forget_device");
    templates_.erase_if([device](uint64_t, HostTemplate &held) { return held.device_ == device; });
}

void HostTemplateTable::forget_all()
{
    AV_SCOPE("HostTemplateTable::forget_all");
    templates_.clear();
    reserved_ = HostTemplate{VK_NULL_HANDLE, 0u};
}

uint64_t HostTemplateTable::bytes_of(VkDescriptorUpdateTemplate made) const
{
    AV_SCOPE("HostTemplateTable::bytes_of");
    const HostTemplate *const held = made != VK_NULL_HANDLE ? templates_.find(key_of(made)) : nullptr;
    return held != nullptr ? held->bytes_ : 0u;
}

uint32_t HostTemplateTable::live() const
{
    AV_SCOPE("HostTemplateTable::live");
    return templates_.count();
}

bool HostTemplateTable::invariants_hold() const
{
    AV_SCOPE("HostTemplateTable::invariants_hold");
    if (templates_.find(0u) != nullptr) return false;
    if (reserved_.device_ == VK_NULL_HANDLE && reserved_.bytes_ != 0u) return false;
    bool of_devices = true;
    templates_.for_each([&of_devices](uint64_t, const HostTemplate &held) {
        of_devices = of_devices && held.device_ != VK_NULL_HANDLE;
    });
    return of_devices;
}

}  // namespace av
