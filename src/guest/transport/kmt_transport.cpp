#include "guest/transport/kmt_transport.hpp"
#include "guest/transport/release_route.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/escape/tail_publish_record.hpp"
#include "proto/escape/wake_event_record.hpp"
#include "proto/submission/batch_header.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/counter_spin.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/publish_gate.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/channel_stats_report.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/command_pieces.hpp"
#include "proto/submission/command_stream.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "proto/submission/submission_descriptor.hpp"

namespace av {

namespace {
constexpr uint64_t page_bytes = 4096u;
constexpr size_t   max_batch = 0xFFFFFFFFu;
}

KmtTransport::KmtTransport(GuestConnection &connection, BlockSource &memory, SubApi sub_api, uint8_t ring_order,
                           uint32_t reply_bytes, uint8_t *staging, size_t staging_bytes)
    : connection_(connection), sub_api_(sub_api), ring_order_(ring_order), reply_bytes_(reply_bytes),
      staging_(staging), staging_bytes_(staging != nullptr ? staging_bytes : 0u),
      room_(staging != nullptr ? staging_bytes : 0u, reply_bytes), allocations_{},
      dma_address_(0u), context_(0u), hardware_scheduled_(false), ring_(), newest_end_(0u), newest_fence_(0u), previous_end_(0u), doorbell_(connection.kmt()),
      path_(connection.kmt(), doorbell_, ring_), reply_(), watch_(connection.kmt(), ring_), fences_(), fence_ends_{}, faulted_(false), held_{}, route_(nullptr), standby_(nullptr), gates_busy_(0u), pending_(nullptr, 0u), unpublished_(0u), answered_(false), answered_fence_(0u),
      decode_arena_(nullptr, 0u), blobs_(), arena_(nullptr), room_pressure_(0u), room_wait_ticks_(0u), window_began_(0u), windowed_(false), spill_(memory), answer_(memory)
{
    AV_SCOPE("KmtTransport::KmtTransport");
}

KmtTransport::KmtTransport(GuestConnection &connection, SubApi sub_api, uint8_t ring_order, uint32_t reply_bytes,
                           uint8_t *staging, size_t staging_bytes)
    : connection_(connection), sub_api_(sub_api), ring_order_(ring_order), reply_bytes_(reply_bytes),
      staging_(staging), staging_bytes_(staging != nullptr ? staging_bytes : 0u),
      room_(staging != nullptr ? staging_bytes : 0u, reply_bytes), allocations_{},
      dma_address_(0u), context_(0u), hardware_scheduled_(false), ring_(), newest_end_(0u), newest_fence_(0u), previous_end_(0u), doorbell_(connection.kmt()),
      path_(connection.kmt(), doorbell_, ring_), reply_(), watch_(connection.kmt(), ring_), fences_(), fence_ends_{}, faulted_(false), held_{}, route_(nullptr), standby_(nullptr), gates_busy_(0u), pending_(nullptr, 0u), unpublished_(0u), answered_(false), answered_fence_(0u),
      decode_arena_(nullptr, 0u), blobs_(), arena_(nullptr), room_pressure_(0u), room_wait_ticks_(0u), window_began_(0u), windowed_(false), spill_(), answer_()
{
    AV_SCOPE("KmtTransport::KmtTransport");
}

KmtTransport::~KmtTransport()
{
    AV_SCOPE("KmtTransport::~KmtTransport");
    teardown();
}

Status KmtTransport::setup_allocation(uint32_t index, AllocationKind kind, uint64_t bytes, bool map_gpu,
                                      uint64_t &address)
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
    return Status::ok;
}

Status KmtTransport::initialize(const GlContextOptions &options)
{
    AV_SCOPE("KmtTransport::initialize");
    if (context_ != 0u) return Status::invalid_state;
    if (staging_bytes_ <= CommandHeader::encoded_size) return Status::invalid_parameter;
    if (!record_carries(options)) return Status::invalid_parameter;
    ScopeExit undo([&] { teardown(); });
    uint64_t  unused = 0u;
    const Status named = setup_allocation(name_index, AllocationKind::context_ring, page_bytes, false, unused);
    if (named != Status::ok) return named;
    const Status dma_ready = setup_allocation(dma_index, AllocationKind::client, dma_bytes, true, dma_address_);
    if (dma_ready != Status::ok) return dma_ready;
    const AvContextRecord record(sub_api_, ring_order_, allocations_[name_index], reply_bytes_, options);
    uint8_t      private_data[AvContextRecord::encoded_size] = {};
    static_cast<void>(record.encode(private_data, sizeof private_data));
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
    if (hardware_scheduled_ &&
        doorbell_.initialize(connection_.device(), context_, allocations_[name_index]) == Status::ok) {
        static_cast<void>(doorbell_.connect());
    } else if (hardware_scheduled_) {
        static_cast<void>(connection_.kmt().destroy_context(context_));
        context_ = 0u;
        hardware_scheduled_ = false;
        const Status plain = connection_.kmt().create_context_virtual(connection_.device(), private_data,
                                                                      sizeof private_data, context_);
        if (plain != Status::ok) return plain;
    }
    const Status viewed = adopt_views();
    if (viewed != Status::ok) return viewed;
    if (!doorbell_.established()) {
        uint8_t escape[TailPublishRecord::encoded_size] = {};
        static_cast<void>(TailPublishRecord(ring()).encode(escape, sizeof escape));
        if (connection_.kmt().escape(connection_.adapter(), connection_.device(), context_, escape, sizeof escape) ==
            Status::ok)
            path_.publish_before_kernel();
    }
    undo.dismiss();
    return Status::ok;
}

