#include "guest/transport/kmt_transport.hpp"

#include "core/support/scope_exit.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/guest_connection.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

namespace av {

namespace {
constexpr uint32_t ring_index  = 0u;
constexpr uint32_t reply_index = 1u;
constexpr uint32_t dma_index   = 2u;
constexpr uint64_t page_bytes  = 4096u;
}  // namespace

KmtTransport::KmtTransport(GuestConnection &connection, const RvkContextRecord &record)
    : connection_(connection), record_(record), allocations_{}, dma_address_(0u), context_(0u), ring_(),
      reply_(), fences_(), dma_view_(nullptr), decode_arena_(nullptr, 0u), blobs_(), staging_{} {}

KmtTransport::~KmtTransport() { teardown(); }

Status KmtTransport::setup_allocation(uint64_t bytes, bool map_gpu, uint32_t &allocation, uint8_t *&view,
                                      uint64_t &address)
{
    Kmt           &kmt = connection_.kmt();
    const uint32_t device = connection_.device();
    uint8_t        descriptor[AllocationDescriptor::encoded_size] = {};
    static_cast<void>(AllocationDescriptor(AllocationKind::client, true, SurfaceLayout(), bytes, ScanoutTarget())
                          .encode(descriptor, sizeof descriptor));
    const Status created = kmt.create_allocation(device, descriptor, sizeof descriptor, allocation);
    if (created != Status::ok) return created;
    uint64_t     fence = 0u;
    const Status resident = kmt.make_resident(connection_.queue(), allocation, fence);
    if (resident != Status::ok) return resident;
    const Status waited = kmt.wait_for_fence(device, connection_.fence_object(), fence);
    if (waited != Status::ok) return waited;
    if (map_gpu) {
        const uint64_t pages = (bytes + page_bytes - 1u) / page_bytes;
        uint64_t       map_fence = 0u;
        const Status   mapped = kmt.map_gpu_virtual_address(connection_.queue(), allocation, pages, address,
                                                            map_fence);
        if (mapped != Status::ok) return mapped;
        const Status settled = kmt.wait_for_fence(device, connection_.fence_object(), map_fence);
        if (settled != Status::ok) return settled;
    }
    void        *mapped_view = nullptr;
    const Status locked = kmt.lock(device, allocation, mapped_view);
    if (locked != Status::ok) return locked;
    view = static_cast<uint8_t *>(mapped_view);
    return Status::ok;
}

Status KmtTransport::initialize()
{
    if (context_ != 0u) return Status::invalid_state;
    ScopeExit undo([&] { teardown(); });
    const uint8_t order = record_.ring_order();
    uint8_t      *ring_view = nullptr;
    uint8_t      *reply_view = nullptr;
    uint64_t      unused = 0u;
    const Status  ring_ready = setup_allocation(RingHeader::encoded_size + (uint64_t{1} << order), false,
                                                allocations_[ring_index], ring_view, unused);
    if (ring_ready != Status::ok) return ring_ready;
    const Status reply_ready = setup_allocation(reply_bytes, false, allocations_[reply_index], reply_view, unused);
    if (reply_ready != Status::ok) return reply_ready;
    const Status dma_ready = setup_allocation(dma_bytes, true, allocations_[dma_index], dma_view_, dma_address_);
    if (dma_ready != Status::ok) return dma_ready;
    const Status ring_bound = ring_.initialize(ring_view, order);
    if (ring_bound != Status::ok) return ring_bound;
    reply_ = ReplyReader(reply_view, reply_bytes);
    uint8_t      private_data[RvkContextRecord::encoded_size] = {};
    static_cast<void>(record_.encode(private_data, sizeof private_data));
    const Status made = connection_.kmt().create_context_virtual(connection_.device(), private_data,
                                                                 sizeof private_data, context_);
    if (made != Status::ok) return made;
    undo.dismiss();
    return Status::ok;
}

void KmtTransport::teardown()
{
    Kmt           &kmt = connection_.kmt();
    const uint32_t device = connection_.device();
    if (context_ != 0u) {
        static_cast<void>(kmt.destroy_context(context_));
        context_ = 0u;
    }
    for (uint32_t index = 0u; index < blob_count; index++) {
        if (allocations_[index] == 0u) continue;
        static_cast<void>(kmt.unlock(device, allocations_[index]));
        if (index == dma_index && dma_address_ != 0u) {
            static_cast<void>(kmt.free_gpu_virtual_address(connection_.adapter(), dma_address_,
                                                           dma_bytes / page_bytes));
            dma_address_ = 0u;
        }
        static_cast<void>(kmt.destroy_allocation(device, allocations_[index]));
        allocations_[index] = 0u;
    }
    dma_view_ = nullptr;
}

bool KmtTransport::established() const { return context_ != 0u; }

WireWriter KmtTransport::request_writer()
{
    return WireWriter(staging_ + CommandHeader::encoded_size, sizeof staging_ - CommandHeader::encoded_size);
}

TransportResult KmtTransport::publish(uint32_t command_id, WireWriter &writer, uint8_t flags, uint64_t &fence)
{
    fence = 0u;
    if (writer.status() != Status::ok || !established()) return TransportResult::failed;
    const uint32_t body = static_cast<uint32_t>(writer.offset());
    const uint32_t command_size = static_cast<uint32_t>(CommandHeader::encoded_size) + body;
    static_cast<void>(CommandHeader(command_id, command_size, flags).encode(staging_,
                                                                            CommandHeader::encoded_size));
    const uint64_t next = fences_.peek();
    if (next == 0u) return TransportResult::failed;
    uint64_t     begin = 0u;
    uint64_t     end = 0u;
    const Status written = ring_.write_batch(staging_, command_size, next, begin, end);
    if (written != Status::ok) return TransportResult::failed;
    fence = fences_.open();
    uint8_t      descriptor[SubmissionDescriptor::encoded_size] = {};
    static_cast<void>(SubmissionDescriptor(fence, begin, end).encode(descriptor, sizeof descriptor));
    for (size_t i = 0u; i < sizeof descriptor; i++) dma_view_[i] = descriptor[i];
    const Status submitted = connection_.kmt().submit(context_, dma_address_,
                                                      SubmissionDescriptor::encoded_size);
    if (submitted != Status::ok) return TransportResult::failed;
    return TransportResult::ok;
}

TransportResult KmtTransport::send_async(uint32_t command_id, WireWriter &writer)
{
    uint64_t fence = 0u;
    return publish(command_id, writer, 0u, fence);
}

TransportResult KmtTransport::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    uint64_t              fence = 0u;
    const TransportResult sent = publish(command_id, writer, CommandHeader::generate_reply, fence);
    if (sent != TransportResult::ok) return sent;
    const Status waited = connection_.kmt().wait_for_context(connection_.device(), context_);
    if (waited != Status::ok) return TransportResult::failed;
    fences_.close(fence);
    if (reply_.read(fence, reply) != Status::ok) return TransportResult::failed;
    return TransportResult::ok;
}

WireArena      &KmtTransport::decode_arena() { return decode_arena_; }
ClientBlobArena &KmtTransport::blobs()       { return blobs_; }

bool KmtTransport::invariants_hold() const
{
    /* An established context has its DMA buffer mapped; the ring and fence keep
     * their own invariants, tested with them. */
    return context_ == 0u || dma_view_ != nullptr;
}

}  // namespace av
