#include "executor/shim/vulkan_timeline.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/vulkan_device_table.hpp"


namespace av {

VulkanTimeline::VulkanTimeline() : table_(nullptr), semaphore_(VK_NULL_HANDLE)
{
    AV_SCOPE("VulkanTimeline::VulkanTimeline");
}

VulkanTimeline::~VulkanTimeline()
{
    AV_SCOPE("VulkanTimeline::~VulkanTimeline");
    release();
}

bool VulkanTimeline::make_exportable(const VulkanDeviceTable &table)
{
    AV_SCOPE("VulkanTimeline::make_exportable");
    if (made()) return false;
    const VkExportSemaphoreCreateInfo exported = {VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO, nullptr,
                                                  VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT};
    const VkSemaphoreTypeCreateInfo   typed = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, &exported,
                                               VK_SEMAPHORE_TYPE_TIMELINE, 0u};
    if (!table.create_semaphore(&typed, semaphore_)) return false;
    table_ = &table;
    return true;
}

bool VulkanTimeline::import_fd(const VulkanDeviceTable &table, OwnedFd &descriptor)
{
    AV_SCOPE("VulkanTimeline::import_fd");
    if (!descriptor.held()) return false;
    const VkSemaphoreTypeCreateInfo typed = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr,
                                             VK_SEMAPHORE_TYPE_TIMELINE, 0u};
    if (made() || !table.create_semaphore(&typed, semaphore_)) return false;
    table_ = &table;
    const OwnedFdIdentity before = descriptor.identity();
    if (!table.import_semaphore_fd(semaphore_, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT, descriptor.fd())) {
        descriptor.settle_failed_import(before);
        release();
        return false;
    }
    static_cast<void>(descriptor.take());
    return true;
}

bool VulkanTimeline::export_fd(OwnedFd &descriptor) const
{
    AV_SCOPE("VulkanTimeline::export_fd");
    int        fd = -1;
    const bool exported =
        table_ != nullptr && table_->semaphore_fd(semaphore_, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT, fd);
    descriptor.hold(fd);
    return exported;
}

bool VulkanTimeline::wait(uint64_t value, uint64_t nanoseconds) const
{
    AV_SCOPE("VulkanTimeline::wait");
    return value == 0u || (table_ != nullptr && table_->wait_semaphore(semaphore_, value, nanoseconds));
}

void VulkanTimeline::release()
{
    AV_SCOPE("VulkanTimeline::release");
    if (table_ != nullptr) table_->destroy_semaphore(semaphore_);
    table_ = nullptr;
    semaphore_ = VK_NULL_HANDLE;
}

bool VulkanTimeline::made() const
{
    AV_SCOPE("VulkanTimeline::made");
    return semaphore_ != VK_NULL_HANDLE;
}

VkSemaphore VulkanTimeline::semaphore() const
{
    AV_SCOPE("VulkanTimeline::semaphore");
    return semaphore_;
}

}
