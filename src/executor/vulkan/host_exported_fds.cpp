#include "executor/vulkan/host_exported_fds.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"

namespace av {

HostExportedFds::HostExportedFds(BlockSource &blocks) : fds_(blocks, Pool::paged)
{
    AV_SCOPE("HostExportedFds::HostExportedFds");
}

HostExportedFds::~HostExportedFds()
{
    AV_SCOPE("HostExportedFds::~HostExportedFds");
    forget_all();
}

Status HostExportedFds::keep(VkDevice device, OwnedFd &descriptor)
{
    AV_SCOPE("HostExportedFds::keep");
    if (device == VK_NULL_HANDLE || !descriptor.held()) return Status::invalid_parameter;
    uint32_t     slot = 0u;
    const Status kept = fds_.insert(static_cast<uint32_t>(descriptor.fd()), device, slot);
    if (kept == Status::ok) static_cast<void>(descriptor.take());
    return kept;
}

bool HostExportedFds::holds(int fd) const
{
    AV_SCOPE("HostExportedFds::holds");
    return fd >= 0 && fds_.find(static_cast<uint32_t>(fd)) != nullptr;
}

bool HostExportedFds::take(int fd)
{
    AV_SCOPE("HostExportedFds::take");
    return fd >= 0 && fds_.erase(static_cast<uint32_t>(fd));
}

void HostExportedFds::forget_device(VkDevice device)
{
    AV_SCOPE("HostExportedFds::forget_device");
    if (device == VK_NULL_HANDLE) return;
    static_cast<void>(fds_.erase_if([device](uint32_t fd, const VkDevice &exporter) {
        if (exporter != device) return false;
        OwnedFd closed(static_cast<int>(fd));
        return true;
    }));
}

void HostExportedFds::forget_all()
{
    AV_SCOPE("HostExportedFds::forget_all");
    fds_.for_each([](uint32_t fd, const VkDevice &) { OwnedFd closed(static_cast<int>(fd)); });
    fds_.clear();
}

uint32_t HostExportedFds::live() const
{
    AV_SCOPE("HostExportedFds::live");
    return fds_.count();
}

bool HostExportedFds::invariants_hold() const
{
    AV_SCOPE("HostExportedFds::invariants_hold");
    bool holds = fds_.invariants_hold();
    fds_.for_each([&holds](uint32_t fd, const VkDevice &device) {
        holds = holds && device != VK_NULL_HANDLE && fd <= 0x7FFFFFFFu;
    });
    return holds;
}

}  // namespace av
