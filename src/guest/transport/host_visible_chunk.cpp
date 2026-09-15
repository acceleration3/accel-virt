#include "guest/transport/host_visible_chunk.hpp"

#include "guest/kmt/kmt.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/host_visible_bind_record.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/surface_layout.hpp"

namespace av {

HostVisibleChunk::HostVisibleChunk(GuestConnection &connection, KmtTransport &transport)
    : connection_(connection), transport_(transport), allocation_(0u), view_(nullptr), resource_id_(0u),
      pages_(0u) {}

HostVisibleChunk::~HostVisibleChunk() { static_cast<void>(release()); }

Status HostVisibleChunk::allocate(uint64_t bytes)
{
    if (allocation_ != 0u) return Status::invalid_state;
    Kmt           &kmt = connection_.kmt();
    const uint32_t device = connection_.device();
    uint8_t        descriptor[AllocationDescriptor::encoded_size] = {};
    static_cast<void>(AllocationDescriptor(AllocationKind::host_visible_memory, true, SurfaceLayout(), bytes,
                                           ScanoutTarget())
                          .encode(descriptor, sizeof descriptor));   /* the buffer is the encoded size */
    const Status created = kmt.create_allocation(device, descriptor, sizeof descriptor, allocation_);
    if (created != Status::ok) return created;
    uint64_t     fence = 0u;
    const Status resident = kmt.make_resident(connection_.queue(), allocation_, fence);
    if (resident != Status::ok) return resident;
    return kmt.wait_for_fence(device, connection_.fence_object(), fence);
}

Status HostVisibleChunk::lock()
{
    if (allocation_ == 0u || view_ != nullptr) return Status::invalid_state;
    void        *locked = nullptr;
    const Status status = connection_.kmt().lock(connection_.device(), allocation_, locked);
    if (status == Status::ok) view_ = static_cast<unsigned char *>(locked);
    return status;
}

Status HostVisibleChunk::bind()
{
    if (allocation_ == 0u || resource_id_ != 0u) return Status::invalid_state;
    HostVisibleBindRecord record(HostVisibleBindRecord::Operation::bind, allocation_, 0u);
    uint8_t               bytes[HostVisibleBindRecord::encoded_size] = {};
    static_cast<void>(record.encode(bytes, sizeof bytes));   /* the buffer is the encoded size */
    const Status escaped = connection_.kmt().escape(connection_.adapter(), connection_.device(),
                                                    transport_.context(), bytes, sizeof bytes);
    if (escaped != Status::ok) return escaped;
    const Status answered = record.accept_answer(bytes, sizeof bytes);
    if (answered != Status::ok) return answered;
    resource_id_ = record.resource_id();
    pages_ = record.pages();
    return Status::ok;
}

Status HostVisibleChunk::release()
{
    Kmt           &kmt = connection_.kmt();
    const uint32_t device = connection_.device();
    Status         first = Status::ok;
    const auto     keep = [&first](Status status) { first = first == Status::ok ? status : first; };
    if (resource_id_ != 0u) {
        const HostVisibleBindRecord unbind(HostVisibleBindRecord::Operation::unbind, 0u, resource_id_);
        uint8_t                     bytes[HostVisibleBindRecord::encoded_size] = {};
        static_cast<void>(unbind.encode(bytes, sizeof bytes));   /* the buffer is the encoded size */
        keep(kmt.escape(connection_.adapter(), device, transport_.context(), bytes, sizeof bytes));
    }
    if (view_ != nullptr) keep(kmt.unlock(device, allocation_));
    if (allocation_ != 0u) keep(kmt.destroy_allocation(device, allocation_));
    resource_id_ = 0u;
    pages_ = 0u;
    view_ = nullptr;
    allocation_ = 0u;
    return first;
}

uint32_t                HostVisibleChunk::resource_id() const { return resource_id_; }
uint64_t                HostVisibleChunk::pages() const { return pages_; }
volatile unsigned char *HostVisibleChunk::view() const { return view_; }

/* A view and a binding are both of an allocation, and a bound chunk has pages. */
bool HostVisibleChunk::invariants_hold() const
{
    if (allocation_ == 0u) return view_ == nullptr && resource_id_ == 0u;
    return (resource_id_ == 0u) == (pages_ == 0u);
}

}  // namespace av