Status KmtTransport::adopt_views()
{
    AV_SCOPE("KmtTransport::adopt_views");
    ContextViewsRecord views;
    Status             asked = ask_views(views);
    if (asked == Status::invalid_parameter) {
        views = ContextViewsRecord(ContextViewsRecord::without_fence);
        asked = ask_views(views);
    }
    if (asked != Status::ok) return asked;
    const auto ring = static_cast<uintptr_t>(views.ring());
    const auto reply = static_cast<uintptr_t>(views.reply());
    const auto fence = static_cast<uintptr_t>(views.fence());
    if constexpr (sizeof(uintptr_t) < sizeof(uint64_t))
        if (uint64_t{ring} != views.ring() || uint64_t{reply} != views.reply() || uint64_t{fence} != views.fence())
            return Status::malformed;
    const Status ring_bound = ring_.initialize(reinterpret_cast<uint8_t *>(ring), ring_order_);
    if (ring_bound != Status::ok) return ring_bound;
    reply_ = ReplyReader(reinterpret_cast<uint8_t *>(reply), reply_bytes_);
    watch_.read_page(reinterpret_cast<const uint8_t *>(fence));
    return Status::ok;
}

Status KmtTransport::ask_views(ContextViewsRecord &views)
{
    AV_SCOPE("KmtTransport::ask_views");
    uint8_t bytes[ContextViewsRecord::encoded_size] = {};
    static_cast<void>(views.encode(bytes, sizeof bytes));
    const Status escaped =
        connection_.kmt().escape(connection_.adapter(), connection_.device(), context_, bytes, sizeof bytes);
    if (escaped != Status::ok) return escaped;
    return views.accept_answer(bytes, sizeof bytes);
}

uint64_t KmtTransport::gpu_completed() const
{
    AV_SCOPE("KmtTransport::gpu_completed");
    return watch_.completed();
}

bool KmtTransport::knows_gpu_progress() const
{
    AV_SCOPE("KmtTransport::knows_gpu_progress");
    return watch_.reads_page();
}

uint64_t KmtTransport::published_fence() const
{
    AV_SCOPE("KmtTransport::published_fence");
    return __atomic_load_n(&newest_fence_, __ATOMIC_ACQUIRE);
}

uint64_t KmtTransport::covering_fence() const
{
    AV_SCOPE("KmtTransport::covering_fence");
    const bool held = __atomic_load_n(&unpublished_, __ATOMIC_ACQUIRE) != 0u;
    return published_fence() + (held ? 1u : 0u);
}

Status KmtTransport::await_gpu(uint64_t fence)
{
    AV_SCOPE("KmtTransport::await_gpu");
    if (faulted_) return Status::invalid_state;
    if (fence > newest_fence_) return Status::invalid_parameter;
    if (fence == 0u || (watch_.reads_page() && gpu_completed() >= fence)) return Status::ok;
    const bool waited = !kernel_waits_for(fence) || wait_for_host(fence) == Status::ok;
    if (waited && !watch_.reads_page()) return Status::ok;
    if (waited && arrives_eventually([this, fence] { return gpu_completed() >= fence; })) return Status::ok;
    faulted_ = true;
    return Status::invalid_state;
}

bool KmtTransport::kernel_waits_for(uint64_t fence) const
{
    AV_SCOPE("KmtTransport::kernel_waits_for");
    if (path_.releases()) return false;
    return !watch_.reads_page() || doorbell_.queue() != 0u || fence == newest_fence_;
}

Status KmtTransport::wake_on_raise(uint64_t handle)
{
    AV_SCOPE("KmtTransport::wake_on_raise");
    if (!established() || faulted()) return Status::invalid_state;
    uint8_t               bytes[WakeEventRecord::encoded_size] = {};
    const WakeEventRecord request(handle);
    if (request.encode(bytes, sizeof bytes) != Status::ok) return Status::invalid_parameter;
    return connection_.kmt().escape(connection_.adapter(), connection_.device(), context_, bytes, sizeof bytes);
}

