#include "guest/transport/kmt_transport.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/context/tail_publish_record.hpp"
#include "proto/submission/batch_header.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/guest_connection.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/command_stream.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

namespace av {

namespace {
constexpr uint64_t page_bytes = 4096u;
/* A ring counts a batch's bytes in 32 bits. */
constexpr size_t   max_batch = 0xFFFFFFFFu;
}  // namespace

KmtTransport::KmtTransport(GuestConnection &connection, SubApi sub_api, uint8_t ring_order, uint32_t reply_bytes,
                           uint8_t *staging, size_t staging_bytes)
    : connection_(connection), sub_api_(sub_api), ring_order_(ring_order), reply_bytes_(reply_bytes),
      staging_(staging), staging_bytes_(staging != nullptr ? staging_bytes : 0u), allocations_{}, locked_{},
      dma_address_(0u), context_(0u), hardware_scheduled_(false), ring_(), newest_end_(0u), newest_fence_(0u), previous_end_(0u), doorbell_(connection.kmt()),
      path_(connection.kmt(), doorbell_, ring_), reply_(), fences_(), fence_ends_{}, faulted_(false), answered_(false),
      decode_arena_(nullptr, 0u), blobs_()
{
    AV_SCOPE("KmtTransport::KmtTransport");
}

KmtTransport::~KmtTransport()
{
    AV_SCOPE("KmtTransport::~KmtTransport");
    teardown();
}

Status KmtTransport::setup_allocation(uint32_t index, AllocationKind kind, uint64_t bytes, bool map_gpu,
                                      bool do_lock, uint8_t *&view, uint64_t &address)
{
    AV_SCOPE("KmtTransport::setup_allocation");
    uint32_t      &allocation = allocations_[index];
    Kmt           &kmt = connection_.kmt();
    const uint32_t device = connection_.device();
    uint8_t        descriptor[AllocationDescriptor::encoded_size] = {};
    static_cast<void>(AllocationDescriptor(kind, true, SurfaceLayout(), bytes, ScanoutTarget())
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
    if (do_lock) {
        void        *mapped_view = nullptr;
        const Status locked = kmt.lock(device, allocation, mapped_view);
        if (locked != Status::ok) return locked;
        locked_[index] = true;
        view = static_cast<uint8_t *>(mapped_view);
    }
    return Status::ok;
}

Status KmtTransport::initialize(const GlContextOptions &options)
{
    AV_SCOPE("KmtTransport::initialize");
    if (context_ != 0u) return Status::invalid_state;
    if (staging_bytes_ <= CommandHeader::encoded_size) return Status::invalid_parameter;
    if (!record_carries(options)) return Status::invalid_parameter;
    ScopeExit undo([&] { teardown(); });
    uint8_t      *ring_view = nullptr;
    uint8_t      *reply_view = nullptr;
    uint8_t      *unlocked = nullptr;
    uint64_t      unused = 0u;
    /* A ring allocation is a power of two of pages (the driver's rule), and the
     * header and 2^order bytes of data fit the next one up. */
    const Status  ring_ready = setup_allocation(ring_index, AllocationKind::context_ring,
                                                uint64_t{2} << ring_order_, false, true, ring_view, unused);
    if (ring_ready != Status::ok) return ring_ready;
    /* The fence page is the device's to write and the driver's to read; the
     * guest only allocates it, so it is not locked here. */
    const Status fence_ready = setup_allocation(fence_index, AllocationKind::fence_page, page_bytes, false, false,
                                                unlocked, unused);
    if (fence_ready != Status::ok) return fence_ready;
    const Status reply_ready = setup_allocation(reply_index, AllocationKind::reply_slot, reply_bytes_, false, true,
                                                reply_view, unused);
    if (reply_ready != Status::ok) return reply_ready;
    /* The DMA buffer is only what dxgkrnl insists a submission names; the
     * descriptor travels as the submission's private data. */
    const Status dma_ready = setup_allocation(dma_index, AllocationKind::client, dma_bytes, true, false,
                                              unlocked, dma_address_);
    if (dma_ready != Status::ok) return dma_ready;
    const Status ring_bound = ring_.initialize(ring_view, ring_order_);
    if (ring_bound != Status::ok) return ring_bound;
    reply_ = ReplyReader(reply_view, reply_bytes_);
    /* The record names the ring, fence and reply by the handles just allocated,
     * so the miniport publishes exactly this context's memory. */
    const RvkContextRecord record(sub_api_, ring_order_, allocations_[ring_index], allocations_[fence_index],
                                  allocations_[reply_index], options);
    uint8_t      private_data[RvkContextRecord::encoded_size] = {};
    static_cast<void>(record.encode(private_data, sizeof private_data));
    /* A context a hardware queue may run on first: that is what a doorbell
     * hangs off, and a machine whose OS hands out no hardware queue refuses it
     * and gets the context this transport has always had (research.md §13). */
    bool doorbells = false;
    static_cast<void>(connection_.kmt().query_native_gpu_fences(connection_.adapter(), doorbells));
    Status made = doorbells ? connection_.kmt().create_hw_queue_context(connection_.device(), private_data,
                                                                        sizeof private_data, context_)
                            : Status::not_supported;
    hardware_scheduled_ = made == Status::ok;
    if (made != Status::ok)
        made = connection_.kmt().create_context_virtual(connection_.device(), private_data, sizeof private_data,
                                                        context_);
    if (made != Status::ok) return made;
    /* A hardware-scheduled context without a doorbell still submits, but
     * through D3DKMTSubmitCommandToHwQueue at ten times the cost of the plain
     * context's D3DKMTSubmitCommand, so none is asked for where the adapter
     * has no native fences - a doorbell needs them, and 26100 refuses them
     * (049 research.md §20) - and one whose doorbell is refused anyway is
     * given back for the plain one. */
    if (hardware_scheduled_ &&
        doorbell_.initialize(connection_.device(), context_, allocations_[ring_index]) == Status::ok) {
        static_cast<void>(doorbell_.connect());
    } else if (hardware_scheduled_) {
        static_cast<void>(connection_.kmt().destroy_context(context_));
        context_ = 0u;
        hardware_scheduled_ = false;
        const Status plain = connection_.kmt().create_context_virtual(connection_.device(), private_data,
                                                                      sizeof private_data, context_);
        if (plain != Status::ok) return plain;
    }
    if (!doorbell_.established()) {
        uint8_t escape[TailPublishRecord::encoded_size] = {};
        static_cast<void>(TailPublishRecord(ring()).encode(escape, sizeof escape));
        /* An older driver refuses it, and the kernel publishes as it always has. */
        if (connection_.kmt().escape(connection_.adapter(), connection_.device(), context_, escape, sizeof escape) ==
            Status::ok)
            path_.publish_before_kernel();
    }
    undo.dismiss();
    return Status::ok;
}

bool KmtTransport::record_carries(const GlContextOptions &options) const
{
    AV_SCOPE("KmtTransport::record_carries");
    /* Distinct stand-ins for the handles the allocations will have: the record
     * judges the options as the miniport will. */
    const RvkContextRecord candidate(sub_api_, ring_order_, 1u, 2u, 3u, options);
    uint8_t                bytes[RvkContextRecord::encoded_size] = {};
    static_cast<void>(candidate.encode(bytes, sizeof bytes));
    RvkContextRecord decoded;
    return decoded.decode(bytes, sizeof bytes) == Status::ok;
}

bool KmtTransport::ready_for_batch() const
{
    AV_SCOPE("KmtTransport::ready_for_batch");
    return ring_.head() >= previous_end_;
}

uint64_t KmtTransport::submissions_through_doorbell() const
{
    AV_SCOPE("KmtTransport::submissions_through_doorbell");
    return path_.through_doorbell();
}

uint64_t KmtTransport::submissions_through_kernel() const
{
    AV_SCOPE("KmtTransport::submissions_through_kernel");
    return path_.through_kernel();
}

uint64_t KmtTransport::submissions() const
{
    AV_SCOPE("KmtTransport::submissions");
    return path_.submitted();
}

uint32_t KmtTransport::ring() const
{
    AV_SCOPE("KmtTransport::ring");
    return context_ != 0u ? allocations_[ring_index] : 0u;
}

void KmtTransport::teardown()
{
    AV_SCOPE("KmtTransport::teardown");
    Kmt           &kmt = connection_.kmt();
    const uint32_t device = connection_.device();
    /* Before the context: a doorbell names a queue of it. */
    doorbell_.teardown();
    if (context_ != 0u) {
        static_cast<void>(kmt.destroy_context(context_));
        context_ = 0u;
    }
    for (uint32_t index = 0u; index < allocation_count; index++) {
        if (allocations_[index] == 0u) continue;
        if (locked_[index]) static_cast<void>(kmt.unlock(device, allocations_[index]));
        locked_[index] = false;
        if (index == dma_index && dma_address_ != 0u) {
            static_cast<void>(kmt.free_gpu_virtual_address(connection_.adapter(), dma_address_,
                                                           dma_bytes / page_bytes));
            dma_address_ = 0u;
        }
        static_cast<void>(kmt.destroy_allocation(device, allocations_[index]));
        allocations_[index] = 0u;
    }
    /* The ring and reply views are gone with their allocations, and a context
     * set up again numbers its fences from the start: without this a transport
     * torn down could never be initialized again. */
    ring_ = TransportRing();
    reply_ = ReplyReader();
    fences_ = TransportFence();
    faulted_ = false;
}

bool KmtTransport::established() const
{
    AV_SCOPE("KmtTransport::established");
    return context_ != 0u;
}
bool KmtTransport::faulted() const
{
    AV_SCOPE("KmtTransport::faulted");
    return faulted_;
}
uint32_t KmtTransport::context() const
{
    AV_SCOPE("KmtTransport::context");
    return context_;
}

WireWriter KmtTransport::request_writer()
{
    AV_SCOPE("KmtTransport::request_writer");
    if (staging_bytes_ <= CommandHeader::encoded_size) return WireWriter(nullptr, 0u);
    return WireWriter(staging_ + CommandHeader::encoded_size, staging_bytes_ - CommandHeader::encoded_size);
}

TransportResult KmtTransport::publish(uint32_t command_id, WireWriter &writer, uint8_t flags, uint64_t &fence)
{
    AV_SCOPE("KmtTransport::publish");
    fence = 0u;
    if (writer.status() != Status::ok || !established()) return TransportResult::failed;
    const uint32_t body = static_cast<uint32_t>(writer.offset());
    const uint32_t command_size = static_cast<uint32_t>(CommandHeader::encoded_size) + body;
    static_cast<void>(CommandHeader(command_id, command_size, flags).encode(staging_,
                                                                            CommandHeader::encoded_size));
    return publish_batch(staging_, command_size, fence);
}

/* The performance counter in 100 ns, the unit the trace timeline keeps: the
 * device orders batches by it, and Perfetto aligns guest and host by it. */
uint64_t KmtTransport::published_now() const
{
    AV_SCOPE("KmtTransport::published_now");
    constexpr uint64_t hundred_ns = 10000000u;
    uint64_t       frequency = 0u;
    const uint64_t ticks = connection_.kmt().performance_counter(frequency);
    if (frequency == 0u) return ticks;
    return ticks / frequency * hundred_ns + ticks % frequency * hundred_ns / frequency;
}

bool KmtTransport::drain()
{
    AV_SCOPE("KmtTransport::drain");
    const uint64_t newest = fences_.completed() + fences_.outstanding();
    if (fences_.outstanding() == 0u) return true;
    /* The device publishes its head as it finishes each batch; all taken is
     * all done, and that is usually a matter of microseconds away. */
    if (consumed_within_spin()) {
        fences_.close(newest);
        return true;
    }
    const Status waited = wait_for_host(newest);
    if (waited != Status::ok || !arrives_eventually([this] { return ring_.head() == ring_.tail(); })) {
        faulted_ = true;
        return false;
    }
    fences_.close(newest);
    return true;
}

/* The kernel's wait is bounded, so a host that died is reported rather than
 * waited on forever; but a host still working through what was queued ahead
 * of `fence` is alive, just behind. A wait that times out while the device's
 * head moved is waited again; only one in which the device took nothing at
 * all gives up. */
Status KmtTransport::wait_for_host(uint64_t fence)
{
    AV_SCOPE("KmtTransport::wait_for_host");
    for (;;) {
        const uint64_t head = ring_.head();
        const Status   waited = doorbell_.queue() != 0u
                                    ? connection_.kmt().wait_for_hw_queue(connection_.device(),
                                                                          doorbell_.progress_fence(), fence)
                                    : connection_.kmt().wait_for_context(connection_.device(), context_);
        if (waited == Status::ok || connection_.kmt().last_code() != wait_timed_out || ring_.head() == head)
            return waited;
    }
}

/* Every fence whose batch the device has moved its head past is done: the
 * device publishes the head as it finishes each batch. */
void KmtTransport::retire_consumed()
{
    AV_SCOPE("KmtTransport::retire_consumed");
    const uint64_t head = ring_.head();
    const uint64_t newest = fences_.completed() + fences_.outstanding();
    uint64_t       done = 0u;
    for (uint64_t fence = fences_.oldest(); fence != 0u && fence <= newest; fence++) {
        if (fence_ends_[fence % TransportFence::max_outstanding] > head) break;
        done = fence;
    }
    if (done != 0u) fences_.close(done);
}

/* A full fence window waits for its oldest batch, not for all of them: a
 * sender of many small batches - a flush a frame - drained the device empty
 * every 64, and then refilled it from nothing. */
bool KmtTransport::room_for_fence()
{
    AV_SCOPE("KmtTransport::room_for_fence");
    retire_consumed();
    if (!fences_.full()) return true;
    constexpr uint64_t spins_per_second = 1000u;
    uint64_t           frequency = 0u;
    const uint64_t     started = connection_.kmt().performance_counter(frequency);
    const uint64_t     budget = frequency / spins_per_second;
    while (connection_.kmt().performance_counter(frequency) - started < budget) {
        spin_once();
        retire_consumed();
        if (!fences_.full()) return true;
    }
    return drain();
}

TransportResult KmtTransport::publish_batch(const uint8_t *batch, uint32_t length, uint64_t &fence)
{
    AV_SCOPE("KmtTransport::publish_batch");
    /* Asynchronous batches open fences nobody waits on: a sender faster than
     * the host fills the fence window or the ring, and then waits for the
     * host to catch up rather than failing. */
    if (fences_.full() && !room_for_fence()) return TransportResult::failed;
    uint64_t next = fences_.peek();
    if (next == 0u) return TransportResult::failed;
    uint64_t begin = 0u;
    uint64_t end = 0u;
    Status   written = ring_.write_batch(batch, length, next, published_now(), begin, end);
    if (written == Status::no_space && room_within_spin(BatchHeader(length, next, 0u).span()))
        written = ring_.write_batch(batch, length, next, published_now(), begin, end);
    if (written == Status::no_space) {
        if (!drain()) return TransportResult::failed;
        next = fences_.peek();
        written = ring_.write_batch(batch, length, next, published_now(), begin, end);
    }
    if (written != Status::ok) return TransportResult::failed;
    previous_end_ = newest_end_;
    newest_end_ = end;
    fence = fences_.open();
    newest_fence_ = fence;
    fence_ends_[fence % TransportFence::max_outstanding] = end;
    uint8_t      descriptor[SubmissionDescriptor::encoded_size] = {};
    static_cast<void>(SubmissionDescriptor(fence, begin, end).encode(descriptor, sizeof descriptor));
    const Status submitted = path_.submit(context_, dma_address_, descriptor, SubmissionDescriptor::encoded_size,
                                          fence);
    if (submitted != Status::ok) {
        faulted_ = true;
        return TransportResult::failed;
    }
    return TransportResult::ok;
}

TransportResult KmtTransport::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("KmtTransport::send_async");
    if (faulted_) return TransportResult::failed;
    uint64_t fence = 0u;
    return publish(command_id, writer, 0u, fence);
}

void KmtTransport::end_call()
{
    AV_SCOPE("KmtTransport::end_call");
    if (!answered_) return;
    answered_ = false;
    blobs_.retire_all();
}

TransportResult KmtTransport::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("KmtTransport::send_sync");
    if (faulted_) return TransportResult::failed;
    uint64_t              fence = 0u;
    const TransportResult sent = publish(command_id, writer, CommandHeader::generate_reply, fence);
    if (sent != TransportResult::ok) return sent;
    const TransportResult answered = await(fence, reply);
    answered_ = answered == TransportResult::ok;
    return answered;
}

