#include "device/execution/batch_executor.hpp"

#include "core/support/scope_exit.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/batch_staging.hpp"
#include "proto/submission/command_stream.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/transport_command.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/host/host.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/context/ring_window.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/reply_header.hpp"

namespace av {

BatchExecutor::BatchExecutor(Host &host, ContextExecutor &executor, BlobImports &imports, BatchStaging &staging,
                             FenceReporter &fences, WireRecorder &recorder, uint32_t context)
    : host_(host), executor_(executor), imports_(imports), staging_(staging), fences_(fences),
      recorder_(recorder), context_(context), fault_(FaultCode::none), reply_(),
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

bool BatchExecutor::poll()
{
    AV_SCOPE("BatchExecutor::poll");
    return executor_.created() && executor_.poll();
}

void BatchExecutor::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("BatchExecutor::gpu_progress");
    executor_.gpu_progress(marked, reached);
}

bool BatchExecutor::ready() const
{
    AV_SCOPE("BatchExecutor::ready");
    return executor_.created();
}

/* The copy is taken once; the ring's bytes are not read again for this batch. */
Status BatchExecutor::run(const RingWindow &ring, uint64_t head, const BatchHeader &batch)
{
    AV_SCOPE("BatchExecutor::run");
    host_.require_worker("BatchExecutor::run");
    if (!executor_.created()) {
        fault_ = FaultCode::no_executor;
        return failed(batch.fence_id(), Status::invalid_state);
    }
    const size_t length = batch.length();
    if (length != 0u && ring.copy(head + BatchHeader::encoded_size, staging_.payload(), length) != Status::ok) {
        fault_ = FaultCode::command_framing;
        return failed(batch.fence_id(), Status::malformed);
    }
    batches_++;
    /* Before the first command runs, and flushed: a host driver that takes
     * QEMU down with it leaves behind the batch it was given. */
    recorder_.record_batch(context_, batch.fence_id(), staging_.payload(), static_cast<uint32_t>(length));
    CommandStream       stream(staging_.payload(), length);
    CommandHeader       header;
    const uint8_t      *payload = nullptr;
    bool                answered = false;
    size_t              answer = 0u;
    Status              status = Status::ok;
    /* The end publishes too - OpenGL's error word, Direct3D's query results
     * - and the consumer's head and fence follow it. */
    ScopeExit           ended([this] {
        executor_.end_batch();
        host_.fence_stores();
    });
    while ((status = stream.next(header, payload)) == Status::ok) {
        status = run_command(header, payload, answered, answer);
        if (status != Status::ok) return failed(batch.fence_id(), status);
    }
    if (status != Status::no_space) {
        fault_ = FaultCode::command_framing;
        return failed(batch.fence_id(), status);
    }
    /* Before anything says the batch ran - its answer here, then the
     * consumer's head and fence - what its commands stored into the guest's
     * memory, through mappings the executor's host drivers made, has to be
     * there when the guest looks. */
    host_.fence_stores();
    if (answered) write_reply(batch.fence_id(), answer);
    return Status::ok;
}

Status BatchExecutor::failed(uint64_t fence, Status status)
{
    AV_SCOPE("BatchExecutor::failed");
    recorder_.record_fault(context_, fence, fault_);
    return status;
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
        transport ? TransportCommand(imports_, executor_, header, payload).run(reply, capacity, produced)
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
    /* Progress is what the guest's timeout detection watches in a long batch;
     * every command's store to the shared page is a line the guest's DPC then
     * pulls back across, so it goes out every few hundred. */
    constexpr uint64_t commands_per_heartbeat = 256u;
    if (commands_ % commands_per_heartbeat == 0u) fences_.heartbeat();
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
    /* The fence id goes last: a guest polling the slot takes the answer as
     * soon as it sees its own fence there, so everything else must be in
     * place first. */
    constexpr size_t fence_at = 8u;
    constexpr size_t fence_bytes = 8u;
    static_cast<void>(reply_.write(ReplyHeader::encoded_size, staging_.reply(), header.size()));
    static_cast<void>(reply_.write(0u, bytes, fence_at));
    static_cast<void>(reply_.write(fence_at + fence_bytes, bytes + fence_at + fence_bytes,
                                   sizeof bytes - fence_at - fence_bytes));
    __atomic_thread_fence(__ATOMIC_RELEASE);
    static_cast<void>(reply_.write(fence_at, bytes + fence_at, fence_bytes));
    recorder_.record_reply(context_, fence_id, staging_.reply(), header.size());
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