bool KmtTransport::record_carries(const GlContextOptions &options) const
{
    AV_SCOPE("KmtTransport::record_carries");
    const AvContextRecord candidate(sub_api_, ring_order_, 1u, reply_bytes_, options);
    uint8_t                bytes[AvContextRecord::encoded_size] = {};
    static_cast<void>(candidate.encode(bytes, sizeof bytes));
    AvContextRecord decoded;
    return decoded.decode(bytes, sizeof bytes) == Status::ok;
}

void KmtTransport::lock_gates()
{
    AV_SCOPE("KmtTransport::lock_gates");
    while (__atomic_exchange_n(&gates_busy_, 1u, __ATOMIC_ACQUIRE) != 0u) {
    }
}

void KmtTransport::unlock_gates()
{
    AV_SCOPE("KmtTransport::unlock_gates");
    __atomic_store_n(&gates_busy_, 0u, __ATOMIC_RELEASE);
}

bool KmtTransport::ready_for_batch() const
{
    AV_SCOPE("KmtTransport::ready_for_batch");
    for (PublishGate *const &held : held_)
        if (__atomic_load_n(&held, __ATOMIC_ACQUIRE) != nullptr) return false;
    return ring_.head() >= previous_end_;
}

void KmtTransport::hold(PublishGate &gate)
{
    AV_SCOPE("KmtTransport::hold");
    PublishGate **free_slot = nullptr;
    lock_gates();
    for (PublishGate *&held : held_) {
        if (held == &gate) {
            unlock_gates();
            return;
        }
        if (held == nullptr && free_slot == nullptr) free_slot = &held;
    }
    if (free_slot != nullptr) *free_slot = &gate;
    unlock_gates();
    if (free_slot == nullptr) gate.pass();
}

Status KmtTransport::release_by(ReleaseRoute &route)
{
    AV_SCOPE("KmtTransport::release_by");
    if (!established() || faulted_ || !watch_.reads_page()) return watch_.reads_page() ? Status::invalid_state : Status::not_supported;
    if (routed_by(route)) return Status::ok;
    if (route_ != nullptr) {
        if (standby_ != nullptr) return Status::no_space;
        standby_ = &route;
        return Status::ok;
    }
    path_.release_through(&route, ring());
    route_ = &route;
    return Status::ok;
}

void KmtTransport::release_directly(const ReleaseRoute &route)
{
    AV_SCOPE("KmtTransport::release_directly");
    if (standby_ == &route) standby_ = nullptr;
    if (route_ != &route) return;
    if (standby_ != nullptr) {
        route_ = standby_;
        standby_ = nullptr;
        path_.release_through(const_cast<ReleaseRoute *>(route_), ring());
        return;
    }
    if (established() && !faulted_) static_cast<void>(await_gpu(newest_fence_));
    path_.release_through(nullptr, 0u);
    route_ = nullptr;
}

bool KmtTransport::routed_by(const ReleaseRoute &route) const
{
    AV_SCOPE("KmtTransport::routed_by");
    return route_ == &route || standby_ == &route;
}

bool KmtTransport::releases_by_packet() const
{
    AV_SCOPE("KmtTransport::releases_by_packet");
    return path_.releases();
}

void KmtTransport::forget(const PublishGate &gate)
{
    AV_SCOPE("KmtTransport::forget");
    lock_gates();
    for (PublishGate *&held : held_)
        if (held == &gate) held = nullptr;
    unlock_gates();
}

void KmtTransport::hold_in(uint8_t *block, size_t capacity)
{
    AV_SCOPE("KmtTransport::hold_in");
    pending_.adopt(block, capacity);
    pending_.clear();
}

size_t KmtTransport::held() const
{
    AV_SCOPE("KmtTransport::held");
    return pending_.used();
}

size_t KmtTransport::holding_capacity() const
{
    AV_SCOPE("KmtTransport::holding_capacity");
    return pending_.capacity();
}

TransportResult KmtTransport::flush()
{
    AV_SCOPE("KmtTransport::flush");
    if (faulted_ || !established()) return TransportResult::failed;
    if (pending_.used() == 0u) return TransportResult::ok;
    uint64_t              fence = 0u;
    const TransportResult sent = publish_batch(pending_.bytes(), static_cast<uint32_t>(pending_.used()), fence);
    pending_.clear();
    __atomic_store_n(&unpublished_, 0u, __ATOMIC_RELEASE);
    if (sent != TransportResult::ok) faulted_ = true;
    return sent;
}

