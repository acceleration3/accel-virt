#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/batch_executor.hpp"
#include "device/execution/batch_staging.hpp"
#include "device/execution/blob_imports.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "proto/context/gl_context_options.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/memory/host_blob_table.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/memory/blob_window.hpp"
#include "device/context/ring_window.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/context/guest_ring.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/reply_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t   capacity = 4096u;
constexpr std::uint8_t  uuid[16] = {};
constexpr std::uint64_t reply_length = 32u + 128u;

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    const bool          with_reply = (data[0] & 1u) != 0u;
    const std::uint32_t length = static_cast<std::uint32_t>(size - 1u < capacity - 16u ? size - 1u : capacity - 16u);
    static std::uint8_t ring[32u + capacity];
    static std::uint8_t reply[reply_length];
    std::memset(reply, 0, sizeof reply);
    void               *ring_pointer = ring;
    const std::uint64_t ring_length = sizeof ring;
    void               *reply_pointer = reply;
    const std::uint64_t reply_bytes = sizeof reply;
    av::GuestRing       guest(ring, 12u);
    guest.write_batch(1u, data + 1, length);

    av::MockHost        host;
    av::MockExecutorOps ops(AV_EXECUTOR_SUB_API_VULKAN);
    av::ExecutorLibrary library;
    if (library.accept(&ops.table()) != av::Status::ok) __builtin_trap();
    if (library.open_domain(uuid, 0u, 0u) != av::Status::ok) __builtin_trap();
    av::ExecutorShares  shares(host, library);
    av::ContextExecutor executor(host, library, shares, 1u);
    executor.configure(av::SubApi::vulkan, uuid, av::GlContextOptions());
    av::BatchStaging staging(host);
    if (staging.allocate(capacity, with_reply ? reply_length - 32u : 0u) != av::Status::ok) __builtin_trap();
    av::FenceInterrupts interrupts(host);
    av::WireRecorder    recorder(host);
    av::FenceReporter   fences(host, interrupts, recorder, 1u);
    av::HostBlobTable   blobs(host);
    av::BlobImports     imports(host, blobs, executor, recorder, 1u);
    av::BatchExecutor batches(host, executor, imports, staging, fences, recorder, 1u);
    batches.prepare(with_reply ? av::BlobWindow(&reply_pointer, &reply_bytes, 1u, reply_bytes) : av::BlobWindow());
    if (batches.start() != av::Status::ok) __builtin_trap();
    const av::RingWindow ring_window(av::BlobWindow(&ring_pointer, &ring_length, 1u, ring_length), 12u);
    const av::Status ran = batches.run(ring_window, 0u, av::BatchHeader(length, 1u));
    if (!batches.invariants_hold() || (ran == av::Status::ok) != (batches.fault() == av::FaultCode::none))
        __builtin_trap();
    av::ReplyHeader header;
    const bool      written = reply[0] != 0u;
    if (written && header.decode(reply, 32u) != av::Status::ok) __builtin_trap();
    batches.stop();
    while (host.claim(av::HostViolation::worker_required)) {}
    if (ops.misuses() != 0u || ops.live() != 0u) __builtin_trap();
    return 0;
}