/* The batch is judged whole before a byte reaches the ring: a frame the device
 * would fault the context for is the caller's mistake, not the context's. */
/* For a stream that framed the batch itself and knows none of it answers:
 * reading every header back to learn that cost more than writing them, once
 * a Flush lets a batch grow to thousands of commands. */
TransportResult KmtTransport::send_async_batch(const uint8_t *framed, size_t length)
{
    AV_SCOPE("KmtTransport::send_async_batch");
    if (faulted_ || !established() || framed == nullptr || length == 0u || length > max_batch)
        return TransportResult::failed;
    uint64_t fence = 0u;
    return publish_batch(framed, static_cast<uint32_t>(length), fence);
}

uint64_t KmtTransport::marker_address() const
{
    AV_SCOPE("KmtTransport::marker_address");
    return dma_address_;
}

FollowRecord KmtTransport::follow() const
{
    AV_SCOPE("KmtTransport::follow");
    return FollowRecord(ring(), newest_fence_);
}

BatchMark KmtTransport::newest_mark() const
{
    AV_SCOPE("KmtTransport::newest_mark");
    const uint32_t fence_object = doorbell_.queue() != 0u ? doorbell_.progress_fence() : 0u;
    return BatchMark(connection_.device(), context_, fence_object, newest_fence_);
}