TransportResult KmtTransport::hold_framed(const uint8_t *framed, size_t length)
{
    AV_SCOPE("KmtTransport::hold_framed");
    if (faulted_ || !established() || framed == nullptr || length == 0u) return TransportResult::failed;
    if (pending_.capacity() == 0u) {
        if (length > max_batch) return TransportResult::failed;
        blobs_.seal();
        uint64_t fence = 0u;
        return publish_batch(framed, static_cast<uint32_t>(length), fence);
    }
    if (length > pending_.capacity()) return TransportResult::failed;
    if (!pending_.fits(length) && flush() != TransportResult::ok) return TransportResult::failed;
    static_cast<void>(pending_.append(framed, length));
    __atomic_store_n(&unpublished_, 1u, __ATOMIC_RELEASE);
    blobs_.seal();
    return TransportResult::ok;
}

TransportResult KmtTransport::hold(uint32_t command_id, const WireWriter &writer, uint8_t flags)
{
    AV_SCOPE("KmtTransport::hold");
    if (writer.spilled()) return hold_pieces(command_id, writer, flags);
    if (!pending_.frame(command_id, writer, flags)) return TransportResult::failed;
    __atomic_store_n(&unpublished_, 1u, __ATOMIC_RELEASE);
    blobs_.seal();
    return TransportResult::ok;
}

TransportResult KmtTransport::hold_pieces(uint32_t command_id, const WireWriter &writer, uint8_t flags)
{
    AV_SCOPE("KmtTransport::hold_pieces");
    ScopeExit given_back([this] { spill_.release(); });
    if (writer.status() != Status::ok) return TransportResult::failed;
    CommandPieces pieces(command_id, flags, writer.bytes(), writer.offset());
    uint64_t      cut = 0u;
    while (!pieces.done()) {
        if (pending_.frame_piece(pieces)) {
            __atomic_store_n(&unpublished_, 1u, __ATOMIC_RELEASE);
            stats_.piece(++cut);
            continue;
        }
        if (pending_.used() == 0u || flush() != TransportResult::ok) return TransportResult::failed;
    }
    blobs_.seal();
    return TransportResult::ok;
}

TransportResult KmtTransport::publish_held_answered(WireReader &reply)
{
    AV_SCOPE("KmtTransport::publish_held_answered");
    uint64_t              fence = 0u;
    const TransportResult sent = publish_batch(pending_.bytes(), static_cast<uint32_t>(pending_.used()), fence);
    pending_.clear();
    __atomic_store_n(&unpublished_, 0u, __ATOMIC_RELEASE);
    if (sent != TransportResult::ok) {
        faulted_ = true;
        return sent;
    }
    const TransportResult answered = await(fence, reply);
    answered_ = answered == TransportResult::ok;
    return answered;
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
    return context_ != 0u ? allocations_[name_index] : 0u;
}

void KmtTransport::teardown()
{
    AV_SCOPE("KmtTransport::teardown");
    Kmt           &kmt = connection_.kmt();
    const uint32_t device = connection_.device();
    doorbell_.teardown();
    if (context_ != 0u) {
        static_cast<void>(kmt.destroy_context(context_));
        context_ = 0u;
    }
    for (uint32_t index = 0u; index < allocation_count; index++) {
        if (allocations_[index] == 0u) continue;
        if (index == dma_index && dma_address_ != 0u) {
            static_cast<void>(kmt.free_gpu_virtual_address(connection_.adapter(), dma_address_,
                                                           dma_bytes / page_bytes));
            dma_address_ = 0u;
        }
        static_cast<void>(kmt.destroy_allocation(device, allocations_[index]));
        allocations_[index] = 0u;
    }
    ring_ = TransportRing();
    reply_ = ReplyReader();
    watch_.read_page(nullptr);
    fences_ = TransportFence();
    faulted_ = false;
    for (PublishGate *&held : held_) held = nullptr;
    path_.release_through(nullptr, 0u);
    route_ = nullptr;
    standby_ = nullptr;
    pending_.clear();
    __atomic_store_n(&unpublished_, 0u, __ATOMIC_RELEASE);
    answered_ = false;
    spill_.release();
    answer_.release();
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
bool KmtTransport::host_faulted() const
{
    AV_SCOPE("KmtTransport::host_faulted");
    return watch_.faulted();
}

uint32_t KmtTransport::host_fault_code() const
{
    AV_SCOPE("KmtTransport::host_fault_code");
    return watch_.fault_code();
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
    const size_t room = staging_bytes_ - CommandHeader::encoded_size;
    if (pending_.capacity() == 0u) return WireWriter(staging_ + CommandHeader::encoded_size, room, spill_);
    if (!pending_.fits(FramedBlock::header_bytes + room) && flush() != TransportResult::ok)
        return WireWriter(nullptr, 0u);
    return pending_.lend(room, spill_);
}

TransportResult KmtTransport::publish(uint32_t command_id, WireWriter &writer, uint8_t flags, uint64_t &fence)
{
    AV_SCOPE("KmtTransport::publish");
    fence = 0u;
    if (writer.status() != Status::ok || !established()) return TransportResult::failed;
    if (writer.spilled()) return publish_pieces(command_id, writer, flags, fence);
    const uint32_t body = static_cast<uint32_t>(writer.offset());
    const uint32_t command_size = static_cast<uint32_t>(CommandHeader::encoded_size) + body;
    static_cast<void>(CommandHeader(command_id, command_size, flags).encode(staging_,
                                                                            CommandHeader::encoded_size));
    blobs_.seal();
    return publish_batch(staging_, command_size, fence);
}

TransportResult KmtTransport::publish_pieces(uint32_t command_id, const WireWriter &writer, uint8_t flags,
                                             uint64_t &fence)
{
    AV_SCOPE("KmtTransport::publish_pieces");
    ScopeExit     given_back([this] { spill_.release(); });
    CommandPieces pieces(command_id, flags, writer.bytes(), writer.offset());
    uint64_t      cut = 0u;
    while (!pieces.done()) {
        const size_t framed = pieces.frame_next(staging_, staging_bytes_);
        if (framed == 0u) return TransportResult::failed;
        if (pieces.done()) blobs_.seal();
        const TransportResult sent = publish_batch(staging_, static_cast<uint32_t>(framed), fence);
        if (sent != TransportResult::ok) return sent;
        stats_.piece(++cut);
    }
    return TransportResult::ok;
}

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
    if (consumed_within_spin()) {
        fences_.close(newest);
        blobs_.retire_through(newest);
        return true;
    }
    const Status waited = wait_for_host(newest);
    if (waited != Status::ok || !arrives_eventually([this] { return ring_.head() == ring_.tail(); })) {
        faulted_ = true;
        return false;
    }
    fences_.close(newest);
    blobs_.retire_through(newest);
    return true;
}

