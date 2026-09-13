#include "guest/transport/guest_connection.hpp"

#include "core/support/scope_exit.hpp"
#include "guest/kmt/kmt.hpp"

namespace av {

GuestConnection::GuestConnection(Kmt &kmt)
    : kmt_(kmt), adapter_(0u), device_(0u), queue_(0u), fence_object_(0u) {}

Status GuestConnection::open()
{
    if (device_ != 0u) return Status::invalid_state;
    uint32_t adapters[1] = {};
    uint32_t count = 0u;
    const Status enumerated = kmt_.enumerate_adapters(adapters, 1u, count);
    if (enumerated != Status::ok) return enumerated;
    if (count == 0u || adapters[0] == 0u) return Status::not_found;
    ScopeExit close_adapter([&] { static_cast<void>(kmt_.close_adapter(adapters[0])); });
    uint32_t device = 0u;
    const Status made_device = kmt_.create_device(adapters[0], device);
    if (made_device != Status::ok) return made_device;
    ScopeExit destroy_device([&] { static_cast<void>(kmt_.destroy_device(device)); });
    uint32_t queue = 0u;
    uint32_t fence_object = 0u;
    const Status made_queue = kmt_.create_paging_queue(device, queue, fence_object);
    if (made_queue != Status::ok) return made_queue;
    close_adapter.dismiss();
    destroy_device.dismiss();
    adapter_ = adapters[0];
    device_ = device;
    queue_ = queue;
    fence_object_ = fence_object;
    return Status::ok;
}

void GuestConnection::close()
{
    if (queue_ != 0u) static_cast<void>(kmt_.destroy_paging_queue(queue_));
    if (device_ != 0u) static_cast<void>(kmt_.destroy_device(device_));
    if (adapter_ != 0u) static_cast<void>(kmt_.close_adapter(adapter_));
    adapter_ = 0u;
    device_ = 0u;
    queue_ = 0u;
    fence_object_ = 0u;
}

bool     GuestConnection::opened() const       { return device_ != 0u; }
Kmt     &GuestConnection::kmt() const          { return kmt_; }
uint32_t GuestConnection::adapter() const      { return adapter_; }
uint32_t GuestConnection::device() const       { return device_; }
uint32_t GuestConnection::queue() const        { return queue_; }
uint32_t GuestConnection::fence_object() const { return fence_object_; }

bool GuestConnection::invariants_hold() const
{
    /* A device open means an adapter and a queue open with it. */
    return device_ == 0u || (adapter_ != 0u && queue_ != 0u);
}

}  // namespace av