/* For a stream whose batch ends in the one command that answers. */
TransportResult KmtTransport::send_answered_batch(const uint8_t *framed, size_t length, WireReader &reply)
{
    AV_SCOPE("KmtTransport::send_answered_batch");
    reply = WireReader(nullptr, 0u);
    if (faulted_ || !established() || framed == nullptr || length == 0u || length > max_batch)
        return TransportResult::failed;
    uint64_t              fence = 0u;
    const TransportResult sent = publish_batch(framed, static_cast<uint32_t>(length), fence);
    if (sent != TransportResult::ok) return sent;
    const TransportResult answered = await(fence, reply);
    answered_ = answered == TransportResult::ok;
    return answered;
}

TransportResult KmtTransport::send_batch(const uint8_t *framed, size_t length, WireReader &reply)
{
    AV_SCOPE("KmtTransport::send_batch");
    reply = WireReader(nullptr, 0u);
    if (faulted_ || !established() || framed == nullptr || length == 0u || length > max_batch)
        return TransportResult::failed;
    CommandStream  stream(framed, length);
    CommandHeader  header;
    const uint8_t *payload = nullptr;
    bool           replying = false;
    Status         framing = Status::ok;
    while ((framing = stream.next(header, payload)) == Status::ok) replying = replying || header.generates_reply();
    if (framing != Status::no_space) return TransportResult::failed;
    uint64_t              fence = 0u;
    const TransportResult sent = publish_batch(framed, static_cast<uint32_t>(length), fence);
    if (sent != TransportResult::ok || !replying) return sent;
    const TransportResult answered = await(fence, reply);
    answered_ = answered == TransportResult::ok;
    return answered;
}

