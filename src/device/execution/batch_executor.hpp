/* Runs one context's batches on its worker (docs/protocol.md): each batch's
 * payload copied once from the ring, walked command by command, the context's
 * API's commands run through its executor in order, the transport's own
 * answered by the device, and the rest skipped, and
 * the answer of the last command that asked for one written to the reply slot
 * before the batch completes. Every command run moves the context's progress,
 * so a long batch reads as busy rather than hung. */
#ifndef AV_BATCH_EXECUTOR_HPP
#define AV_BATCH_EXECUTOR_HPP

#include "core/support/status.hpp"
#include "device/execution/host_poll.hpp"
#include "device/memory/blob_window.hpp"
#include "proto/submission/fault_code.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BatchHeader;
class BatchStaging;
class BlobImports;
class CommandHeader;
class ContextExecutor;
class FenceReporter;
class Host;
class RingWindow;
class WireRecorder;
struct BatchExecutorProbe;

class BatchExecutor {
public:
    BatchExecutor(Host &host, ContextExecutor &executor, BlobImports &imports, BatchStaging &staging,
                  FenceReporter &fences, WireRecorder &recorder, uint32_t context);
    BatchExecutor(const BatchExecutor &) = delete;
    BatchExecutor &operator=(const BatchExecutor &) = delete;

    /* Main loop, at setup: answers go to `reply`, an empty window for a
     * context without a reply slot. */
    void                 prepare(const BlobWindow &reply);
    /* Main loop: forgets the reply slot of a setup that failed. */
    void                 withdraw();
    /* Worker: the executor's answer to being created. */
    [[nodiscard]] Status start();
    /* Worker. */
    void                 stop();
    bool                 started() const;
    /* Worker: the batch whose header is at `head` of `ring`. ok when every
     * command ran or was skipped; malformed for commands no producer could
     * frame, an answer asked for with no reply slot, or a payload the
     * executor cannot decode; invalid_state for a lost device or no executor. */
    /* It has a host executor to run batches on: a context whose executor never
     * opened holds batches that will not run, and nothing waits on them. */
    bool                 ready() const;
    /* Worker thread, between batches: see AvExecutorOps::poll. Idle with no
     * executor. */
    HostPoll             poll();
    /* Worker: see AvExecutorOps::gpu_progress. */
    void                 gpu_progress(uint64_t &marked, uint64_t &reached) const;
    [[nodiscard]] Status run(const RingWindow &ring, uint64_t head, const BatchHeader &batch);

    /* Batches run, whether or not they completed. */
    uint64_t batches() const;
    uint64_t commands() const;
    uint64_t skipped() const;
    /* Why the last run that failed failed; none before one has. */
    FaultCode fault() const;

    bool invariants_hold() const;

private:
    /* Which fault a failed command is, from the status the executor's answer
     * became. */
    FaultCode fault_for(Status status) const;

    [[nodiscard]] Status run_command(const CommandHeader &header, const uint8_t *payload, bool &answered,
                                     size_t &answer);
    /* Records the fault `fault_` now names against `fence` and answers
     * `status`, so every way a batch can fail leaves one record. */
    [[nodiscard]] Status failed(uint64_t fence, Status status);
    void                 write_reply(uint64_t fence_id, size_t answer);

    Host            &host_;
    ContextExecutor &executor_;
    BlobImports     &imports_;
    BatchStaging    &staging_;
    FenceReporter   &fences_;
    WireRecorder    &recorder_;
    uint32_t         context_;
    FaultCode        fault_;
    BlobWindow       reply_;
    uint64_t         batches_;
    uint64_t         commands_;
    uint64_t         skipped_;

    friend struct BatchExecutorProbe;
};

}  // namespace av

#endif  // AV_BATCH_EXECUTOR_HPP
