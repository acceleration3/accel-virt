/* What one context executes with, over a mock executor: the library, the
 * context's executor configured for Vulkan, staging for a ring of `order`, and
 * the batch executor reporting to `fences`, without a reply slot. For tests of
 * what sits above it. */
#ifndef AV_MOCK_CONTEXT_EXECUTION_HPP
#define AV_MOCK_CONTEXT_EXECUTION_HPP

#include "device/execution/batch_executor.hpp"
#include "device/execution/batch_staging.hpp"
#include "device/execution/blob_imports.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/memory/blob_table.hpp"
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

private:
    MockExecutorOps ops_;
    ExecutorLibrary library_;
    ContextExecutor executor_;
    BlobTable       blobs_;
    BlobImports     imports_;
    BatchStaging    staging_;
    BatchExecutor   batches_;
};

}  // namespace av

#endif  // AV_MOCK_CONTEXT_EXECUTION_HPP