const TransferRoom &KmtTransport::room() const
{
    AV_SCOPE("KmtTransport::room");
    return room_;
}

bool KmtTransport::make_room_for_blobs(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("KmtTransport::make_room_for_blobs");
    if (!blobs_.ready()) return false;
    if (blobs_.fits(sizes, count)) return true;
    if (!blobs_.fits_drained(sizes, count) && !grow_for_blobs(sizes, count)) return false;
    if (blobs_.fits(sizes, count)) return true;
    if (flush() != TransportResult::ok) return false;
    retire_consumed();
    if (blobs_.fits(sizes, count)) return true;
    room_pressure_++;
    const uint64_t waiting_from = published_now();
    ScopeExit      waited([this, waiting_from] { room_wait_ticks_ += published_now() - waiting_from; });
    if (consumed_for_blobs(sizes, count)) return true;
    while (!blobs_.fits(sizes, count) && fences_.outstanding() != 0u && !faulted_) {
        const uint64_t oldest = fences_.oldest();
        const uint64_t end = fence_ends_[oldest % TransportFence::max_outstanding];
        if (ring_.head() < end) {
            const uint64_t began = published_now();
            const bool     arrived = wait_for_host(oldest) == Status::ok &&
                                 arrives_eventually([this, end] { return ring_.head() >= end; });
            note_wait(began, ChannelCounter::blob_room_waits, ChannelCounter::blob_room_wait_us);
            if (!arrived) {
                faulted_ = true;
                return false;
            }
        }
        retire_consumed();
    }
    return blobs_.fits(sizes, count);
}

bool KmtTransport::consumed_for_blobs(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("KmtTransport::consumed_for_blobs");
    if (arena_ == nullptr || fences_.outstanding() == 0u) return false;
    CounterSpin spin(connection_.kmt(), grow_spin_us);
    while (spin.lasts()) {
        spin_once();
        retire_consumed();
        if (blobs_.fits(sizes, count)) return true;
    }
    return false;
}

bool KmtTransport::grow_for_blobs(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("KmtTransport::grow_for_blobs");
    if (arena_ == nullptr || faulted_) return false;
    uint64_t sum = 0u;
    uint64_t largest = 0u;
    for (uint32_t index = 0u; index < count; index++) {
        sum = sizes[index] > ~sum ? ~uint64_t{0} : sum + sizes[index];
        if (sizes[index] > largest) largest = sizes[index];
    }
    if (largest > GuestBytes::slot_bytes) return false;
    const uint64_t need = sum <= GuestBytes::slot_bytes ? sum : largest;
    return grow_arena(need) && blobs_.fits_drained(sizes, count);
}

bool KmtTransport::grow_arena(uint64_t need)
{
    AV_SCOPE("KmtTransport::grow_arena");
    if (arena_->grow(need) != Status::ok) return false;
    stats_.note(ChannelCounter::arena_grows, 1u);
    stats_.observe(blobs_);
    retire_consumed();
    return true;
}

