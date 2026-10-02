#include "device/executor/host_exported_fds.hpp"

#include "core/trace/function_scope.hpp"

#include <unistd.h>

namespace av {

HostExportedFds::HostExportedFds() : slots_{}
{
    AV_SCOPE("HostExportedFds::HostExportedFds");
    for (HostExportedFdSlot &slot : slots_) slot.fd_ = -1;
}

HostExportedFds::~HostExportedFds()
{
    AV_SCOPE("HostExportedFds::~HostExportedFds");
    for (HostExportedFdSlot &slot : slots_) release(slot);
}

bool HostExportedFds::keep(VkDevice device, int fd)
{
    AV_SCOPE("HostExportedFds::keep");
    if (device == VK_NULL_HANDLE || fd < 0) return false;
    for (HostExportedFdSlot &slot : slots_) {
        if (slot.fd_ >= 0) continue;
        slot.device_ = device;
        slot.fd_ = fd;
        return true;
    }
    return false;
}

bool HostExportedFds::holds(int fd) const
{
    AV_SCOPE("HostExportedFds::holds");
    return index_of(fd) >= 0;
}

bool HostExportedFds::take(int fd)
{
    AV_SCOPE("HostExportedFds::take");
    const int index = index_of(fd);
    if (index < 0) return false;
    slots_[index] = HostExportedFdSlot{VK_NULL_HANDLE, -1};
    return true;
}

void HostExportedFds::forget_device(VkDevice device)
{
    AV_SCOPE("HostExportedFds::forget_device");
    if (device == VK_NULL_HANDLE) return;
    for (HostExportedFdSlot &slot : slots_)
        if (slot.device_ == device) release(slot);
}

void HostExportedFds::forget_all()
{
    AV_SCOPE("HostExportedFds::forget_all");
    for (HostExportedFdSlot &slot : slots_) release(slot);
}

uint32_t HostExportedFds::live() const
{
    AV_SCOPE("HostExportedFds::live");
    uint32_t found = 0u;
    for (const HostExportedFdSlot &slot : slots_) found += slot.fd_ >= 0 ? 1u : 0u;
    return found;
}

int HostExportedFds::index_of(int fd) const
{
    AV_SCOPE("HostExportedFds::index_of");
    if (fd < 0) return -1;
    for (uint32_t index = 0u; index < max_fds; index++)
        if (slots_[index].fd_ == fd) return static_cast<int>(index);
    return -1;
}

void HostExportedFds::release(HostExportedFdSlot &slot)
{
    AV_SCOPE("HostExportedFds::release");
    if (slot.fd_ >= 0) static_cast<void>(::close(slot.fd_));
    slot = HostExportedFdSlot{VK_NULL_HANDLE, -1};
}

bool HostExportedFds::invariants_hold() const
{
    AV_SCOPE("HostExportedFds::invariants_hold");
    for (uint32_t index = 0u; index < max_fds; index++) {
        const HostExportedFdSlot &slot = slots_[index];
        if (slot.fd_ < 0) {
            if (slot.device_ != VK_NULL_HANDLE) return false;
            continue;
        }
        if (slot.device_ == VK_NULL_HANDLE) return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (slots_[earlier].fd_ == slot.fd_) return false;
    }
    return true;
}

}  // namespace av
