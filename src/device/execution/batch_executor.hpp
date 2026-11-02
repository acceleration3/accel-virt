#ifndef AV_BATCH_EXECUTOR_HPP
#define AV_BATCH_EXECUTOR_HPP

#include "core/support/status.hpp"
#include "device/execution/host_poll.hpp"
#include "device/execution/gathered_command.hpp"
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

    void                 prepare(const BlobWindow &reply);
    void                 withdraw();
    [[nodiscard]] Status start();
    void                 stop();
    void                 abandon();
    bool                 abandoned() const;
    bool                 started() const;
    bool                 ready() const;
    HostPoll             poll();
    void                 gpu_progress(uint64_t &marked, uint64_t &reached) const;
    [[nodiscard]] Status run(const RingWindow &ring, uint64_t head, const BatchHeader &batch,
                             uint64_t *answering = nullptr);

    uint64_t batches() const;
    uint64_t commands() const;
    uint64_t skipped() const;
    FaultCode fault() const;

    bool invariants_hold() const;

private:
    FaultCode fault_for(Status status) const;

    [[nodiscard]] Status run_command(const CommandHeader &header, const uint8_t *payload, bool &answered,
                                     size_t &answer);
    [[nodiscard]] Status run_whole(const CommandHeader &header, const uint8_t *payload, size_t size, bool &answered,
                                   size_t &answer);
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
    GatheredCommand  gathered_;
    bool             abandoned_;

    friend struct BatchExecutorProbe;
};

}

#endif