bool KmtTransport::grow_if_pressed()
{
    AV_SCOPE("KmtTransport::grow_if_pressed");
    if (arena_ == nullptr || faulted_ || !blobs_.ready()) return false;
    const uint64_t now = published_now();
    if (!windowed_) {
        windowed_ = true;
        window_began_ = now;
    }
    const bool waiting = room_pressure_ >= pressure_waits || room_wait_ticks_ >= pressure_wait_ticks;
    const bool pressed = waiting && uint64_t{blobs_.window_peak()} * 100u >=
                                        uint64_t{blobs_.ring_bytes()} * pressure_percent;
    if (!pressed && now - window_began_ < pressure_window_ticks) return false;
    blobs_.restart_window();
    room_pressure_ = 0u;
    room_wait_ticks_ = 0u;
    window_began_ = now;
    if (!pressed) return false;
    static_cast<void>(grow_arena(0u));
    return true;
}

void KmtTransport::tune_arena()
{
    AV_SCOPE("KmtTransport::tune_arena");
    if (arena_ != nullptr && !faulted_) retire_consumed();
    if (grow_if_pressed()) return;
    release_idle_segments();
}

ClientBlobLease KmtTransport::open_region(uint64_t bytes)
{
    AV_SCOPE("KmtTransport::open_region");
    ArenaSegment *segment = nullptr;
    if (arena_ == nullptr || arena_->open_region(bytes, segment) != Status::ok) return ClientBlobLease();
    return ClientBlobLease(segment->view(), segment->base(), static_cast<size_t>(bytes));
}

void KmtTransport::close_region(const ClientBlobLease &region)
{
    AV_SCOPE("KmtTransport::close_region");
    if (arena_ != nullptr && region.taken()) static_cast<void>(arena_->close_region(region.offset()));
}

void KmtTransport::release_idle_segments()
{
    AV_SCOPE("KmtTransport::release_idle_segments");
    if (arena_ != nullptr && !faulted_) static_cast<void>(arena_->release_idle(published_now()));
}

void KmtTransport::attach_arena(KmtBlobArena *arena)
{
    AV_SCOPE("KmtTransport::attach_arena");
    arena_ = arena;
}

Status KmtTransport::wait_for_host(uint64_t fence)
{
    AV_SCOPE("KmtTransport::wait_for_host");
    watch_.start();
    if (path_.releases()) return wait_on_page(fence);
    for (;;) {
        const Status waited = doorbell_.queue() != 0u
                                  ? connection_.kmt().wait_for_hw_queue(connection_.device(),
                                                                        doorbell_.progress_fence(), fence)
                                  : connection_.kmt().wait_for_context(connection_.device(), context_);
        if (waited == Status::ok || connection_.kmt().last_code() != wait_timed_out || !watch_.keeps_waiting())
            return waited;
    }
}

Status KmtTransport::wait_on_page(uint64_t fence)
{
    AV_SCOPE("KmtTransport::wait_on_page");
    uint64_t       frequency = 0u;
    const uint64_t began = connection_.kmt().performance_counter(frequency);
    bool           told = false;
    CounterSpin    spin(connection_.kmt(), gpu_spin_us);
    for (;;) {
        if (gpu_completed() >= fence) return Status::ok;
        if (!watch_.keeps_waiting()) return Status::invalid_state;
        if (!told && route_ != nullptr &&
            connection_.kmt().performance_counter(frequency) - began > frequency * slow_wait_seconds) {
            told = true;
            const_cast<ReleaseRoute *>(route_)->waited_long();
        }
        if (spin.lasts()) spin_once();
        else connection_.kmt().sleep_briefly();
    }
}

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
    if (done == 0u) return;
    fences_.close(done);
    blobs_.retire_through(done);
}

bool KmtTransport::room_for_fence()
{
    AV_SCOPE("KmtTransport::room_for_fence");
    retire_consumed();
    if (!fences_.full()) return true;
    const uint64_t began = published_now();
    CounterSpin    spin(connection_.kmt(), fence_spin_us);
    bool           roomy = false;
    while (!roomy && spin.lasts()) {
        spin_once();
        retire_consumed();
        roomy = !fences_.full();
    }
    if (!roomy) roomy = drain();
    note_wait(began, ChannelCounter::fence_waits, ChannelCounter::fence_wait_us);
    return roomy;
}

TransportResult KmtTransport::publish_batch(const uint8_t *batch, uint32_t length, uint64_t &fence)
{
    AV_SCOPE("KmtTransport::publish_batch");
    PublishGate *passing[most_gates] = {};
    uint32_t     passes = 0u;
    lock_gates();
    for (PublishGate *&held : held_) {
        if (held == nullptr) continue;
        passing[passes++] = held;
        held = nullptr;
    }
    unlock_gates();
    for (uint32_t index = 0u; index < passes; index++) passing[index]->pass();
    if (fences_.full() && !room_for_fence()) return TransportResult::failed;
    uint64_t next = fences_.peek();
    if (next == 0u) return TransportResult::failed;
    uint64_t       begin = 0u;
    uint64_t       end = 0u;
    uint8_t        trailer[ChannelStats::command_bytes] = {};
    const uint64_t now = published_now();
    const uint32_t trailer_length = stats_trailer(batch, length, now, trailer);
    Status written = ring_.write_batch(batch, length, trailer, trailer_length, next, now, begin, end);
    if (written == Status::no_space)
        written = write_when_room(batch, length, trailer, trailer_length, next, begin, end);
    if (written != Status::ok) return TransportResult::failed;
    if (trailer_length != 0u) stats_.sent(now);
    return submit_written(begin, end, fence);
}

