#include "device/context/device_context.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "device/abi/entry_points.hpp"
#include "device/fence/fence_window.hpp"
#include "device/ring/ring_window.hpp"
#include "device/threading/work_item.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/submission/reply_header.hpp"

namespace av {

DeviceContext::DeviceContext(Host &host, BlobTable &blobs, FenceInterrupts &interrupts,
                             const ExecutorLibrary &library, uint32_t id)
    : id_(id), binding_(host, blobs), fences_(host, interrupts), imports_(host, blobs, executor_),
      executor_(host, library), staging_(host), batches_(host, executor_, imports_, staging_, fences_), ring_(host, fences_, batches_), worker_(host, ring_)
      {
          AV_SCOPE("DeviceContext::DeviceContext");
      }

Status DeviceContext::start()
{
    AV_SCOPE("DeviceContext::start");
    return worker_.start();
}

Status DeviceContext::post(const WorkItem &item)
{
    AV_SCOPE("DeviceContext::post");
    return worker_.post(item);
}

/* The attachment is posted last: once the worker may read the ring, nothing
 * here can fail. */
Status DeviceContext::bind(const ContextSetup &setup, const CapsetInfo &capset)
{
    AV_SCOPE("DeviceContext::bind");
    Status status = binding_.bind(setup, capset);
    if (status != Status::ok) return status;
    ScopeExit unbind([this] { binding_.unbind(); });
    ScopeExit withdraw([this] {
        ring_.withdraw();
        fences_.withdraw();
        batches_.withdraw();
        staging_.release();
    });
    if (fences_.prepare(binding_.fence_window(), setup.doorbell()) != Status::ok ||
        ring_.prepare(binding_.ring_window()) != Status::ok)
        return Status::invalid_parameter;
    const BlobWindow reply = binding_.reply_window();
    const uint64_t   answer_bytes = reply.length() != 0u ? reply.length() - ReplyHeader::encoded_size : 0u;
    status = staging_.allocate(uint64_t{1} << setup.ring_order(), answer_bytes);
    if (status != Status::ok) return status;
    executor_.configure(setup.sub_api(), capset.gpu().uuid());
    imports_.configure(setup.ring_resource());
    batches_.prepare(reply);
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
    AV_SCOPE("DeviceContext::binding");
    return binding_;
}

uint32_t DeviceContext::id() const
{
    AV_SCOPE("DeviceContext::id");
    return id_;
}

bool DeviceContext::running() const
{
    AV_SCOPE("DeviceContext::running");
    return worker_.running();
}

}  // namespace av
