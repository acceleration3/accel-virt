#include "guest/transport/guest_connection.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av {

GuestConnection::GuestConnection(Kmt &kmt)
    : kmt_(kmt), adapter_(0u), device_(0u), queue_(0u), fence_object_(0u)
    {
        AV_SCOPE("GuestConnection::GuestConnection");
    }

Status GuestConnection::open(uint32_t adapter)
{
    AV_SCOPE("GuestConnection::open");
    if (adapter == 0u) return Status::invalid_parameter;
    if (device_ != 0u) return Status::invalid_state;
    uint32_t device = 0u;
    const Status made_device = kmt_.create_device(adapter, device);
    if (made_device != Status::ok) return made_device;
    ScopeExit destroy_device([&] { static_cast<void>(kmt_.destroy_device(device)); });
    uint32_t queue = 0u;
    uint32_t fence_object = 0u;
    const Status made_queue = kmt_.create_paging_queue(device, queue, fence_object);
    if (made_queue != Status::ok) return made_queue;
    destroy_device.dismiss();
    adapter_ = adapter;
    device_ = device;
    queue_ = queue;
    fence_object_ = fence_object;
    return Status::ok;
}

void GuestConnection::close()
{
    AV_SCOPE("GuestConnection::close");
    if (queue_ != 0u) static_cast<void>(kmt_.destroy_paging_queue(queue_));
    if (device_ != 0u) static_cast<void>(kmt_.destroy_device(device_));
    adapter_ = 0u;
    device_ = 0u;
    queue_ = 0u;
    fence_object_ = 0u;
}

bool     GuestConnection::opened() const
{
    AV_SCOPE("GuestConnection::opened");
    return device_ != 0u;
}
Kmt     &GuestConnection::kmt() const
{
    AV_SCOPE("GuestConnection::kmt");
    return kmt_;
}
uint32_t GuestConnection::adapter() const
{
    AV_SCOPE("GuestConnection::adapter");
    return adapter_;
}
uint32_t GuestConnection::device() const
{
    AV_SCOPE("GuestConnection::device");
    return device_;
}
uint32_t GuestConnection::queue() const
{
    AV_SCOPE("GuestConnection::queue");
    return queue_;
}
uint32_t GuestConnection::fence_object() const
{
    AV_SCOPE("GuestConnection::fence_object");
    return fence_object_;
}

bool GuestConnection::invariants_hold() const
{
    AV_SCOPE("GuestConnection::invariants_hold");
    return device_ == 0u || (adapter_ != 0u && queue_ != 0u);
}

}
