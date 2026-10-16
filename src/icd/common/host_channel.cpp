#include "icd/common/host_channel.hpp"

#include "core/log/driver_log.hpp"
#include "core/platform/block_source.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/guest_connection.hpp"

namespace av::icd {

ChannelShape::ChannelShape(SubApi sub_api, uint8_t ring_order, uint32_t reply_bytes, size_t staging_bytes,
                           size_t batch_bytes, uint64_t arena_bytes)
    : sub_api_(sub_api), ring_order_(ring_order), reply_bytes_(reply_bytes), staging_bytes_(staging_bytes),
      batch_bytes_(batch_bytes), arena_bytes_(arena_bytes)
{
    AV_SCOPE("ChannelShape::ChannelShape");
}

HostChannel::HostChannel(BlockSource &memory, DriverLog &log, const ChannelShape &shape,
                         ThreadLock &lock)
    : memory_(memory), lines_(log), lock_(lock), sub_api_(shape.sub_api_), ring_order_(shape.ring_order_),
      reply_bytes_(shape.reply_bytes_), staging_bytes_(shape.staging_bytes_), batch_bytes_(shape.batch_bytes_),
      arena_bytes_(shape.arena_bytes_), staging_(nullptr), batch_block_(nullptr), transport_(), locked_(), arena_(),
      device_(nullptr), memories_(memory), handles_(memory, Pool::paged), retired_(memory, Pool::paged)
{
    AV_SCOPE("HostChannel::HostChannel");
}

HostChannel::~HostChannel()
{
    AV_SCOPE("HostChannel::~HostChannel");
    close(true);
}

Status HostChannel::open(GuestConnection &device, const GlContextOptions &options)
{
    AV_SCOPE("HostChannel::open");
    if (opened()) return Status::invalid_state;
    ScopeExit undo([this] { close(true); });
    staging_ = static_cast<uint8_t *>(memory_.allocate(staging_bytes_, Pool::paged));
    if (staging_ == nullptr) return Status::no_memory;
    static_cast<void>(transport_.construct(device, memory_, sub_api_, ring_order_, reply_bytes_, staging_, staging_bytes_));
    const Status initialized = transport_.object().initialize(options);
    if (initialized != Status::ok) {
        lines_.write("the driver refused the context");
        return initialized;
    }
    if (batch_bytes_ != 0u) {
        batch_block_ = static_cast<uint8_t *>(memory_.allocate(batch_bytes_, Pool::paged));
        if (batch_block_ == nullptr) return Status::no_memory;
        transport_.object().hold_in(batch_block_, batch_bytes_);
    }
    static_cast<void>(locked_.construct(transport_.object(), lock_));
    device_ = &device;
    if (arena_bytes_ != 0u && open_arena(device) != Status::ok) {
        /* A device that will not import leaves the channel usable, with bulk
         * data inline; a transport that faulted while asking has taken the
         * context with it. */
        if (transport_.object().faulted()) {
            lines_.write("the context faulted opening its blob arena");
            return Status::invalid_state;
        }
        lines_.write("no blob arena for the context; bulk data crosses inline");
    }
    undo.dismiss();
    return Status::ok;
}

Status HostChannel::open_arena(GuestConnection &device)
{
    AV_SCOPE("HostChannel::open_arena");
    static_cast<void>(arena_.construct(device, transport_.object()));
    uint64_t bytes = arena_bytes_;
    Status   opened = arena_.object().open(bytes);
    while (opened != Status::ok && !transport_.object().faulted() && bytes / 2u >= KmtBlobArena::default_bytes) {
        bytes /= 2u;
        opened = arena_.object().open(bytes);
    }
    if (opened != Status::ok) arena_.destroy();
    return opened;
}

void HostChannel::close(bool lost)
{
    AV_SCOPE("HostChannel::close");
    /* What was sent last still reaches the host before its context goes. */
    if (!lost && transport_.constructed()) static_cast<void>(transport_.object().flush());
    /* Retired or not, every allocation goes with the context: the host's work
     * over them goes with it too. */
    retired_.reclaim([](HostAllocation *) {});
    close_memories(lost);
    device_ = nullptr;
    if (arena_.constructed()) {
        /* Neither answer changes what is torn down next: a close the device
         * is not told of is one it does with the context. */
        const Status closed = lost ? arena_.object().abandon() : arena_.object().close();
        if (closed != Status::ok) lines_.write("the blob arena did not close whole");
        arena_.destroy();
    }
    locked_.destroy();
    transport_.destroy();
    if (batch_block_ != nullptr) memory_.release(batch_block_);
    batch_block_ = nullptr;
    if (staging_ != nullptr) memory_.release(staging_);
    staging_ = nullptr;
}

Status HostChannel::open_memory(uint64_t bytes, HostMemoryRequest memory, const void *owner, HostAllocation *&made)
{
    AV_SCOPE("HostChannel::open_memory");
    made = nullptr;
    const ThreadLockHold release(lock_);
    Kept                *kept = nullptr;
    const Status         room = keep(owner, kept);
    if (room != Status::ok) return room;
    return settle(kept, kept->allocation_.object().open(bytes, memory), made);
}

Status HostChannel::adopt_memory(void *address, uint64_t bytes, HostMemoryRequest memory, const void *owner,
                                 HostAllocation *&made)
{
    AV_SCOPE("HostChannel::adopt_memory");
    made = nullptr;
    const ThreadLockHold release(lock_);
    Kept                *kept = nullptr;
    const Status         room = keep(owner, kept);
    if (room != Status::ok) return room;
    return settle(kept, kept->allocation_.object().adopt(address, bytes, memory), made);
}

/* The index has room before the host is asked, so an allocation the host
 * made is never one this cannot keep. */
Status HostChannel::keep(const void *owner, Kept *&kept)
{
    AV_SCOPE("HostChannel::keep");
    if (!opened() || faulted()) return Status::invalid_state;
    if (handles_.reserve(handles_.count() + 1u) != Status::ok) return Status::no_memory;
    if (memories_.create(kept) != Status::ok) return Status::no_memory;
    static_cast<void>(kept->allocation_.construct(*device_, transport_.object()));   /* a fresh slot */
    kept->owner_ = owner;
    return Status::ok;
}

Status HostChannel::settle(Kept *kept, Status taken, HostAllocation *&made)
{
    AV_SCOPE("HostChannel::settle");
    if (taken != Status::ok) {
        static_cast<void>(memories_.destroy(kept));
        return taken;
    }
    uint32_t slot = 0u;
    if (handles_.insert(kept->allocation_.object().handle(), kept, slot) != Status::ok) {
        lines_.write("the host named two allocations by one handle; the second is released");
        static_cast<void>(close_kept(kept, false));
        return Status::malformed;
    }
    made = &kept->allocation_.object();
    return Status::ok;
}

Status HostChannel::close_kept(Kept *kept, bool lost)
{
    AV_SCOPE("HostChannel::close_kept");
    HostAllocation &allocation = kept->allocation_.object();
    const Status    released = lost ? allocation.abandon() : allocation.close();
    static_cast<void>(memories_.destroy(kept));
    return released;
}

Status HostChannel::close_memory(HostAllocation &allocation)
{
    AV_SCOPE("HostChannel::close_memory");
    const ThreadLockHold release(lock_);
    Kept *const *const   found = handles_.find(allocation.handle());
    if (found == nullptr || &(*found)->allocation_.object() != &allocation) return Status::invalid_parameter;
    Kept *const kept = *found;
    static_cast<void>(handles_.erase(allocation.handle()));
    return close_kept(kept, faulted());
}

Status HostChannel::close_owned(const void *owner)
{
    AV_SCOPE("HostChannel::close_owned");
    const ThreadLockHold release(lock_);
    const bool           lost = faulted();
    Status               first = Status::ok;
    static_cast<void>(handles_.erase_if([this, owner, lost, &first](uint64_t, Kept *&kept) {
        if (kept->owner_ != owner) return false;
        const Status released = close_kept(kept, lost);
        if (first == Status::ok) first = released;
        return true;
    }));
    return first;
}

void HostChannel::retire_memory(HostAllocation &allocation)
{
    AV_SCOPE("HostChannel::retire_memory");
    {
        const ThreadLockHold held(lock_);
        if (retired_.add(&allocation) == Status::ok) return;
    }
    lines_.write("more memory let go of than can wait for the host; closed at once");
    static_cast<void>(close_memory(allocation));
}

/* Taken out of the list one at a time under the lock and closed after it: a
 * close takes the lock itself. */
void HostChannel::reclaim_retired()
{
    AV_SCOPE("HostChannel::reclaim_retired");
    for (;;) {
        HostAllocation *next = nullptr;
        {
            const ThreadLockHold held(lock_);
            if (!retired_.take_last(next)) return;
        }
        static_cast<void>(close_memory(*next));
    }
}

HostAllocation *HostChannel::find_memory(uint64_t handle, const void *owner)
{
    AV_SCOPE("HostChannel::find_memory");
    if (handle == 0u) return nullptr;
    const ThreadLockHold release(lock_);
    Kept *const *const   found = handles_.find(handle);
    return found != nullptr && (*found)->owner_ == owner ? &(*found)->allocation_.object() : nullptr;
}

uint32_t HostChannel::memories() const
{
    AV_SCOPE("HostChannel::memories");
    return memories_.live();
}

void HostChannel::close_memories(bool lost)
{
    AV_SCOPE("HostChannel::close_memories");
    static_cast<void>(handles_.erase_if([this, lost](uint64_t, Kept *&kept) {
        if (close_kept(kept, lost) != Status::ok) lines_.write("host memory did not close whole");
        return true;
    }));
}

bool HostChannel::opened() const
{
    AV_SCOPE("HostChannel::opened");
    return transport_.constructed();
}

bool HostChannel::faulted() const
{
    AV_SCOPE("HostChannel::faulted");
    return transport_.constructed() && transport_.object().faulted();
}

bool HostChannel::lost() const
{
    AV_SCOPE("HostChannel::lost");
    if (!transport_.constructed()) return false;
    const KmtTransport &transport = transport_.object();
    return transport.faulted() || transport.host_faulted();
}

FaultCode HostChannel::fault_code() const
{
    AV_SCOPE("HostChannel::fault_code");
    if (!lost()) return FaultCode::none;
    return static_cast<FaultCode>(transport_.object().host_fault_code());
}

ClientTransport &HostChannel::wire()
{
    AV_SCOPE("HostChannel::wire");
    return locked_.object();
}

TransportResult HostChannel::flush()
{
    AV_SCOPE("HostChannel::flush");
    if (!opened()) return TransportResult::failed;
    const ThreadLockHold release(lock_);
    return transport_.object().flush();
}

TransportResult HostChannel::send_batch(const uint8_t *framed, size_t length)
{
    AV_SCOPE("HostChannel::send_batch");
    if (!opened()) return TransportResult::failed;
    const ThreadLockHold release(lock_);
    WireReader           unanswered(nullptr, 0u);
    return transport_.object().send_batch(framed, length, unanswered);
}

TransportResult HostChannel::send_command(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("HostChannel::send_command");
    if (!opened()) return TransportResult::failed;
    const ThreadLockHold release(lock_);
    return transport_.object().send_async(command_id, writer);
}

Status HostChannel::run_held()
{
    AV_SCOPE("HostChannel::run_held");
    if (!opened()) return Status::invalid_state;
    const ThreadLockHold release(lock_);
    return transport_.object().run_held();
}

KmtTransport &HostChannel::transport()
{
    AV_SCOPE("HostChannel::transport");
    return transport_.object();
}

const KmtTransport &HostChannel::transport() const
{
    AV_SCOPE("HostChannel::transport");
    return transport_.object();
}

uint8_t *HostChannel::arena_view() const
{
    AV_SCOPE("HostChannel::arena_view");
    return arena_.constructed() ? arena_.object().view() : nullptr;
}

uint64_t HostChannel::arena_bytes() const
{
    AV_SCOPE("HostChannel::arena_bytes");
    return arena_.constructed() ? arena_.object().bytes() : 0u;
}

uint64_t HostChannel::host_errors() const
{
    AV_SCOPE("HostChannel::host_errors");
    return arena_.constructed() ? arena_.object().host_errors() : 0u;
}

void HostChannel::report(StateWriter &writer) const
{
    AV_SCOPE("HostChannel::report");
    writer.flag("opened", opened());
    writer.flag("faulted", faulted());
    writer.number("arena_bytes", arena_bytes());
}

bool HostChannel::invariants_hold() const
{
    AV_SCOPE("HostChannel::invariants_hold");
    if (!opened()) return staging_ == nullptr && batch_block_ == nullptr && !locked_.constructed();
    if (staging_ == nullptr || !locked_.constructed() || (batch_bytes_ != 0u) != (batch_block_ != nullptr))
        return false;
    if (transport_.object().holding_capacity() != (batch_block_ != nullptr ? batch_bytes_ : 0u)) return false;
    return handles_.count() == memories_.live() && transport_.object().invariants_hold();
}

}  // namespace av::icd
