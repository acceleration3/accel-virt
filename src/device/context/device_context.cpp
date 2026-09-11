#include "device/context/device_context.hpp"

#include "core/support/scope_exit.hpp"
#include "device/abi/entry_points.hpp"
#include "device/fence/fence_window.hpp"
#include "device/ring/ring_window.hpp"
#include "device/threading/work_item.hpp"
#include "proto/context/context_setup.hpp"

namespace av {

DeviceContext::DeviceContext(Host &host, BlobTable &blobs, FenceInterrupts &interrupts, uint32_t id)
    : id_(id), binding_(host, blobs), fences_(host, interrupts), ring_(host, fences_), worker_(host, ring_) {}

Status DeviceContext::start()
{
    return worker_.start();
}

Status DeviceContext::post(const WorkItem &item)
{
    return worker_.post(item);
}

/* The attachment is posted last: once the worker may read the ring, nothing
 * here can fail. */
Status DeviceContext::bind(const ContextSetup &setup, const CapsetInfo &capset)
{
    Status status = binding_.bind(setup, capset);
    if (status != Status::ok) return status;
    ScopeExit unbind([this] { binding_.unbind(); });
    ScopeExit withdraw([this] {
        ring_.withdraw();
        fences_.withdraw();
    });
    if (fences_.prepare(binding_.fence_window(), setup.doorbell()) != Status::ok ||
        ring_.prepare(binding_.ring_window()) != Status::ok)
        return Status::invalid_parameter;
    status = worker_.connect_doorbell(setup.doorbell());
    if (status != Status::ok) return status;
    ScopeExit disconnect([this] { worker_.disconnect_doorbell(); });
    status = worker_.post(WorkItem(&RvkRingAttach, nullptr, &ring_));
    if (status != Status::ok) return status;
    disconnect.dismiss();
    withdraw.dismiss();
    unbind.dismiss();
    return Status::ok;
}

const ContextBinding &DeviceContext::binding() const
{
    return binding_;
}

uint32_t DeviceContext::id() const
{
    return id_;
}

bool DeviceContext::running() const
{
    return worker_.running();
}

}  // namespace av
