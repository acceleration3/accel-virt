#include "conformance/executor/executor_transport.hpp"

namespace av {

ExecutorTransport::ExecutorTransport(const AvExecutorOps &ops, AvExecutor *executor)
    : ops_(ops), executor_(executor), request_{}, reply_{}, block_{}, arena_(block_, sizeof block_), blobs_(),
      ran_(false)
{}

WireWriter ExecutorTransport::request_writer() { return WireWriter(request_, sizeof request_); }

TransportResult ExecutorTransport::send_async(std::uint32_t command_id, WireWriter &writer)
{
    std::size_t answered = 0u;
    return deliver(command_id, writer, answered);
}

TransportResult ExecutorTransport::send_sync(std::uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    std::size_t           answered = 0u;
    const TransportResult delivered = deliver(command_id, writer, answered);
    reply = WireReader(reply_, delivered == TransportResult::ok ? answered : 0u);
    return delivered;
}

void ExecutorTransport::end_call() {}

WireArena &ExecutorTransport::decode_arena()
{
    arena_.reset();
    return arena_;
}

ClientBlobArena &ExecutorTransport::blobs() { return blobs_; }

bool ExecutorTransport::ran() const { return ran_; }

TransportResult ExecutorTransport::deliver(std::uint32_t command_id, WireWriter &writer, std::size_t &answered)
{
    ran_ = false;
    answered = 0u;
    if (writer.status() != Status::ok) return TransportResult::failed;
    const std::uint32_t result = ops_.execute(ops_.opaque, executor_, command_id, request_, writer.offset(),
                                              reply_, sizeof reply_, &answered);
    ran_ = result == AV_EXECUTED && answered <= sizeof reply_;
    if (!ran_) answered = 0u;
    return ran_ ? TransportResult::ok : TransportResult::failed;
}

}
