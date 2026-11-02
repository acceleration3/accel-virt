#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "guest/transport/blob_import.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/host_allocation.hpp"
#include "guest/transport/host_window_chunk.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/submission/blob_import_answer.hpp"

#include <cstddef>
#include <cstdint>
#include <fcntl.h>

using av::BlobImport;
using av::BlobImportAnswer;
using av::FullStackRig;
using av::GuestConnection;
using av::HostAllocation;
using av::HostBlobId;
using av::HostMemoryRequest;
using av::HostWindowChunk;
using av::KmtTransport;
using av::MockExecutorOps;
using av::Status;
using av::SubApi;

namespace {

constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::uint8_t  ring_order = 12u;
constexpr std::uint64_t chunk_bytes = 0x3000u;
constexpr std::uint32_t transport_views = 3u;

const HostMemoryRequest upload(HostMemoryRequest::Kind::upload, 0u);

class Process {
public:
    explicit Process(FullStackRig &rig)
        : connection_(rig.kmt()), staging_{},
          transport_(connection_, SubApi::vulkan, ring_order, reply_bytes, staging_, sizeof staging_), started_(false)
    {
        started_ = connection_.open(av::DdiKmt::adapter_handle) == Status::ok && transport_.initialize() == Status::ok;
    }
    ~Process()
    {
        transport_.teardown();
        connection_.close();
    }

    bool             started() const { return started_; }
    GuestConnection &connection() { return connection_; }
    KmtTransport    &transport() { return transport_; }

private:
    GuestConnection connection_;
    std::uint8_t    staging_[staging_bytes];
    KmtTransport    transport_;
    bool            started_;
};

std::uint8_t *executor_view(FullStackRig &rig, std::uint32_t resource)
{
    MockExecutorOps &executor = rig.device().executor();
    return executor.memory_bytes(executor.last_opened_domain(), resource);
}

bool descriptor_open(int fd)
{
    return fd >= 0 && ::fcntl(fd, F_GETFD) != -1;
}

void lands_as_asked(FullStackRig &rig, Process &process, HostMemoryRequest memory)
{
    MockExecutorOps &executor = rig.device().executor();
    HostWindowChunk  chunk(process.connection(), process.transport());
    CHECK_STATUS(chunk.open(chunk_bytes - 0x10u, memory), Status::ok);
    CHECK(HostBlobId(chunk.resource_id()).valid());
    CHECK_EQ(chunk.bytes(), chunk_bytes);
    const MockExecutorOps::MemoryRequest asked = executor.last_memory_request();
    CHECK_EQ(asked.key, std::uint64_t{chunk.resource_id()});
    CHECK_EQ(asked.size, chunk_bytes);
    CHECK_EQ(asked.request, memory.encoded());
    CHECK_EQ(asked.domain, executor.last_opened_domain());
    CHECK_EQ(rig.device().host().live_placements(), 1u);
    CHECK_EQ(rig.device().host().last_placement().size, chunk_bytes);
    std::uint8_t *const host = executor_view(rig, chunk.resource_id());
    CHECK(host != nullptr);
    if (host == nullptr) return;
    chunk.view()[0] = 0x11u;
    chunk.view()[chunk_bytes - 1u] = 0x22u;
    CHECK_EQ(host[0], 0x11u);
    CHECK_EQ(host[chunk_bytes - 1u], 0x22u);
    host[0x1000u] = 0x33u;
    CHECK_EQ(chunk.view()[0x1000u], 0x33u);
    CHECK_STATUS(chunk.release(), Status::ok);
    CHECK_EQ(rig.device().host().live_placements(), 0u);
    CHECK_EQ(executor.live_memory(), 0u);
}

}

TEST(fullstack_host_memory, every_request_class_lands_as_a_placed_host_blob_the_executor_shares)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  Process process(rig);
  CHECK(process.started());
  lands_as_asked(rig, process, upload);
  lands_as_asked(rig, process, HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u));
  lands_as_asked(rig, process, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, 3u));
  lands_as_asked(rig, process, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, 31u));
  CHECK_EQ(rig.device().executor().memory_allocations(), 4u);
  CHECK_EQ(rig.platform().process_mappings(), transport_views); }

TEST(fullstack_host_memory, the_hosts_caching_is_the_caching_of_the_process_view)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  Process process(rig);
  CHECK(process.started());
  const std::uint32_t combined = rig.platform().write_combined_process_mappings();
  HostWindowChunk cached(process.connection(), process.transport());
  CHECK_STATUS(cached.open(chunk_bytes, upload), Status::ok);
  CHECK_EQ(rig.platform().write_combined_process_mappings(), combined);
  rig.device().executor().set_memory_caching(AV_MEMORY_WRITE_COMBINED);
  HostWindowChunk uncached(process.connection(), process.transport());
  CHECK_STATUS(uncached.open(chunk_bytes, upload), Status::ok);
  CHECK_EQ(rig.platform().write_combined_process_mappings(), combined + 1u);
  CHECK_EQ(rig.platform().process_mappings(), transport_views + 2u);
  CHECK_STATUS(uncached.release(), Status::ok);
  CHECK_EQ(rig.platform().write_combined_process_mappings(), combined);
  CHECK_STATUS(cached.release(), Status::ok);
  CHECK_EQ(rig.platform().process_mappings(), transport_views); }

