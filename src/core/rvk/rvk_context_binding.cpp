#include "core/rvk/rvk_context_binding.hpp"

#include "core/rvk/ring_submitter.hpp"

#include "core/trace/function_scope.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/rvk_request_writer.hpp"

namespace av {

RvkContextBinding::RvkContextBinding(Platform &platform, RegisterWindow &doorbells,
                                     const RvkContextRecord &record, uint32_t slot, uint32_t generation,
                                     uint8_t *request, size_t capacity)
    : context_(platform, doorbells), record_(record),
      context_id_(slot + RingSubmitter::doorbell_pages * (generation % generations) + 1u), doorbell_(slot),
      first_resource_id_(first_resource_id +
                         resources_per_context * (slot + RingSubmitter::doorbell_pages * (generation % generations))),
      request_(request), capacity_(capacity),
      answer_{}
      {
          AV_SCOPE("RvkContextBinding::RvkContextBinding");
      }

Status RvkContextBinding::set_up(VirtioGpuDevice &device, const CapsetInfo &capset, const ContextPages &pages)
{
    AV_SCOPE("RvkContextBinding::set_up");
    const ProtocolVersion ours(ProtocolVersion::current_major, ProtocolVersion::current_minor);
    const ContextSetup    setup(capset.version().negotiated_with(ours), record_.sub_api(), first_resource_id_,
                                record_.ring_order(), first_resource_id_ + 1u, doorbell_, first_resource_id_ + 2u,
                                record_.options());
    if (!setup.options_fit()) return Status::invalid_parameter;
    const Status begun = context_.set_up(capset, context_id_, setup, pages);
    if (begun != Status::ok) return begun;
    run_sequence(device);
    return context_.state() == GuestContext::State::established ? Status::ok : Status::not_supported;
}

Status RvkContextBinding::tear_down(VirtioGpuDevice &device)
{
    AV_SCOPE("RvkContextBinding::tear_down");
    const Status begun = context_.tear_down();
    if (begun != Status::ok) return begun;
    run_sequence(device);
    return context_.release();
}

void RvkContextBinding::abandon()
{
    AV_SCOPE("RvkContextBinding::abandon");
    context_.abandon();
    /* Abandoned, the context is torn down, which release always accepts. */
    static_cast<void>(context_.release());
}

Status RvkContextBinding::submit(const SubmissionDescriptor &submission)
{
    AV_SCOPE("RvkContextBinding::submit");
    return context_.submit(submission);
}

void RvkContextBinding::kick()
{
    AV_SCOPE("RvkContextBinding::kick");
    context_.kick();
}

void RvkContextBinding::user_mode_submits(uint64_t *progress)
{
    AV_SCOPE("RvkContextBinding::user_mode_submits");
    context_.user_mode_submits(progress);
}

void RvkContextBinding::driver_submits()
{
    AV_SCOPE("RvkContextBinding::driver_submits");
    context_.driver_submits();
}

bool RvkContextBinding::forget_progress(const uint64_t *progress)
{
    AV_SCOPE("RvkContextBinding::forget_progress");
    return context_.forget_progress(progress);
}

Status RvkContextBinding::serve()
{
    AV_SCOPE("RvkContextBinding::serve");
    return context_.serve();
}

/* Carries the sequence's requests to the device and its answers back, one at a
 * time, until the sequence has no more; a device that stops answering ends it,
 * and the context's state says whether it established or failed. */
void RvkContextBinding::run_sequence(VirtioGpuDevice &device)
{
    AV_SCOPE("RvkContextBinding::run_sequence");
    for (;;) {
        size_t       length = 0u;
        const Status next = context_.next_request(request_, capacity_, length);
        if (next != Status::ok) break;
        uint32_t     written = 0u;
        const Status asked = device.ask_control(request_, length, answer_, sizeof answer_, written);
        if (asked != Status::ok) break;
        static_cast<void>(context_.accept(answer_, written));
    }
}

uint32_t RvkContextBinding::chunk_resource(uint32_t index) const
{
    AV_SCOPE("RvkContextBinding::chunk_resource");
    return ContextResources(first_resource_id_).chunk(index);
}

Status RvkContextBinding::publish_chunk(VirtioGpuDevice &device, uint32_t resource_id, const GuestPageList &pages)
{
    AV_SCOPE("RvkContextBinding::publish_chunk");
    RvkRequestWriter writer(request_, capacity_);
    const Status     written = writer.guest_blob_pages(resource_id, pages);
    if (written != Status::ok) return written;
    uint32_t     answered = 0u;
    const Status asked = device.ask_control(request_, writer.length(), answer_, sizeof answer_, answered);
    if (asked != Status::ok) return asked;
    GpuResponseReader answer;
    if (answer.decode(answer_, answered) != Status::ok) return Status::malformed;
    return answer.type() == GpuResponse::ok_nodata ? Status::ok : Status::not_supported;
}

void RvkContextBinding::release_chunk(VirtioGpuDevice &device, uint32_t resource_id)
{
    AV_SCOPE("RvkContextBinding::release_chunk");
    RvkRequestWriter writer(request_, capacity_);
    if (writer.unref(resource_id) != Status::ok) return;
    uint32_t answered = 0u;
    static_cast<void>(device.ask_control(request_, writer.length(), answer_, sizeof answer_, answered));
}

GuestContext::State RvkContextBinding::state() const
{
    AV_SCOPE("RvkContextBinding::state");
    return context_.state();
}
bool                RvkContextBinding::established() const
{
    AV_SCOPE("RvkContextBinding::established");
    return context_.state() == GuestContext::State::established;
}
uint32_t            RvkContextBinding::context_id() const
{
    AV_SCOPE("RvkContextBinding::context_id");
    return context_id_;
}
SubApi              RvkContextBinding::sub_api() const
{
    AV_SCOPE("RvkContextBinding::sub_api");
    return record_.sub_api();
}
uint64_t            RvkContextBinding::completed() const
{
    AV_SCOPE("RvkContextBinding::completed");
    return context_.completed();
}
bool                RvkContextBinding::faulted() const
{
    AV_SCOPE("RvkContextBinding::faulted");
    return context_.faulted();
}

bool RvkContextBinding::invariants_hold() const
{
    AV_SCOPE("RvkContextBinding::invariants_hold");
    return context_.invariants_hold();
}

}  // namespace av