Status KmtTransport::write_when_room(const uint8_t *batch, uint32_t length, const uint8_t *trailer,
                                     uint32_t trailer_length, uint64_t &next, uint64_t &begin, uint64_t &end)
{
    AV_SCOPE("KmtTransport::write_when_room");
    const uint64_t began = published_now();
    Status         written = Status::no_space;
    if (room_within_spin(BatchHeader(length + trailer_length, next, 0u).span()))
        written = ring_.write_batch(batch, length, trailer, trailer_length, next, published_now(), begin, end);
    if (written == Status::no_space) {
        if (!drain()) {
            note_wait(began, ChannelCounter::ring_waits, ChannelCounter::ring_wait_us);
            return Status::invalid_state;
        }
        next = fences_.peek();
        written = ring_.write_batch(batch, length, trailer, trailer_length, next, published_now(), begin, end);
    }
    note_wait(began, ChannelCounter::ring_waits, ChannelCounter::ring_wait_us);
    return written;
}

TransportResult KmtTransport::submit_written(uint64_t begin, uint64_t end, uint64_t &fence)
{
    AV_SCOPE("KmtTransport::submit_written");
    previous_end_ = newest_end_;
    newest_end_ = end;
    fence = fences_.open();
    __atomic_store_n(&newest_fence_, fence, __ATOMIC_RELEASE);
    fence_ends_[fence % TransportFence::max_outstanding] = end;
    blobs_.mark(fence);
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

uint32_t KmtTransport::stats_trailer(const uint8_t *batch, uint32_t length, uint64_t now, uint8_t *into)
{
    AV_SCOPE("KmtTransport::stats_trailer");
    constexpr uint32_t size = static_cast<uint32_t>(ChannelStats::command_bytes);
    stats_.observe(blobs_);
    if (!stats_.due(now) || uint64_t{length} + size > max_batch) return 0u;
    if (!BatchHeader(length + size, 1u, now).fits(ring_.capacity())) return 0u;
    if (CommandStream(batch, length).ends_in_piece() || !stats_.frame_report(into, size)) return 0u;
    return size;
}

void KmtTransport::note_wait(uint64_t began, ChannelCounter waits, ChannelCounter wait_us)
{
    AV_SCOPE("KmtTransport::note_wait");
    constexpr uint64_t ticks_per_microsecond = 10u;
    stats_.note(waits, 1u);
    stats_.note(wait_us, (published_now() - began) / ticks_per_microsecond);
}

void KmtTransport::note(ChannelCounter counter, uint64_t amount)
{
    AV_SCOPE("KmtTransport::note");
    stats_.note(counter, amount);
}

WireWriter KmtTransport::request_writer_with_room(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("KmtTransport::request_writer_with_room");
    static_cast<void>(make_room_for_blobs(sizes, count));
    return request_writer();
}

TransportResult KmtTransport::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("KmtTransport::send_async");
    if (faulted_) return TransportResult::failed;
    if (pending_.capacity() != 0u) return hold(command_id, writer, 0u);
    uint64_t fence = 0u;
    return publish(command_id, writer, 0u, fence);
}

void KmtTransport::end_call()
{
    AV_SCOPE("KmtTransport::end_call");
    if (!answered_) return;
    answered_ = false;
    blobs_.retire_through(answered_fence_);
    answer_.release();
}

TransportResult KmtTransport::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("KmtTransport::send_sync");
    reply = WireReader(nullptr, 0u);
    if (faulted_) return TransportResult::failed;
    if (pending_.capacity() != 0u) {
        if (!established() || hold(command_id, writer, CommandHeader::generate_reply) != TransportResult::ok)
            return TransportResult::failed;
        return publish_held_answered(reply);
    }
    uint64_t              fence = 0u;
    const TransportResult sent = publish(command_id, writer, CommandHeader::generate_reply, fence);
    if (sent != TransportResult::ok) return sent;
    const TransportResult answered = await(fence, reply);
    answered_ = answered == TransportResult::ok;
    return answered;
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
    if (framing != Status::no_space || flush() != TransportResult::ok) return TransportResult::failed;
    blobs_.seal();
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
    const Status read = await_slot(fence, reply);
    if (read == Status::too_large) return fetch_answer(reply);
    return read == Status::ok ? TransportResult::ok : TransportResult::failed;
}

