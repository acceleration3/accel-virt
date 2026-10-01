#include "icd/vulkan/connection.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/transport/host_window_chunk.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/common/thread_lock_hold.hpp"
#include "proto/context/sub_api.hpp"

namespace av::icd {

Connection::Connection(Kmt &kmt, BlockSource &memory, ThreadLock &lock, LogSink &log)
    : kmt_(kmt), lock_(lock), search_(kmt), connection_(kmt),
      channel_(memory, log, "avvk: ", ChannelShape(SubApi::vulkan, ring_order, reply_bytes, command_bytes, 0u, 0u)),
      locked_(), luid_(0u), host_dma_buf_(false)
{
    AV_SCOPE("Connection::Connection");
}

Connection::~Connection()
{
    AV_SCOPE("Connection::~Connection");
    close();
}

Status Connection::open()
{
    AV_SCOPE("Connection::open");
    if (opened()) return Status::invalid_state;
    ScopeExit undo([&] { close(); });
    const PciIdentity avgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::av_subsystem);
    if (search_.find(avgpu) != Status::ok) return Status::not_supported;
    if (kmt_.adapter_luid(search_.adapter(), luid_) != Status::ok) return Status::not_supported;
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

Status Connection::open_chunk(ObjectStorage<HostWindowChunk> &chunk, uint64_t bytes)
{
    AV_SCOPE("Connection::open_chunk");
    if (!opened() || faulted() || chunk.constructed()) return Status::invalid_state;
    const icd::ThreadLockHold release(lock_);
    static_cast<void>(chunk.construct(connection_, channel_.transport()));   /* checked empty above */
    const Status status = chunk.object().open(bytes);
    if (status != Status::ok) chunk.destroy();
    return status;
}

Status Connection::close_chunk(ObjectStorage<HostWindowChunk> &chunk)
{
    AV_SCOPE("Connection::close_chunk");
    if (!chunk.constructed()) return Status::ok;
    const icd::ThreadLockHold release(lock_);
    const Status released = chunk.object().release();
    chunk.destroy();
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