TransportResult KmtTransport::await(uint64_t fence, WireReader &reply)
{
    AV_SCOPE("KmtTransport::await");
    /* What a hardware-scheduled context has in flight is not in any packet the
     * kernel queued, so what it waits on is the queue's progress fence - the
     * monitored fence dxgkrnl made with the queue, which this driver writes
     * what the device finished into (research.md §12). */
    if (!replied_within_spin(fence)) {
        const Status waited = wait_for_host(fence);
        /* A batch the kernel was not told of is the device's alone, so the
         * kernel's wait can return before it: the slot is watched until the
         * answer lands. */
        if (waited != Status::ok || !arrives_eventually([this, fence] {
                WireReader   probe(nullptr, 0u);
                const Status read = reply_.read(fence, probe);
                return read == Status::ok || read == Status::too_large;
            })) {
            faulted_ = true;
            return TransportResult::failed;
        }
    }
    fences_.close(fence);
    if (reply_.read(fence, reply) != Status::ok) {
        faulted_ = true;
        return TransportResult::failed;
    }
    return TransportResult::ok;
}

/* No PAUSE in a guest's spin: KVM takes a run of them for a vCPU waiting on a
 * lock held elsewhere and descheduled it, so the answer arrived to a vCPU
 * that was not running. A compiler barrier keeps the loads in the loop. */
