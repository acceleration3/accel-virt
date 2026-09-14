#include "core/rvk/rvk_context_binding.hpp"

#include "core/virtio/virtio_gpu_device.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/submission/submission_descriptor.hpp"

namespace av {

RvkContextBinding::RvkContextBinding(Platform &platform, RegisterWindow &doorbells,
                                     const RvkContextRecord &record, uint32_t slot, uint8_t *request,
                                     size_t capacity)
    : context_(platform, doorbells), record_(record), context_id_(slot + 1u), doorbell_(slot),
      first_resource_id_(first_resource_id + resources_per_context * slot), request_(request), capacity_(capacity),
      answer_{} {}

Status RvkContextBinding::set_up(VirtioGpuDevice &device, const CapsetInfo &capset, const ContextPages &pages)
{
    const ContextSetup setup(capset.version(), record_.sub_api(), first_resource_id_, record_.ring_order(),
                             first_resource_id_ + 1u, doorbell_, first_resource_id_ + 2u);
    const Status begun = context_.set_up(capset, context_id_, setup, pages);
    if (begun != Status::ok) return begun;
    run_sequence(device);
    return context_.state() == GuestContext::State::established ? Status::ok : Status::not_supported;
}

Status RvkContextBinding::tear_down(VirtioGpuDevice &device)
{
    const Status begun = context_.tear_down();
    if (begun != Status::ok) return begun;
    run_sequence(device);
    return context_.release();
}

void RvkContextBinding::abandon()
{
    context_.abandon();
    /* Abandoned, the context is torn down, which release always accepts. */
    static_cast<void>(context_.release());
}

Status RvkContextBinding::submit(const SubmissionDescriptor &submission)
{
    return context_.submit(submission);
}

Status RvkContextBinding::serve() { return context_.serve(); }

/* Carries the sequence's requests to the device and its answers back, one at a
 * time, until the sequence has no more; a device that stops answering ends it,
 * and the context's state says whether it established or failed. */
void RvkContextBinding::run_sequence(VirtioGpuDevice &device)
{
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

GuestContext::State RvkContextBinding::state() const { return context_.state(); }
bool                RvkContextBinding::established() const { return context_.state() == GuestContext::State::established; }
uint32_t            RvkContextBinding::context_id() const { return context_id_; }
uint64_t            RvkContextBinding::completed() const { return context_.completed(); }
bool                RvkContextBinding::faulted() const { return context_.faulted(); }

bool RvkContextBinding::invariants_hold() const { return context_.invariants_hold(); }

}  // namespace av
