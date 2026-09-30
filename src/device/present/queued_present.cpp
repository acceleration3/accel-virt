#include "device/present/queued_present.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/control/region_mover.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

static_assert(QueuedPresentRequest::max_entries <= GuestPageMapping::max_entries,
              "every page list a request may carry is one the device maps");
static_assert(QueuedPresentRequest::entry_size == GuestPageMapping::entry_size, "one memory entry, one layout");

QueuedPresent::QueuedPresent(Host &host)
    : source_(host), destination_(host), present_(), regions_(), refusals_after_(0u)
{
    AV_SCOPE("QueuedPresent::QueuedPresent");
}

Status QueuedPresent::accept(ControlRequest &request, const QueuedPresentRequest &present)
{
    AV_SCOPE("QueuedPresent::accept");
    if (present.source() == QueuedPresentRequest::Source::image) return Status::not_supported;
    present_ = present;
    Status status = map(request);
    if (status == Status::ok) status = read_regions(request);
    if (status != Status::ok) release();
    return status;
}

Status QueuedPresent::map(ControlRequest &request)
{
    AV_SCOPE("QueuedPresent::map");
    const PresentCopyRequest &copy = present_.copy();
    if (copy.operation() == PresentOperation::blt) {
        const Status mapped = source_.map(request, QueuedPresentRequest::encoded_size, copy.source_entries());
        if (mapped != Status::ok) return mapped;
    }
    return destination_.map(request, present_.destination_entries_offset(), copy.destination_entries());
}

Status QueuedPresent::read_regions(ControlRequest &request)
{
    AV_SCOPE("QueuedPresent::read_regions");
    for (uint32_t index = 0u; index < present_.copy().regions(); index++) {
        uint8_t    bytes[PresentRegion::encoded_size] = {};
        WireReader reader(bytes, request.read(present_.regions_offset() + index * PresentRegion::encoded_size, bytes,
                                              sizeof bytes));
        if (regions_[index].decode(reader) != Status::ok) return Status::invalid_parameter;
    }
    return Status::ok;
}

Status QueuedPresent::move()
{
    AV_SCOPE("QueuedPresent::move");
    RegionMover mover(source_.window(), destination_.window(), present_.copy());
    for (uint32_t index = 0u; index < present_.copy().regions(); index++)
        if (mover.move(regions_[index]) != Status::ok) return Status::invalid_parameter;
    return Status::ok;
}

void QueuedPresent::release()
{
    AV_SCOPE("QueuedPresent::release");
    source_.release();
    destination_.release();
    present_ = QueuedPresentRequest();
    refusals_after_ = 0u;
}

uint64_t QueuedPresent::ticket() const
{
    AV_SCOPE("QueuedPresent::ticket");
    return present_.ticket();
}

void QueuedPresent::follow_with_refusal()
{
    AV_SCOPE("QueuedPresent::follow_with_refusal");
    refusals_after_++;
}

uint64_t QueuedPresent::refusals_after() const
{
    AV_SCOPE("QueuedPresent::refusals_after");
    return refusals_after_;
}

bool QueuedPresent::invariants_hold() const
{
    AV_SCOPE("QueuedPresent::invariants_hold");
    if (!source_.invariants_hold() || !destination_.invariants_hold()) return false;
    const bool mapped = destination_.extents() != 0u;
    return mapped == (present_.ticket() != 0u) && (mapped || refusals_after_ == 0u);
}

}  // namespace av