void KmtTransport::spin_once() const
{
    AV_SCOPE("KmtTransport::spin_once");
    __asm__ __volatile__("" ::: "memory");
}

/* A reply usually lands within tens of microseconds, and the kernel's wait -
 * a CPU notification the scheduler signals behind the context's work - takes
 * far longer to come back: the slot is watched for a millisecond first. The
 * device writes the fence id last, so seeing it means the answer is whole. */
bool KmtTransport::replied_within_spin(uint64_t fence)
{
    AV_SCOPE("KmtTransport::replied_within_spin");
    constexpr uint64_t spins_per_second = 1000u;
    uint64_t           frequency = 0u;
    const uint64_t     started = connection_.kmt().performance_counter(frequency);
    const uint64_t     budget = frequency / spins_per_second;
    for (;;) {
        WireReader   probe(nullptr, 0u);
        const Status read = reply_.read(fence, probe);
        if (read == Status::ok || read == Status::too_large) return true;
        uint64_t unused = 0u;
        if (connection_.kmt().performance_counter(unused) - started >= budget) return false;
        spin_once();
    }
}

/* A full ring needs room for one batch, not an empty ring: the host frees it
 * a batch at a time, and waiting for all of it would stop the guest until the
 * host had nothing left to do. */
bool KmtTransport::room_within_spin(uint64_t span)
{
    AV_SCOPE("KmtTransport::room_within_spin");
    constexpr uint64_t spins_per_second = 20u;
    uint64_t           frequency = 0u;
    const uint64_t     started = connection_.kmt().performance_counter(frequency);
    const uint64_t     budget = frequency / spins_per_second;
    for (;;) {
        if (ring_.free_space() >= span) return true;
        uint64_t unused = 0u;
        if (connection_.kmt().performance_counter(unused) - started >= budget) return false;
        spin_once();
    }
}