TransportResult KmtTransport::fetch_answer(WireReader &reply)
{
    AV_SCOPE("KmtTransport::fetch_answer");
    reply = WireReader(nullptr, 0u);
    ScopeExit dropped([this] { answer_.release(); });
    uint64_t  total = 0u;
    uint64_t  fetched = 0u;
    do {
        WireReader part(nullptr, 0u);
        if (fetch_part(fetched, part) != TransportResult::ok) return TransportResult::failed;
        const uint64_t length = part.read_u64();
        const size_t   bytes = part.remaining();
        const bool     first = fetched == 0u;
        if (part.status() != Status::ok || (first && length == 0u) || (!first && length != total) ||
            bytes > length - fetched || (bytes == 0u && fetched < length)) {
            faulted_ = true;
            return TransportResult::failed;
        }
        size_t capacity = 0u;
        if (first && (length > ~size_t{0} ||
                      answer_.grow(nullptr, 0u, static_cast<size_t>(length), capacity) == nullptr))
            return TransportResult::failed;
        total = length;
        part.copy_bytes(answer_.bytes() + fetched, bytes);
        fetched += bytes;
    } while (fetched < total);
    dropped.dismiss();
    reply = WireReader(answer_.bytes(), static_cast<size_t>(total));
    return TransportResult::ok;
}

TransportResult KmtTransport::fetch_part(uint64_t offset, WireReader &part)
{
    AV_SCOPE("KmtTransport::fetch_part");
    WireWriter request(staging_ + CommandHeader::encoded_size, staging_bytes_ - CommandHeader::encoded_size);
    request.write_u64(offset);
    uint64_t fence = 0u;
    if (publish(CommandHeader::answer_piece_id, request, CommandHeader::generate_reply, fence) != TransportResult::ok)
        return TransportResult::failed;
    return await_slot(fence, part) == Status::ok ? TransportResult::ok : TransportResult::failed;
}

Status KmtTransport::await_slot(uint64_t fence, WireReader &reply)
{
    AV_SCOPE("KmtTransport::await_slot");
    if (!replied_within_spin(fence)) {
        const Status waited = wait_for_host(fence);
        if (waited != Status::ok || !arrives_eventually([this, fence] {
                WireReader   probe(nullptr, 0u);
                const Status read = reply_.read(fence, probe);
                return read == Status::ok || read == Status::too_large;
            })) {
            faulted_ = true;
            return Status::invalid_state;
        }
    }
    fences_.close(fence);
    answered_fence_ = fence;
    const Status read = reply_.read(fence, reply);
    if (read == Status::ok || read == Status::too_large) return read;
    faulted_ = true;
    return Status::invalid_state;
}

void KmtTransport::spin_once() const
{
    AV_SCOPE("KmtTransport::spin_once");
    __asm__ __volatile__("" ::: "memory");
}

bool KmtTransport::replied_within_spin(uint64_t fence)
{
    AV_SCOPE("KmtTransport::replied_within_spin");
    CounterSpin spin(connection_.kmt(), reply_spin_us);
    for (;;) {
        WireReader   probe(nullptr, 0u);
        const Status read = reply_.read(fence, probe);
        if (read == Status::ok || read == Status::too_large) return true;
        if (!spin.lasts()) return false;
        spin_once();
    }
}

bool KmtTransport::room_within_spin(uint64_t span)
{
    AV_SCOPE("KmtTransport::room_within_spin");
    CounterSpin spin(connection_.kmt(), ring_spin_us);
    for (;;) {
        if (ring_.free_space() >= span) return true;
        if (!spin.lasts()) return false;
        spin_once();
    }
}

template <typename Arrived> bool KmtTransport::arrives_eventually(Arrived arrived)
{
    AV_SCOPE("KmtTransport::arrives_eventually");
    watch_.start();
    CounterSpin spin(connection_.kmt(), gpu_spin_us);
    for (;;) {
        if (arrived()) return true;
        if (!watch_.keeps_waiting()) return false;
        if (spin.lasts()) spin_once();
        else connection_.kmt().sleep_briefly();
    }
}

bool KmtTransport::consumed_within_spin()
{
    AV_SCOPE("KmtTransport::consumed_within_spin");
    CounterSpin spin(connection_.kmt(), drain_spin_us);
    for (;;) {
        if (ring_.head() == ring_.tail()) return true;
        if (!spin.lasts()) return false;
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
    end_call();
    if (sent != TransportResult::ok) return Status::invalid_state;
    return answered != 0u ? Status::malformed : Status::ok;
}

Status KmtTransport::run_held()
{
    AV_SCOPE("KmtTransport::run_held");
    uint32_t answered = 0u;
    return nop(answered) == Status::invalid_state ? Status::invalid_state : Status::ok;
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
    return pending_.invariants_hold() && (context_ == 0u || (allocations_[name_index] != 0u && dma_address_ != 0u));
}

}