TEST(fullstack_host_memory, a_closed_allocation_leaves_nothing_behind)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  Process process(rig);
  CHECK(process.started());
  MockExecutorOps &executor = rig.device().executor();
  std::uint32_t    resource = 0u;
  { HostAllocation allocation(process.connection(), process.transport());
    CHECK_STATUS(allocation.open(chunk_bytes, upload), Status::ok);
    CHECK(allocation.handle() != 0u);
    CHECK_EQ(executor.live_imports(), 1u);
    resource = HostBlobId::first;
    std::uint64_t key = 0u;
    CHECK(executor.import_of(1u, key));
    CHECK_EQ(key, std::uint64_t{resource});
    const int device_fd = executor.answered_descriptor(executor.last_opened_domain(), resource);
    CHECK(descriptor_open(device_fd));
    CHECK_STATUS(allocation.close(), Status::ok);
    CHECK(!descriptor_open(device_fd));
    CHECK(!allocation.held()); }
  CHECK_EQ(executor.live_imports(), 0u);
  CHECK_EQ(executor.held_memory(), 0u);
  CHECK_EQ(executor.live_memory(), 0u);
  CHECK_EQ(executor.memory_frees(), 1u);
  CHECK_EQ(rig.device().host().live_placements(), 0u);
  CHECK_EQ(rig.platform().process_mappings(), transport_views);
  HostWindowChunk again(process.connection(), process.transport());
  CHECK_STATUS(again.open(chunk_bytes, upload), Status::ok);
  CHECK_EQ(again.resource_id(), resource);
  CHECK_EQ(rig.device().host().last_placement().offset, 0u);
  CHECK_STATUS(again.release(), Status::ok); }

TEST(fullstack_host_memory, an_import_keeps_the_memory_after_its_chunk_is_closed)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  Process process(rig);
  CHECK(process.started());
  MockExecutorOps &executor = rig.device().executor();
  HostWindowChunk  chunk(process.connection(), process.transport());
  BlobImport       import(process.transport());
  CHECK_STATUS(chunk.open(chunk_bytes, upload), Status::ok);
  const std::uint32_t resource = chunk.resource_id();
  chunk.view()[0x20u] = 0x7Eu;
  CHECK_STATUS(import.import(resource), Status::ok);
  CHECK_STATUS(chunk.release(), Status::ok);
  CHECK_EQ(executor.held_memory(), 0u);
  CHECK_EQ(executor.live_memory(), 1u);
  CHECK_EQ(rig.device().host().live_placements(), 0u);
  const std::uint8_t *const kept = executor_view(rig, resource);
  CHECK(kept != nullptr);
  if (kept != nullptr) CHECK_EQ(kept[0x20u], 0x7Eu);
  CHECK_STATUS(import.release(), Status::ok);
  CHECK_EQ(executor.live_memory(), 0u); }

TEST(fullstack_host_memory, a_chunk_the_host_refuses_is_refused_with_nothing_left)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  Process process(rig);
  CHECK(process.started());
  MockExecutorOps &executor = rig.device().executor();
  HostWindowChunk  chunk(process.connection(), process.transport());

  executor.refuse_memory_at(1u);
  CHECK_STATUS(chunk.open(chunk_bytes, upload), Status::not_supported);
  CHECK_EQ(executor.memory_allocations(), 1u);
  CHECK_EQ(chunk.resource_id(), 0u);

  rig.device().host().fail_placement_at(1u);
  CHECK_STATUS(chunk.open(chunk_bytes, upload), Status::not_supported);
  CHECK_EQ(executor.memory_allocations(), 2u);
  CHECK_EQ(executor.memory_frees(), 1u);

  executor.set_memory_caching(0u);
  CHECK_STATUS(chunk.open(chunk_bytes, upload), Status::not_supported);
  CHECK_EQ(executor.memory_frees(), 2u);
  executor.set_memory_caching(AV_MEMORY_WRITE_BACK);

  CHECK_EQ(executor.live_memory(), 0u);
  CHECK_EQ(rig.device().host().live_placements(), 0u);
  CHECK_EQ(rig.platform().process_mappings(), transport_views);
  CHECK_STATUS(chunk.open(chunk_bytes, upload), Status::ok);
  CHECK_EQ(chunk.resource_id(), HostBlobId::first);
  CHECK_EQ(rig.device().host().last_placement().offset, 0u);
  CHECK_STATUS(chunk.release(), Status::ok); }

TEST(fullstack_host_memory, a_context_importing_another_contexts_blob_is_refused)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  Process owner(rig);
  Process other(rig);
  CHECK(owner.started());
  CHECK(other.started());
  MockExecutorOps &executor = rig.device().executor();
  HostWindowChunk  chunk(owner.connection(), owner.transport());
  CHECK_STATUS(chunk.open(chunk_bytes, upload), Status::ok);
  BlobImport stolen(other.transport());
  CHECK_STATUS(stolen.import(chunk.resource_id()), Status::invalid_parameter);
  CHECK_EQ(stolen.host_status(), std::uint32_t{BlobImportAnswer::unknown_resource});
  CHECK_EQ(stolen.memory(), 0u);
  CHECK_EQ(executor.imported(), 0u);
  BlobImport own(owner.transport());
  CHECK_STATUS(own.import(chunk.resource_id()), Status::ok);
  CHECK(own.memory() != 0u);
  CHECK_EQ(executor.imported(), 1u);
  CHECK_STATUS(own.release(), Status::ok);
  CHECK_STATUS(chunk.release(), Status::ok);
  CHECK_EQ(executor.live_memory(), 0u); }
