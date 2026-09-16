#include "core/rvk/capset_query.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/rvk_request_writer.hpp"

namespace av {

CapsetQuery::CapsetQuery(Platform &platform)
    : platform_(platform), step_(Step::send_info), capset_(), described_(false)
    {
        AV_SCOPE("CapsetQuery::CapsetQuery");
    }

Status CapsetQuery::next_request(uint8_t *bytes, size_t capacity, size_t &length)
{
    AV_SCOPE("CapsetQuery::next_request");
    platform_.require_irql_at_most(Irql::dispatch, "CapsetQuery::next_request");
    length = 0u;
    if (step_ != Step::send_info && step_ != Step::send_capset) return Status::invalid_state;
    RvkRequestWriter writer(bytes, capacity);
    const Status status = step_ == Step::send_info ? writer.capset_info(0u)
                                                   : writer.capset(CapsetInfo::capset_id, capset_version);
    if (status != Status::ok) return status;
    length = writer.length();
    step_ = step_ == Step::send_info ? Step::await_info : Step::await_capset;
    return Status::ok;
}

Status CapsetQuery::accept(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("CapsetQuery::accept");
    platform_.require_irql_at_most(Irql::dispatch, "CapsetQuery::accept");
    if (step_ == Step::await_info) return accept_info(bytes, length);
    if (step_ == Step::await_capset) return accept_capset(bytes, length);
    return Status::invalid_state;
}

/* A device answers the one capset it has at index 0, big enough to be ours. */
Status CapsetQuery::accept_info(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("CapsetQuery::accept_info");
    GpuResponseReader answer;
    if (answer.decode(bytes, length) != Status::ok) return fail(Status::malformed, "capset info answer malformed");
    if (!answer.ok()) return fail(Status::not_supported, "the device refused capset info");
    if (answer.type() != GpuResponse::ok_capset_info) return fail(Status::malformed, "capset info answered wrong");
    if (answer.capset_id() != CapsetInfo::capset_id || answer.capset_max_version() < capset_version)
        return fail(Status::not_supported, "the device offers no rvk capset of this version");
    if (answer.capset_max_size() < CapsetInfo::encoded_size)
        return fail(Status::malformed, "the device's rvk capset is smaller than one");
    step_ = Step::send_capset;
    return Status::ok;
}

Status CapsetQuery::accept_capset(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("CapsetQuery::accept_capset");
    GpuResponseReader answer;
    if (answer.decode(bytes, length) != Status::ok) return fail(Status::malformed, "capset answer malformed");
    if (!answer.ok()) return fail(Status::not_supported, "the device refused the capset");
    if (answer.type() != GpuResponse::ok_capset) return fail(Status::malformed, "capset answered wrong");
    if (!answer.capset().version().compatible_with(ProtocolVersion(ProtocolVersion::current_major,
                                                                   ProtocolVersion::current_minor)))
        return fail(Status::not_supported, "the device speaks another rvk major");
    capset_ = answer.capset();
    described_ = true;
    step_ = Step::done;
    return Status::ok;
}

Status CapsetQuery::fail(Status status, const char *reason)
{
    AV_SCOPE("CapsetQuery::fail");
    platform_.log(reason);
    step_ = Step::failed;
    return status;
}

bool CapsetQuery::complete() const
{
    AV_SCOPE("CapsetQuery::complete");
    return step_ == Step::done;
}

bool CapsetQuery::failed() const
{
    AV_SCOPE("CapsetQuery::failed");
    return step_ == Step::failed;
}

const CapsetInfo &CapsetQuery::capset() const
{
    AV_SCOPE("CapsetQuery::capset");
    return capset_;
}

bool CapsetQuery::invariants_hold() const
{
    AV_SCOPE("CapsetQuery::invariants_hold");
    return step_ != Step::done || described_;
}

}  // namespace av
