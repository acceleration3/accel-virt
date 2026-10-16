#include "icd/vulkan/connection.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "proto/context/sub_api.hpp"

namespace av::icd {

Connection::Connection(Kmt &kmt, BlockSource &memory, ThreadLock &lock, LogSink &log, KeyedLog &lines)
    : kmt_(kmt), search_(kmt), connection_(kmt),
      channel_(memory, log, "avvk: ", ChannelShape(SubApi::vulkan, ring_order, reply_bytes, command_bytes, 0u, 0u),
               lock),
      loss_(lines), opened_(false), adapter_(), host_dma_buf_(false)
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
    /* A name Windows will not give is no reason to refuse the adapter: the
     * host's own name stands. Its LUID is what names it. */
    adapter_.learn(kmt_, search_.adapter());
    if (!adapter_.knows(AdapterFacts::luid_fact)) return Status::not_supported;
    if (connection_.open(search_.adapter()) != Status::ok) return Status::not_supported;
    const Status opened = channel_.open(connection_);
    if (opened != Status::ok) return opened == Status::no_memory ? Status::no_memory : Status::not_supported;
    opened_ = true;
    undo.dismiss();
    return Status::ok;
}

void Connection::close()
{
    AV_SCOPE("Connection::close");
    opened_ = false;
    /* No batch and no arena: nothing differs for a faulted channel. */
    channel_.close(false);
    connection_.close();
    /* Nothing is left to report a failed close to: the instance is gone. */
    static_cast<void>(search_.close());
    adapter_.forget();
}

void Connection::report(StateWriter &writer) const
{
    AV_SCOPE("Connection::report");
    writer.flag("opened", opened());
    loss_.report(writer);
    writer.number("adapter", adapter());
    writer.hex("luid", adapter_.luid());
    writer.number("device", opened() ? connection_.device() : 0u);
    writer.number("context", opened() ? channel_.transport().context() : 0u);
}

bool             Connection::opened() const
{
    AV_SCOPE("Connection::opened");
    return opened_;
}
bool             Connection::lost() const
{
    AV_SCOPE("Connection::lost");
    return opened() && channel_.lost();
}

bool Connection::refuses(const char *entry)
{
    AV_SCOPE("Connection::refuses");
    if (lost()) loss_.lose(channel_.fault_code());
    return loss_.refuses(entry);
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
    return adapter_.luid();
}
const char *Connection::adapter_name() const
{
    AV_SCOPE("Connection::adapter_name");
    return adapter_.name();
}

uint64_t Connection::submissions() const
{
    AV_SCOPE("Connection::submissions");
    return channel_.opened() ? channel_.transport().submissions() : 0u;
}

ClientTransport &Connection::transport()
{
    AV_SCOPE("Connection::transport");
    return channel_.wire();
}

TransportResult Connection::send_batch(const uint8_t *framed, size_t length)
{
    AV_SCOPE("Connection::send_batch");
    return channel_.send_batch(framed, length);
}

TransportResult Connection::send_command(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("Connection::send_command");
    return channel_.send_command(command_id, writer);
}

HostChannel &Connection::channel()
{
    AV_SCOPE("Connection::channel");
    return channel_;
}

bool Connection::invariants_hold() const
{
    AV_SCOPE("Connection::invariants_hold");
    if (!connection_.invariants_hold()) return false;
    if (!channel_.invariants_hold() || !loss_.invariants_hold()) return false;
    if (loss_.lost() && opened() && !channel_.lost()) return false;
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
