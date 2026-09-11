#include "device/execution/batch_executor.hpp"

#include "device/execution/batch_staging.hpp"
#include "device/execution/command_stream.hpp"
#include "device/execution/context_executor.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/host/host.hpp"
#include "device/ring/ring_window.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/reply_header.hpp"

namespace av {

BatchExecutor::BatchExecutor(Host &host, ContextExecutor &executor, BatchStaging &staging, FenceReporter &fences)
    : host_(host), executor_(executor), staging_(staging), fences_(fences), fault_(FaultCode::none), reply_(),
      batches_(0u), commands_(0u), skipped_(0u) {}

void BatchExecutor::prepare(const BlobWindow &reply)
{
    host_.require_main_loop("BatchExecutor::prepare");
    reply_ = reply;
}

void BatchExecutor::withdraw()
{
    host_.require_main_loop("BatchExecutor::withdraw");
    reply_ = BlobWindow();
}

Status BatchExecutor::start()
{
    host_.require_worker("BatchExecutor::start");
    return executor_.create();
}

void BatchExecutor::stop()
{
    host_.require_worker("BatchExecutor::stop");
    executor_.destroy();
}

bool BatchExecutor::started() const
{
    return executor_.created();
}

/* The copy is taken once; the ring's bytes are not read again for this batch. */
Status BatchExecutor::run(const RingWindow &ring, uint64_t head, const BatchHeader &batch)
{
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
Status BatchExecutor::run_command(const CommandHeader &header, const uint8_t *payload, bool &answered, size_t &answer)
{
    if (header.generates_reply() && reply_.length() == 0u) {
        fault_ = FaultCode::command_framing;
        return Status::malformed;
    }
    if (header.api() != executor_.command_api()) {
        skipped_++;
        return Status::ok;
    }
    const bool   replying = header.generates_reply();
    size_t       produced = 0u;
    const Status status = executor_.execute(header.id(), payload, header.size() - CommandHeader::encoded_size,
                                            replying ? staging_.reply() : nullptr,
                                            replying ? staging_.reply_capacity() : 0u, produced);
    if (status == Status::not_found) {
        skipped_++;
        return Status::ok;
    }
    if (status != Status::ok) {
        fault_ = status == Status::malformed ? FaultCode::undecodable : FaultCode::device_lost;
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
    const uint32_t required = answer < 0xFFFFFFFFu ? static_cast<uint32_t>(answer) : 0xFFFFFFFFu;
    const ReplyHeader header =
        ReplyHeader(fence_id, required).within(static_cast<uint32_t>(staging_.reply_capacity()));
    uint8_t bytes[ReplyHeader::encoded_size] = {};
    static_cast<void>(header.encode(bytes, sizeof bytes));
    static_cast<void>(reply_.write(0u, bytes, sizeof bytes));
    static_cast<void>(reply_.write(ReplyHeader::encoded_size, staging_.reply(), header.size()));
}

uint64_t BatchExecutor::batches() const  { return batches_; }
uint64_t BatchExecutor::commands() const { return commands_; }
uint64_t  BatchExecutor::skipped() const  { return skipped_; }
FaultCode BatchExecutor::fault() const    { return fault_; }

/* Every command run or skipped belongs to a batch that was run. */
bool BatchExecutor::invariants_hold() const
{
    return reply_.invariants_hold() && (batches_ != 0u || (commands_ == 0u && skipped_ == 0u));
}

}  // namespace av
