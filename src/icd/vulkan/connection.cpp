#include "icd/vulkan/connection.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/common/thread_lock_hold.hpp"
#include "proto/context/sub_api.hpp"

namespace av::icd {

Connection::Connection(Kmt &kmt, BlockSource &memory, ThreadLock &lock, LogSink &log)
    : kmt_(kmt), lock_(lock), search_(kmt), connection_(kmt),
      channel_(memory, log, "avvk: ", ChannelShape(SubApi::vulkan, ring_order, reply_bytes, command_bytes, 0u, 0u)),
      locked_(), luid_(0u), name_(), host_dma_buf_(false)
{
    AV_SCOPE("Connection::Connection");
}

Connection::~Connection()
{
    AV_SCOPE("Connection::~Connection");
    close();
}

Status Connection::open(uint64_t luid)
{
    AV_SCOPE("Connection::open");
    if (opened()) return Status::invalid_state;
    ScopeExit undo([&] { close(); });
    if (search_.find_avgpu(luid) != Status::ok) return Status::not_supported;
    if (kmt_.adapter_luid(search_.adapter(), luid_) != Status::ok) return Status::not_supported;
    /* A name Windows will not give is no reason to refuse the adapter: the
     * host's own name stands. */
    uint16_t wide[AdapterString::bytes] = {};
    if (kmt_.query_adapter_name(search_.adapter(), wide, AdapterString::bytes) == Status::ok) name_.take(wide);
    if (connection_.open(search_.adapter()) != Status::ok) return Status::not_supported;
    const Status opened = channel_.open(connection_);
    if (opened != Status::ok) return opened == Status::no_memory ? Status::no_memory : Status::not_supported;
    static_cast<void>(locked_.construct(channel_.transport(), lock_));
    undo.dismiss();
    return Status::ok;
}

void Connection::close()
{
    AV_SCOPE("Connection::close");
    locked_.destroy();
    /* No batch and no arena: nothing differs for a faulted channel. */
    channel_.close(false);
    connection_.close();
    /* Nothing is left to report a failed close to: the instance is gone. */
    static_cast<void>(search_.close());
    luid_ = 0u;
    name_.take(nullptr);
}

void Connection::report(StateWriter &writer) const
{
    AV_SCOPE("Connection::report");
    writer.flag("opened", opened());
    writer.flag("faulted", faulted());
    writer.number("adapter", adapter());
    writer.hex("luid", luid_);
    writer.number("device", opened() ? connection_.device() : 0u);
    writer.number("context", opened() ? channel_.transport().context() : 0u);
}

bool             Connection::opened() const
{
    AV_SCOPE("Connection::opened");
    return locked_.constructed();
}
bool             Connection::faulted() const
{
    AV_SCOPE("Connection::faulted");
    return opened() && channel_.faulted();
}
uint32_t         Connection::adapter() const
{
    AV_SCOPE("Connection::adapter");
    return opened() ? connection_.adapter() : 0u;
}

uint32_t Connection::device() const
{
    AV_SCOPE("Connection::device");
    return opened() ? connection_.device() : 0u;
}

uint64_t         Connection::luid() const
{
    AV_SCOPE("Connection::luid");
    return luid_;
}
const char *Connection::adapter_name() const
{
    AV_SCOPE("Connection::adapter_name");
    return name_.text();
}

uint64_t Connection::submissions() const
{
    AV_SCOPE("Connection::submissions");
    return channel_.opened() ? channel_.transport().submissions() : 0u;
}

ClientTransport &Connection::transport()
{
    AV_SCOPE("Connection::transport");
    return locked_.object();
}

TransportResult Connection::send_batch(const uint8_t *framed, size_t length)
{
    AV_SCOPE("Connection::send_batch");
    const icd::ThreadLockHold release(lock_);
    WireReader unanswered(nullptr, 0u);
    return channel_.transport().send_batch(framed, length, unanswered);
}

Status Connection::open_memory(ObjectStorage<HostAllocation> &allocation, uint64_t bytes, HostMemoryRequest memory)
{
    AV_SCOPE("Connection::open_memory");
    if (!opened() || faulted() || allocation.constructed()) return Status::invalid_state;
    const icd::ThreadLockHold release(lock_);
    static_cast<void>(allocation.construct(connection_, channel_.transport()));   /* checked empty above */
    const Status status = allocation.object().open(bytes, memory);
    if (status != Status::ok) allocation.destroy();
    return status;
}

Status Connection::close_memory(ObjectStorage<HostAllocation> &allocation)
{
    AV_SCOPE("Connection::close_memory");
    if (!allocation.constructed()) return Status::ok;
    const icd::ThreadLockHold release(lock_);
    const Status released = faulted() ? allocation.object().abandon() : allocation.object().close();
    allocation.destroy();
    return released;
}

bool Connection::invariants_hold() const
{
    AV_SCOPE("Connection::invariants_hold");
    if (!connection_.invariants_hold()) return false;
    if (!channel_.invariants_hold()) return false;
    if (!opened()) return !channel_.opened() && !connection_.opened() && !search_.found();
    return channel_.opened() && connection_.opened() && search_.found() && connection_.adapter() == search_.adapter();
}

void Connection::note_host_dma_buf()
{
    AV_SCOPE("Connection::note_host_dma_buf");
    host_dma_buf_ = true;
}

bool Connection::host_has_dma_buf() const
{
    AV_SCOPE("Connection::host_has_dma_buf");
    return host_dma_buf_;
}

}  // namespace av::icd
