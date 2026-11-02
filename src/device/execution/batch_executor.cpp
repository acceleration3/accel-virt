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
      batches_(0u), commands_(0u), skipped_(0u), gathered_(host), abandoned_(false)
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
    __atomic_store_n(&abandoned_, false, __ATOMIC_RELEASE);
    return executor_.create();
}

void BatchExecutor::stop()
{
    AV_SCOPE("BatchExecutor::stop");
    host_.require_worker("BatchExecutor::stop");
    executor_.destroy();
    gathered_.clear();
}

void BatchExecutor::abandon()
{
    AV_SCOPE("BatchExecutor::abandon");
    __atomic_store_n(&abandoned_, true, __ATOMIC_RELEASE);
}

bool BatchExecutor::abandoned() const
{
    AV_SCOPE("BatchExecutor::abandoned");
    return __atomic_load_n(&abandoned_, __ATOMIC_ACQUIRE);
}

bool BatchExecutor::started() const
{
    AV_SCOPE("BatchExecutor::started");
    return executor_.created();
}

HostPoll BatchExecutor::poll()
{
    AV_SCOPE("BatchExecutor::poll");
    return executor_.poll();
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

Status BatchExecutor::run(const RingWindow &ring, uint64_t head, const BatchHeader &batch, uint64_t *answering)
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
    recorder_.record_batch(context_, batch.fence_id(), staging_.payload(), static_cast<uint32_t>(length));
    CommandStream       stream(staging_.payload(), length);
    CommandHeader       header;
    const uint8_t      *payload = nullptr;
    bool                answered = false;
    size_t              answer = 0u;
    Status              status = Status::ok;
    ScopeExit           ended([this] {
        executor_.end_batch();
        host_.fence_stores();
    });
    while ((status = stream.next(header, payload)) == Status::ok) {
        if (abandoned()) return Status::invalid_state;
        if (answering != nullptr && header.generates_reply())
            __atomic_store_n(answering, head + batch.span(), __ATOMIC_RELEASE);
        status = run_command(header, payload, answered, answer);
        if (status != Status::ok) return failed(batch.fence_id(), status);
    }
    if (status != Status::no_space) {
        fault_ = FaultCode::command_framing;
        return failed(batch.fence_id(), status);
    }
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

FaultCode BatchExecutor::fault_for(Status status) const
{
    AV_SCOPE("BatchExecutor::fault_for");
    if (status == Status::malformed) return FaultCode::undecodable;
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
    const size_t length = header.size() - CommandHeader::encoded_size;
    const bool   transport = header.api() == CommandHeader::transport_api;
    const bool   piece = transport && header.id() == CommandHeader::piece_id;
    if ((piece && header.generates_reply()) ||
        (gathered_.gathering() && !piece && (transport || header.api() != executor_.command_api()))) {
        gathered_.clear();
        fault_ = FaultCode::command_framing;
        return Status::malformed;
    }
    if (piece || gathered_.gathering()) {
        const uint8_t *whole = nullptr;
        size_t         size = 0u;
        const Status   held = piece ? gathered_.gather(payload, length) : gathered_.close(payload, length, whole, size);
        if (held != Status::ok) {
            fault_ = FaultCode::undecodable;
            return Status::malformed;
        }
        if (piece) return Status::ok;
        const Status ran = run_whole(header, whole, size, answered, answer);
        gathered_.clear();
        return ran;
    }
    return run_whole(header, payload, length, answered, answer);
}

Status BatchExecutor::run_whole(const CommandHeader &header, const uint8_t *payload, size_t size, bool &answered,
                                size_t &answer)
{
    AV_SCOPE("BatchExecutor::run_whole");
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
                  : executor_.execute(header.id(), payload, size, reply, capacity, produced);
    if (status == Status::not_found) {
        skipped_++;
        return Status::ok;
    }
    if (status != Status::ok) {
        fault_ = transport ? FaultCode::command_framing : fault_for(status);
        return status;
    }
    constexpr uint64_t commands_per_heartbeat = 256u;
    if (commands_ % commands_per_heartbeat == 0u) fences_.heartbeat();
    commands_++;
    answered = answered || replying;
    answer = replying ? produced : answer;
    return Status::ok;
}

void BatchExecutor::write_reply(uint64_t fence_id, size_t answer)
{
    AV_SCOPE("BatchExecutor::write_reply");
    const uint32_t required = answer < 0xFFFFFFFFu ? static_cast<uint32_t>(answer) : 0xFFFFFFFFu;
    const ReplyHeader header =
        ReplyHeader(fence_id, required).within(static_cast<uint32_t>(staging_.reply_capacity()));
    uint8_t bytes[ReplyHeader::encoded_size] = {};
    static_cast<void>(header.encode(bytes, sizeof bytes));
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

bool BatchExecutor::invariants_hold() const
{
    AV_SCOPE("BatchExecutor::invariants_hold");
    return reply_.invariants_hold() && (batches_ != 0u || (commands_ == 0u && skipped_ == 0u));
}

}
