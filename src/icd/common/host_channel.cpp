#include "icd/common/host_channel.hpp"

#include "core/platform/block_source.hpp"
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

HostChannel::HostChannel(BlockSource &memory, LogSink &log, const char *prefix, const ChannelShape &shape)
    : memory_(memory), lines_(log, prefix), sub_api_(shape.sub_api_), ring_order_(shape.ring_order_),
      reply_bytes_(shape.reply_bytes_), staging_bytes_(shape.staging_bytes_), batch_bytes_(shape.batch_bytes_),
      arena_bytes_(shape.arena_bytes_), staging_(nullptr), batch_block_(nullptr), transport_(), batched_(), arena_()
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
    static_cast<void>(transport_.construct(device, sub_api_, ring_order_, reply_bytes_, staging_, staging_bytes_));
    const Status initialized = transport_.object().initialize(options);
    if (initialized != Status::ok) {
        lines_.write("the driver refused the context");
        return initialized;
    }
    if (batch_bytes_ != 0u) {
        batch_block_ = static_cast<uint8_t *>(memory_.allocate(batch_bytes_, Pool::paged));
        if (batch_block_ == nullptr) return Status::no_memory;
        static_cast<void>(batched_.construct(transport_.object(), batch_block_, batch_bytes_, staging_bytes_));
    }
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
    if (!lost && batched_.constructed()) static_cast<void>(batched_.object().flush());
    if (arena_.constructed()) {
        /* Neither answer changes what is torn down next: a close the device
         * is not told of is one it does with the context. */
        const Status closed = lost ? arena_.object().abandon() : arena_.object().close();
        if (closed != Status::ok) lines_.write("the blob arena did not close whole");
        arena_.destroy();
    }
    batched_.destroy();
    transport_.destroy();
    if (batch_block_ != nullptr) memory_.release(batch_block_);
    batch_block_ = nullptr;
    if (staging_ != nullptr) memory_.release(staging_);
    staging_ = nullptr;
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

BatchedTransport &HostChannel::batched()
{
    AV_SCOPE("HostChannel::batched");
    return batched_.object();
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
    if (!opened()) return staging_ == nullptr && batch_block_ == nullptr;
    if (staging_ == nullptr) return false;
    if (batched_.constructed() != (batch_bytes_ != 0u) || batched_.constructed() != (batch_block_ != nullptr))
        return false;
    return transport_.object().invariants_hold();
}

}  // namespace av::icd
