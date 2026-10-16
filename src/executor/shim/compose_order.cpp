#include "executor/shim/compose_order.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/d3d11_host_device.hpp"

namespace av {

ComposeOrder::ComposeOrder() : device11_(nullptr), waited_(), value_(0u)
{
    AV_SCOPE("ComposeOrder::ComposeOrder");
}

bool ComposeOrder::connect(D3d11HostDevice &device11, OwnedFd &descriptor)
{
    AV_SCOPE("ComposeOrder::connect");
    if (!device11.vulkan().import_timeline(descriptor, waited_)) return false;
    device11_ = &device11;
    value_ = 0u;
    return true;
}

bool ComposeOrder::connected() const
{
    AV_SCOPE("ComposeOrder::connected");
    return waited_.made();
}

uint64_t ComposeOrder::next() const
{
    AV_SCOPE("ComposeOrder::next");
    return value_ + 1u;
}

uint64_t ComposeOrder::value() const
{
    AV_SCOPE("ComposeOrder::value");
    return value_;
}

bool ComposeOrder::presented()
{
    AV_SCOPE("ComposeOrder::presented");
    if (device11_ == nullptr) return false;
    value_++;
    return device11_->vulkan().wait(waited_, value_);
}

/* DXVK's idle flushes the immediate context: under the Direct3D 11 lock. */
void ComposeOrder::release()
{
    AV_SCOPE("ComposeOrder::release");
    if (waited_.made() && device11_ != nullptr) {
        device11_->lock(nullptr);
        device11_->vulkan().idle();
        waited_.release();
        device11_->unlock();
    }
    device11_ = nullptr;
    value_ = 0u;
}

}  // namespace av
