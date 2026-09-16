#include "device/execution/batch_executor.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/batch_staging.hpp"
#include "proto/submission/command_stream.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/transport_command.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/host/host.hpp"
#include "device/ring/ring_window.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/reply_header.hpp"

namespace av {

BatchExecutor::BatchExecutor(Host &host, ContextExecutor &executor, BlobImports &imports, BatchStaging &staging,
                             FenceReporter &fences)
    : host_(host), executor_(executor), imports_(imports), staging_(staging), fences_(fences), fault_(FaultCode::none), reply_(),
      batches_(0u), commands_(0u), skipped_(0u)
      {
          AV_SCOPE("BatchExecutor::BatchExecutor");
      }

void BatchExecutor::prepare(const BlobWindow &reply)
{
    AV_SCOPE("BatchExecutor::prepare");
    host_.require_main_loop("BatchExecutor::prepare");
    reply_ = reply;
}

void BatchExecutor::withdraw()
{
    AV_SCOPE("BatchExecutor::withdraw");
    host_.require_main_loop("BatchExecutor::withdraw");
    reply_ = BlobWindow();
}

Status BatchExecutor::start()
{
    AV_SCOPE("BatchExecutor::start");
    host_.require_worker("BatchExecutor::start");
    return executor_.create();
}

void BatchExecutor::stop()
{
    AV_SCOPE("BatchExecutor::stop");
    host_.require_worker("BatchExecutor::stop");
    executor_.destroy();
}

bool BatchExecutor::started() const
{
    AV_SCOPE("BatchExecutor::started");
    return executor_.created();
}

/* The copy is taken once; the ring's bytes are not read again for this batch. */
Status BatchExecutor::run(const RingWindow &ring, uint64_t head, const BatchHeader &batch)
{
    AV_SCOPE("BatchExecutor::run");
    host_.require_worker("BatchExecutor::run");
    if (!executor_.created()) {
        fault_ = FaultCode::no_executor;
        return Status::invalid_state;
    }
    const size_t length = batch.length();
    if (length != 0u && ring.copy(head + BatchHeader::encoded_size, staging_.payload(), length) != Status::ok) {
        fault_ = FaultCode::command_framing;
        return Status::malformed;
    }
    batches_++;
    CommandStream       stream(staging_.payload(), length);
    CommandHeader       header;
    const uint8_t      *payload = nullptr;
    bool                answered = false;
    size_t              answer = 0u;
    Status              status = Status::ok;
    while ((status = stream.next(header, payload)) == Status::ok) {
        status = run_command(header, payload, answered, answer);
        if (status != Status::ok) return status;
    }
    if (status != Status::no_space) {
        fault_ = FaultCode::command_framing;
        return status;
    }
    if (answered) write_reply(batch.fence_id(), answer);
    return Status::ok;
}

/* Only a command that asks for an answer is given somewhere to write it, so
 * a later command cannot overwrite the answer kept. */
FaultCode BatchExecutor::fault_for(Status status) const
{
    AV_SCOPE("BatchExecutor::fault_for");
    if (status == Status::malformed) return FaultCode::undecodable;
    /* A library answering outside the ABI is a different problem from a GPU
     * that died, and the guest is told which so an operator looks in the right
     * place (feature 042). */
    if (status == Status::not_supported) return FaultCode::broken_executor;
    return FaultCode::device_lost;
}

Status BatchExecutor::run_command(const CommandHeader &header, const uint8_t *payload, bool &answered, size_t &answer)
{
    AV_SCOPE("BatchExecutor::run_command");
    if (header.generates_reply() && reply_.length() == 0u) {
        fault_ = FaultCode::command_framing;
        return Status::malformed;
    }
    const bool transport = header.api() == CommandHeader::transport_api;
    if (!transport && header.api() != executor_.command_api()) {
        skipped_++;
        return Status::ok;
    }
    const bool     replying = header.generates_reply();
    uint8_t *const reply = replying ? staging_.reply() : nullptr;
    const size_t   capacity = replying ? staging_.reply_capacity() : 0u;
    size_t         produced = 0u;
    const Status   status =
        transport ? TransportCommand(imports_, header, payload).run(reply, capacity, produced)
                  : executor_.execute(header.id(), payload, header.size() - CommandHeader::encoded_size, reply,
                                      capacity, produced);
    if (status == Status::not_found) {
        skipped_++;
        return Status::ok;
    }
    /* A transport command the device refuses was framed wrong; nothing was
     * handed to a library that could have failed to decode it. */
    if (status != Status::ok) {
        fault_ = transport ? FaultCode::command_framing : fault_for(status);
        return status;
    }
    fences_.heartbeat();
    commands_++;
    answered = answered || replying;
    answer = replying ? produced : answer;
    return Status::ok;
}

/* The slot was sized at setup, so both writes land inside it. */
void BatchExecutor::write_reply(uint64_t fence_id, size_t answer)
{
    AV_SCOPE("BatchExecutor::write_reply");
    const uint32_t required = answer < 0xFFFFFFFFu ? static_cast<uint32_t>(answer) : 0xFFFFFFFFu;
    const ReplyHeader header =
        ReplyHeader(fence_id, required).within(static_cast<uint32_t>(staging_.reply_capacity()));
    uint8_t bytes[ReplyHeader::encoded_size] = {};
    static_cast<void>(header.encode(bytes, sizeof bytes));
    static_cast<void>(reply_.write(0u, bytes, sizeof bytes));
    static_cast<void>(reply_.write(ReplyHeader::encoded_size, staging_.reply(), header.size()));
}

uint64_t BatchExecutor::batches() const
{
    AV_SCOPE("BatchExecutor::batches");
    return batches_;
}
uint64_t BatchExecutor::commands() const
{
    AV_SCOPE("BatchExecutor::commands");
    return commands_;
}
uint64_t  BatchExecutor::skipped() const
{
    AV_SCOPE("BatchExecutor::skipped");
    return skipped_;
}
FaultCode BatchExecutor::fault() const
{
    AV_SCOPE("BatchExecutor::fault");
    return fault_;
}

/* Every command run or skipped belongs to a batch that was run. */
bool BatchExecutor::invariants_hold() const
{
    AV_SCOPE("BatchExecutor::invariants_hold");
    return reply_.invariants_hold() && (batches_ != 0u || (commands_ == 0u && skipped_ == 0u));
}

}  // namespace av
