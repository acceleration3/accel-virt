#ifndef AV_MOCK_CONTEXT_EXECUTION_HPP
#define AV_MOCK_CONTEXT_EXECUTION_HPP

#include "device/execution/batch_executor.hpp"
#include "device/execution/batch_staging.hpp"
#include "device/execution/blob_imports.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/memory/host_blob_table.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/executor/mock_executor_ops.hpp"

#include <cstdint>

namespace av {

class FenceReporter;
class Host;

class MockContextExecution {
public:
    MockContextExecution(Host &host, FenceReporter &fences, std::uint8_t order);
    MockContextExecution(const MockContextExecution &) = delete;
    MockContextExecution &operator=(const MockContextExecution &) = delete;

    MockExecutorOps &ops();
    BatchExecutor   &batches();
    WireRecorder    &recorder();

private:
    MockExecutorOps ops_;
    WireRecorder    recorder_;
    ExecutorLibrary library_;
    ExecutorShares  shares_;
    ContextExecutor executor_;
    HostBlobTable   blobs_;
    BlobImports          imports_;
    BatchStaging    staging_;
    BatchExecutor   batches_;
};

}

#endif
