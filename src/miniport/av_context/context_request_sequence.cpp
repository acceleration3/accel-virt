#include "miniport/av_context/context_request_sequence.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"

namespace av {

ContextRequestSequence::ContextRequestSequence(Platform &platform)
    : platform_(platform), phase_(Phase::idle), setup_(), context_id_(0u), pages_(), step_(0u),
      awaiting_(false), context_created_(false), blobs_created_(0u), refusal_(0u), refused_(false)
      {
          AV_SCOPE("ContextRequestSequence::ContextRequestSequence");
      }

Status ContextRequestSequence::begin_setup(uint32_t context_id, const ContextSetup &setup, const ContextPages &pages)
{
    AV_SCOPE("ContextRequestSequence::begin_setup");
    platform_.require_irql_at_most(Irql::dispatch, "ContextRequestSequence::begin_setup");
    if (phase_ != Phase::idle) return Status::invalid_state;
    if (!pages.complete()) return Status::invalid_parameter;
    setup_ = setup;
    context_id_ = context_id;
    pages_ = pages;
    phase_ = Phase::setting_up;
    step_ = 0u;
    refused_ = false;
    refusal_ = 0u;
    return Status::ok;
}

Status ContextRequestSequence::begin_teardown()
{
    AV_SCOPE("ContextRequestSequence::begin_teardown");
    platform_.require_irql_at_most(Irql::dispatch, "ContextRequestSequence::begin_teardown");
    if (phase_ == Phase::setting_up || phase_ == Phase::tearing_down || teardown_steps() == 0u)
        return Status::invalid_state;
    phase_ = Phase::tearing_down;
    step_ = context_step;
    return Status::ok;
}

Status ContextRequestSequence::next_request(uint8_t *bytes, size_t capacity, size_t &length)
{
    AV_SCOPE("ContextRequestSequence::next_request");
    platform_.require_irql_at_most(Irql::dispatch, "ContextRequestSequence::next_request");
    length = 0u;
    if (awaiting_ || (phase_ != Phase::setting_up && phase_ != Phase::tearing_down)) return Status::invalid_state;
    const Status status = phase_ == Phase::setting_up ? write_setup_request(bytes, capacity, length)
                                                      : write_teardown_request(bytes, capacity, length);
    awaiting_ = status == Status::ok;
    return status;
}

Status ContextRequestSequence::write_setup_request(uint8_t *bytes, size_t capacity, size_t &length)
{
    AV_SCOPE("ContextRequestSequence::write_setup_request");
    AvRequestWriter    writer(bytes, capacity);
    const uint32_t      resources[blob_count] = {setup_.ring_resource(), setup_.fence_resource(), setup_.reply_resource()};
    const GuestPageList blobs[blob_count] = {pages_.ring(), pages_.fence(), pages_.reply()};
    Status              status = Status::ok;
    if (step_ == context_step) status = writer.create_context(context_id_, CapsetInfo::capset_id);
    else if (step_ == setup_step) status = writer.setup(context_id_, setup_);
    else status = writer.guest_blob_pages(resources[step_ - first_blob_step], blobs[step_ - first_blob_step]);
    length = writer.length();
    return status;
}

/* Step 0 destroys the context; steps 1 to blobs_created_ release the blobs. */
Status ContextRequestSequence::write_teardown_request(uint8_t *bytes, size_t capacity, size_t &length)
{
    AV_SCOPE("ContextRequestSequence::write_teardown_request");
    AvRequestWriter writer(bytes, capacity);
    const uint32_t   resources[blob_count] = {setup_.ring_resource(), setup_.fence_resource(), setup_.reply_resource()};
    const Status     status = step_ == context_step ? writer.destroy_context(context_id_)
                                                    : writer.unref(resources[step_ - first_blob_step]);
    length = writer.length();
    return status;
}

Status ContextRequestSequence::accept(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ContextRequestSequence::accept");
    platform_.require_irql_at_most(Irql::dispatch, "ContextRequestSequence::accept");
    if (!awaiting_) return Status::invalid_state;
    awaiting_ = false;
    GpuResponseReader answer;
    const bool readable = answer.decode(bytes, length) == Status::ok;
    const bool done = readable && answer.type() == GpuResponse::ok_nodata;
    if (phase_ == Phase::setting_up) finish_setup_step(done || !readable);
    else step_++;
    if (phase_ == Phase::tearing_down && step_ > blobs_created_) abandon();
    if (!readable) {
        refuse(unreadable);
        phase_ = phase_ == Phase::setting_up ? Phase::set_up : phase_;
        return Status::malformed;
    }
    if (done) return Status::ok;
    refuse(answer.type());
    return Status::not_supported;
}

/* A step that took effect, or may have, is counted for teardown; a refused
 * setup ends. */
void ContextRequestSequence::finish_setup_step(bool done)
{
    AV_SCOPE("ContextRequestSequence::finish_setup_step");
    if (done && step_ == context_step) context_created_ = true;
    if (done && step_ >= first_blob_step && step_ < setup_step) blobs_created_++;
    step_++;
    if (!done || step_ == setup_steps) phase_ = Phase::set_up;
}

void ContextRequestSequence::abandon()
{
    AV_SCOPE("ContextRequestSequence::abandon");
    platform_.require_irql_at_most(Irql::dispatch, "ContextRequestSequence::abandon");
    awaiting_ = false;
    phase_ = Phase::idle;
    context_created_ = false;
    blobs_created_ = 0u;
}

void ContextRequestSequence::refuse(uint32_t type)
{
    AV_SCOPE("ContextRequestSequence::refuse");
    if (!refused_) refusal_ = type;
    refused_ = true;
    platform_.log("av: the device refused a context request");
}

/* The context itself, and each blob. */
uint32_t ContextRequestSequence::teardown_steps() const
{
    AV_SCOPE("ContextRequestSequence::teardown_steps");
    return (context_created_ ? 1u : 0u) + blobs_created_;
}

bool ContextRequestSequence::finished() const
{
    AV_SCOPE("ContextRequestSequence::finished");
    return !awaiting_ && (phase_ == Phase::set_up || phase_ == Phase::idle);
}

bool     ContextRequestSequence::refused() const
{
    AV_SCOPE("ContextRequestSequence::refused");
    return refused_;
}
uint32_t ContextRequestSequence::refusal() const
{
    AV_SCOPE("ContextRequestSequence::refusal");
    return refusal_;
}

bool ContextRequestSequence::invariants_hold() const
{
    AV_SCOPE("ContextRequestSequence::invariants_hold");
    return blobs_created_ <= blob_count && (context_created_ || blobs_created_ == 0u);
}

}  // namespace av
