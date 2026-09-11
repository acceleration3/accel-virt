#include "harness/executor/mock_context_execution.hpp"

#include "device/memory/blob_window.hpp"
#include "proto/context/sub_api.hpp"

namespace av {

namespace {

constexpr std::uint8_t no_uuid[ContextExecutor::uuid_bytes] = {};

}  // namespace

MockContextExecution::MockContextExecution(Host &host, std::uint8_t order)
    : ops_(RVK_EXECUTOR_SUB_API_VULKAN), library_(), executor_(host, library_), staging_(host),
      batches_(host, executor_, staging_)
{
    static_cast<void>(library_.accept(&ops_.table()));
    executor_.configure(SubApi::vulkan, no_uuid);
    static_cast<void>(staging_.allocate(std::uint64_t{1} << order, 0u));
    batches_.prepare(BlobWindow());
}

MockExecutorOps &MockContextExecution::ops() { return ops_; }
BatchExecutor   &MockContextExecution::batches() { return batches_; }

}  // namespace av
