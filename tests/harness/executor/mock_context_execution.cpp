#include "harness/executor/mock_context_execution.hpp"

#include "device/memory/blob_window.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"

namespace av {

namespace {

constexpr std::uint8_t no_uuid[ContextExecutor::uuid_bytes] = {};

}  // namespace

MockContextExecution::MockContextExecution(Host &host, FenceReporter &fences, std::uint8_t order)
    : ops_(AV_EXECUTOR_SUB_API_VULKAN), recorder_(host), library_(), shares_(host, library_),
      executor_(host, library_, shares_, 1u), blobs_(host), shared_objects_(host),
      imports_(host, blobs_, executor_, recorder_, 1u), shared_(shared_objects_, executor_), staging_(host),
      batches_(host, executor_, imports_, shared_, staging_, fences, recorder_, 1u)
{
    static_cast<void>(library_.accept(&ops_.table()));
    executor_.configure(SubApi::vulkan, no_uuid, GlContextOptions());
    static_cast<void>(staging_.allocate(std::uint64_t{1} << order, 0u));
    batches_.prepare(BlobWindow());
}

MockExecutorOps &MockContextExecution::ops() { return ops_; }
WireRecorder    &MockContextExecution::recorder() { return recorder_; }
BatchExecutor   &MockContextExecution::batches() { return batches_; }

}  // namespace av