/* A device that has stopped taking the ring leaves the process waiting, so
 * the wait ends in a fault after a few seconds rather than never. */
template <typename Arrived> bool KmtTransport::arrives_eventually(Arrived arrived)
{
    AV_SCOPE("KmtTransport::arrives_eventually");
    constexpr uint64_t patience_seconds = 5u;
    uint64_t           frequency = 0u;
    const uint64_t     started = connection_.kmt().performance_counter(frequency);
    for (;;) {
        if (arrived()) return true;
        uint64_t unused = 0u;
        const uint64_t elapsed = connection_.kmt().performance_counter(unused) - started;
        if (elapsed >= frequency * patience_seconds) return false;
        /* Spinning, as a GPU wait of a few milliseconds usually is; a sleep
         * costs a timer tick, up to 15 ms on Windows' default resolution. */
        constexpr uint64_t spins_per_second = 50u;
        if (elapsed < frequency / spins_per_second) spin_once();
        else connection_.kmt().sleep_briefly();
    }
}

bool KmtTransport::consumed_within_spin()
{
    AV_SCOPE("KmtTransport::consumed_within_spin");
    constexpr uint64_t spins_per_second = 500u;
    uint64_t           frequency = 0u;
    const uint64_t     started = connection_.kmt().performance_counter(frequency);
    const uint64_t     budget = frequency / spins_per_second;
    for (;;) {
        if (ring_.head() == ring_.tail()) return true;
        uint64_t unused = 0u;
        if (connection_.kmt().performance_counter(unused) - started >= budget) return false;
        spin_once();
    }
}

Status KmtTransport::nop(uint32_t &answered)
{
    AV_SCOPE("KmtTransport::nop");
    WireWriter            request = request_writer();
    WireReader            answer(nullptr, 0u);
    const TransportResult sent = send_sync(CommandHeader::nop_id, request, answer);
    answered = static_cast<uint32_t>(answer.remaining());
    if (sent != TransportResult::ok) return Status::invalid_state;
    return answered != 0u ? Status::malformed : Status::ok;
}

WireArena      &KmtTransport::decode_arena()
{
    AV_SCOPE("KmtTransport::decode_arena");
    return decode_arena_;
}
ClientBlobArena &KmtTransport::blobs()
{
    AV_SCOPE("KmtTransport::blobs");
    return blobs_;
}

void KmtTransport::give_back_doorbell()
{
    AV_SCOPE("KmtTransport::give_back_doorbell");
    doorbell_.give_back();
}

bool KmtTransport::invariants_hold() const
{
    AV_SCOPE("KmtTransport::invariants_hold");
    /* An established context has its DMA buffer mapped, and only an allocation
     * that exists can be locked; the ring and fence keep their own invariants,
     * tested with them. */
    for (uint32_t index = 0u; index < allocation_count; index++)
        if (locked_[index] && allocations_[index] == 0u) return false;
    return context_ == 0u || dma_address_ != 0u;
}

}  // namespace av
