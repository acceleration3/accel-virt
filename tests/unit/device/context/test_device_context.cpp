/* Binding one context to its ring: the blobs pinned, the ring prepared, the
 * doorbell connected and the ring attached on the worker - all of it, or none. */
#include "core/support/status.hpp"
#include "device/context/device_context.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "device/control/control_request.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "device/threading/worker.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/recording/recorded_file.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/ring/guest_ring.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_resources.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/reply_header.hpp"

#include <cstdint>
#include <cstring>
#include <sched.h>

using av::BlobTable;
using av::CapsetInfo;
using av::ContextSetup;
using av::ControlRequest;
using av::DeviceContext;
using av::GpuIdentity;
using av::GpuRequestBuilder;
using av::GuestRing;
using av::MockHost;
using av::MockRequest;
using av::ProtocolVersion;
using av::Status;
using av::SubApi;
using av::Worker;

namespace av {

struct DeviceContextProbe {
    static Worker        &worker(DeviceContext &context) { return context.worker_; }
    static FenceReporter &fences(DeviceContext &context) { return context.fences_; }
};

struct WorkerProbe {
    static void fill(Worker &worker) { worker.count_ = Worker::queue_capacity; }
    static void empty(Worker &worker) { worker.count_ = 0u; }
};

}  // namespace av

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint32_t ring = 1u, fence = 2u;
constexpr std::uint8_t  uuid[GpuIdentity::uuid_bytes] = {7};

CapsetInfo capset()
{
    return CapsetInfo(ProtocolVersion(1u, 0u), static_cast<std::uint32_t>(SubApi::vulkan), 64u, 12u, 24u, 0u,
                      GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 3u, 4u, "gpu"));
}

constexpr std::uint32_t reply = 3u;

ContextSetup setup(std::uint32_t doorbell = 3u, std::uint32_t reply_id = 0u)
{
    return ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, ring, 12u, fence, doorbell, reply_id);
}

/* A guest-backed ring the test writes as the guest, and a host fence blob. */
class Fixture {
public:
    Fixture()
        : blobs_(host_), interrupts_(host_), ops_(RVK_EXECUTOR_SUB_API_VULKAN), library_(),
          shares_(host_, library_), recorder_(host_),
          context_(host_, blobs_, interrupts_, library_, shares_, recorder_, 5u)
    {
        static_cast<void>(blobs_.initialize());
        static_cast<void>(shares_.initialize());
        static_cast<void>(library_.accept(&ops_.table()));
        host_.add_guest_ram(ram, 0x2000u);
        GpuRequestBuilder entries(0u);
        entries.u64(ram).u32(0x2000u).u32(0u);
        MockRequest    mock(entries.bytes(), entries.size());
        ControlRequest request(host_, &mock);
        static_cast<void>(blobs_.create_guest(ring, 0x2000u, request, 24u, 1u));
        static_cast<void>(blobs_.create_host(fence, 0x1000u));
        static_cast<void>(blobs_.create_host(reply, 0x1000u));
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(interrupts_.connect());
        static_cast<void>(context_.start());
    }

    MockHost      &host() { return host_; }
    av::WireRecorder &recorder() { return recorder_; }
    av::MockExecutorOps &ops() { return ops_; }
    BlobTable     &blobs() { return blobs_; }
    DeviceContext &context() { return context_; }
    GuestRing      guest() { return GuestRing(host_.guest_ram(ram), 12u); }
    std::uint8_t  *ring_bytes() { return host_.guest_ram(ram); }

    /* The fence page the guest reads, through the pinned blob. */
    av::FencePage page()
    {
        av::BlobWindow window;
        std::uint8_t   bytes[av::FencePage::encoded_size] = {};
        av::FencePage  page;
        if (blobs_.window(fence, window) == Status::ok && window.read(0u, bytes, sizeof bytes) == Status::ok)
            static_cast<void>(page.decode(bytes, sizeof bytes));
        return page;
    }

    /* Whether the worker published `head` before patience ran out. */
    bool published(std::uint64_t head)
    {
        for (std::uint32_t spins = 0u; spins < 50000000u && published_head() != head; spins++) sched_yield();
        return published_head() == head;
    }

private:
    std::uint64_t published_head()
    {
        std::uint64_t value = 0u;
        std::memcpy(&value, host_.guest_ram(ram) + GuestRing::head_offset, sizeof value);
        return value;
    }

    /* Declared before the context, which unpins its blobs as it goes. */
    MockHost            host_;
    BlobTable           blobs_;
    av::FenceInterrupts interrupts_;
    av::MockExecutorOps ops_;
    av::ExecutorLibrary library_;
    av::ExecutorShares  shares_;
    av::WireRecorder    recorder_;
    DeviceContext       context_;
};

}  // namespace

TEST(device_context_bind, a_bound_context_serves_its_ring_when_its_doorbell_rings)
{ Fixture fixture;
  GuestRing guest = fixture.guest();
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::ok);
  CHECK(fixture.context().binding().bound());
  CHECK_EQ(fixture.context().binding().doorbell(), 3u);
  CHECK_EQ(fixture.host().live_doorbells(), 1u);
  CHECK_EQ(fixture.page().progress(), 0u);
  guest.write_batch(1u, 40u);
  guest.write_batch(2u, 0u);
  guest.publish();
  CHECK(fixture.host().ring_doorbell(3u));
  CHECK(fixture.published(guest.tail()));
  for (std::uint32_t spins = 0u; spins < 50000000u && fixture.host().raised(3u) == 0u; spins++) sched_yield();
  CHECK_EQ(fixture.page().completed_fence(), 2u);
  CHECK_EQ(fixture.page().progress(), 3u);   /* two batches and the command in the first */
  CHECK_EQ(fixture.host().raised(3u), 1u);
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::invalid_state);
  CHECK_EQ(fixture.host().live_doorbells(), 1u); }

TEST(device_context_bind, a_host_that_finishes_on_its_own_thread_reaches_the_guest_through_the_fence)
{ Fixture fixture;
  /* A fresh ring header, which is what the ring this binds to needs. */
  GuestRing guest = fixture.guest();
  static_cast<void>(guest.tail());
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::ok);
  /* The executor is made on the worker, when the ring attaches, and the
   * library is given somewhere to call straight after - so the wait is for
   * the second of the two. Waiting for create() alone and then asking about
   * the signal is a race: the worker is between them often enough to see. */
  for (std::uint32_t spins = 0u; spins < 50000000u && fixture.ops().signals_set() == 0u; spins++)
      sched_yield();
  /* signals_through hands the context's landing pad to create(), and a
   * version 5 library takes it. */
  CHECK_EQ(fixture.ops().created(), 1u);
  CHECK(fixture.ops().signal_bound());
  CHECK_EQ(fixture.ops().signals_set(), 1u);

  const std::uint32_t before = fixture.host().raised(3u);
  const std::uint64_t completed = fixture.page().completed_fence();
  /* The host's own thread, which is neither the worker nor the main loop, says
   * a callback fired. */
  CHECK(fixture.ops().fire_signal());
  for (std::uint32_t spins = 0u;
       spins < 50000000u && fixture.host().raised(3u) == before; spins++)
      sched_yield();
  /* The guest is interrupted and told to ask what completed. What it was
   * already promised never moves backwards, and nothing was submitted, so the
   * completed fence is exactly where it was. */
  CHECK_EQ(fixture.host().raised(3u), before + 1u);
  CHECK_EQ(fixture.page().completed_fence(), completed); }

TEST(device_context_bind, a_context_with_a_reply_slot_answers_into_it)
{ Fixture fixture;
  GuestRing guest = fixture.guest();
  CHECK_STATUS(fixture.context().bind(setup(3u, reply), capset()), Status::ok);
  std::uint8_t batch[20] = {};
  const std::uint32_t id = 0x10007u, size_word = 20u | (1u << 24);
  std::memcpy(batch, &id, 4u);
  std::memcpy(batch + 4, &size_word, 4u);
  std::memset(batch + 8, 0x77, 12u);
  guest.write_batch(1u, batch, sizeof batch);
  guest.publish();
  CHECK(fixture.host().ring_doorbell(3u));
  CHECK(fixture.published(guest.tail()));
  av::BlobWindow window;
  CHECK_STATUS(fixture.blobs().window(reply, window), Status::ok);
  std::uint8_t answer[32u + 12u] = {};
  CHECK_STATUS(window.read(0u, answer, sizeof answer), Status::ok);
  av::ReplyHeader header;
  CHECK_STATUS(header.decode(answer, 32u), Status::ok);
  CHECK_EQ(header.fence_id(), 1u);
  CHECK_EQ(header.size(), 12u);
  CHECK_EQ(answer[43], 0x77u); }

TEST(device_context_bind, a_host_that_cannot_stage_the_batches_binds_nothing)
{ Fixture fixture;
  static_cast<void>(fixture.guest());
  fixture.host().fail_allocation_at(1u);
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::no_memory);
  CHECK(!fixture.context().binding().bound());
  CHECK_EQ(fixture.host().live_doorbells(), 0u);
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::ok); }

TEST(device_context, the_worker_releases_the_executor_it_made)
{ Fixture fixture;
  GuestRing guest = fixture.guest();
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::ok);
  guest.write_batch(1u, 8u);
  guest.publish();
  CHECK(fixture.host().ring_doorbell(3u));
  CHECK(fixture.published(guest.tail()));
  CHECK_EQ(fixture.ops().created(), 1u);
  av::DeviceContextProbe::worker(fixture.context()).stop();
  CHECK_EQ(fixture.ops().destroyed(), 1u);
  CHECK_EQ(fixture.ops().misuses(), 0u); }

TEST(device_context_bind, a_ring_without_a_fresh_header_binds_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::invalid_parameter);
  CHECK(!fixture.context().binding().bound());
  CHECK_EQ(fixture.host().live_doorbells(), 0u);
  CHECK_STATUS(fixture.blobs().release(ring), Status::ok);
  Fixture again;
  static_cast<void>(again.guest());
  again.ring_bytes()[16] = 1u;   /* head already moved */
  CHECK_STATUS(again.context().bind(setup(), capset()), Status::invalid_parameter);
  static_cast<void>(again.guest());
  CHECK_STATUS(again.context().bind(setup(), capset()), Status::ok); }

TEST(device_context_bind, a_fence_page_that_cannot_be_prepared_binds_nothing)
{ Fixture fixture;
  static_cast<void>(fixture.guest());
  std::uint8_t        page[av::FencePage::encoded_size] = {};
  void               *pointer = page;
  const std::uint64_t length = sizeof page;
  CHECK_STATUS(av::DeviceContextProbe::fences(fixture.context())
                   .prepare(av::FenceWindow(av::BlobWindow(&pointer, &length, 1u, length)), 1u),
               Status::ok);
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::invalid_parameter);
  CHECK(!fixture.context().binding().bound());
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::ok); }

TEST(device_context_bind, a_worker_that_is_not_running_binds_nothing)
{ Fixture fixture;
  static_cast<void>(fixture.guest());
  av::DeviceContextProbe::worker(fixture.context()).stop();
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::invalid_state);
  CHECK(!fixture.context().binding().bound());
  CHECK_STATUS(fixture.blobs().release(fence), Status::ok); }

TEST(device_context_bind, a_full_work_queue_disconnects_unpins_and_withdraws_the_ring)
{ Fixture fixture;
  static_cast<void>(fixture.guest());
  Worker &worker = av::DeviceContextProbe::worker(fixture.context());
  av::WorkerProbe::fill(worker);
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::no_space);
  av::WorkerProbe::empty(worker);
  CHECK(!fixture.context().binding().bound());
  CHECK_EQ(fixture.host().live_doorbells(), 0u);
  CHECK_STATUS(fixture.context().bind(setup(8u), capset()), Status::ok);
  CHECK_EQ(fixture.context().binding().doorbell(), 8u); }

TEST(device_context, destroying_a_bound_context_disconnects_before_the_worker_goes)
{ Fixture fixture;
  static_cast<void>(fixture.guest());
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::ok);
  /* The mock fails the test if the context's event is destroyed while its
   * doorbell is connected, or a doorbell outlives the context. */
}

/* The setup names the context's resources from its ring, so the chunk three ids
 * past it is the context's to import - and a guest blob there imports through
 * the ring like any command (feature 045). */
TEST(device_context_bind, a_bound_context_imports_the_chunk_after_its_transport_resources)
{ Fixture fixture;
  constexpr std::uint64_t chunk_ram = 0x400000u;
  fixture.host().add_guest_ram(chunk_ram, 0x1000u);
  GpuRequestBuilder entries(0u);
  entries.u64(chunk_ram).u32(0x1000u).u32(0u);
  MockRequest    mock(entries.bytes(), entries.size());
  ControlRequest request(fixture.host(), &mock);
  const std::uint32_t chunk = av::ContextResources(ring).chunk(0u);
  CHECK_STATUS(fixture.blobs().create_guest(chunk, 0x1000u, request, 24u, 1u), Status::ok);
  GuestRing guest = fixture.guest();
  CHECK_STATUS(fixture.context().bind(setup(3u, reply), capset()), Status::ok);
  std::uint8_t batch[8u + av::BlobImportRequest::encoded_size] = {};
  CHECK_STATUS(av::CommandHeader(av::CommandHeader::import_blob_id, sizeof batch, av::CommandHeader::generate_reply)
                   .encode(batch, 8u),
               Status::ok);
  CHECK_STATUS(av::BlobImportRequest(chunk, 0u, 0x1000u, 2u).encode(batch + 8u, av::BlobImportRequest::encoded_size),
               Status::ok);
  guest.write_batch(1u, batch, sizeof batch);
  guest.publish();
  CHECK(fixture.host().ring_doorbell(3u));
  CHECK(fixture.published(guest.tail()));
  av::BlobWindow window;
  CHECK_STATUS(fixture.blobs().window(reply, window), Status::ok);
  std::uint8_t answer[32u + av::BlobImportAnswer::encoded_size] = {};
  CHECK_STATUS(window.read(0u, answer, sizeof answer), Status::ok);
  av::BlobImportAnswer imported;
  CHECK_STATUS(imported.decode(answer + 32u, av::BlobImportAnswer::encoded_size), Status::ok);
  CHECK_EQ(imported.status(), av::BlobImportAnswer::imported);
  CHECK_EQ(imported.device_memory(), 1u | av::MockExecutorOps::handle_bit);
  CHECK_EQ(fixture.ops().live_imports(), 1u);
  CHECK_STATUS(fixture.blobs().release(chunk), Status::invalid_state); }

/* Feature 050: the options a setup carries are what the worker's executor is
 * made with; options the library cannot make bind nothing. */
TEST(device_context_bind, an_opengl_setups_options_reach_the_executor_it_makes)
{ Fixture fixture;
  const CapsetInfo gl(ProtocolVersion(1u, 1u), static_cast<std::uint32_t>(SubApi::opengl), 64u, 12u, 24u, 0u,
                      GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 3u, 4u, "gpu"));
  const av::GlContextOptions options(4u, 5u, av::GlContextOptions::Profile::compatibility,
                                     av::GlContextOptions::robust_access, 0u);
  const ContextSetup with_options(ProtocolVersion(1u, 1u), SubApi::opengl, ring, 12u, fence, 3u, 0u, options);
  static_cast<void>(fixture.guest());   /* writes the fresh ring header */
  CHECK_STATUS(fixture.context().bind(with_options, gl), Status::ok);
  /* Counted once the create has returned, so everything it recorded is there. */
  for (std::uint32_t spins = 0u; spins < 50000000u && fixture.ops().created() == 0u; spins++) sched_yield();
  CHECK_EQ(fixture.ops().option_creates(), 1u);
  const RvkContextOptions asked = fixture.ops().last_options();
  CHECK_EQ(asked.major, 4u);
  CHECK_EQ(asked.minor, 5u);
  CHECK_EQ(asked.profile, RVK_CONTEXT_PROFILE_COMPATIBILITY);
  CHECK_EQ(asked.flags, RVK_CONTEXT_FLAG_ROBUST_ACCESS);
  CHECK_EQ(fixture.ops().last_sub_api(), RVK_EXECUTOR_SUB_API_OPENGL);
  CHECK(fixture.ops().last_share() == nullptr); }

TEST(device_context_bind, options_an_older_library_cannot_make_bind_nothing)
{ MockHost               host;
  av::BlobTable          blobs(host);
  av::FenceInterrupts    interrupts(host);
  av::MockExecutorOps    ops(RVK_EXECUTOR_SUB_API_OPENGL);
  ops.editable().abi_version = RVK_EXECUTOR_ABI_VERSION_HANDLES;
  ops.editable().create_with_options = nullptr;
  av::ExecutorLibrary    old;
  CHECK_STATUS(old.accept(&ops.table()), Status::ok);
  av::ExecutorShares     shares(host, old);
  av::WireRecorder       recorder(host);
  DeviceContext          context(host, blobs, interrupts, old, shares, recorder, 9u);
  const ContextSetup with_options(ProtocolVersion(1u, 1u), SubApi::opengl, ring, 12u, fence, 3u, 0u,
                                  av::GlContextOptions(3u, 3u, av::GlContextOptions::Profile::core, 0u, 0u));
  CHECK_STATUS(context.bind(with_options, capset()), Status::invalid_parameter);
  CHECK(!context.binding().bound());
  CHECK_EQ(ops.created(), 0u);
  CHECK_EQ(host.live_doorbells(), 0u); }

TEST(device_context_bind, records_the_setup_it_bound_and_nothing_for_a_bind_it_refused)
{ Fixture fixture;
  GuestRing guest = fixture.guest();
  static_cast<void>(guest);
  CHECK_STATUS(fixture.recorder().open("setup.rvk", 0u, "setup", av::AtCeiling::stop, av::GpuIdentity(),
                                       av::ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::ok);
  /* A second bind of a bound context is refused and leaves no second record. */
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::invalid_state);
  av::RecordedFile    file(fixture.host(), "setup.rvk");
  av::RecordHeader    record;
  const std::uint8_t *payload = file.expect(av::RecordKind::context_setup, record);
  CHECK(payload != nullptr);
  CHECK_EQ(record.context(), 5u);
  CHECK(record.fence() == 0u);
  if (payload != nullptr) {
      av::ContextSetup decoded;
      CHECK_STATUS(decoded.decode(payload, record.length()), Status::ok);
      CHECK_EQ(decoded.doorbell(), 3u);
      CHECK_EQ(decoded.ring_resource(), ring);
      CHECK(decoded.sub_api() == SubApi::vulkan);
  }
  CHECK_EQ(file.remaining(), 0u);
  fixture.recorder().close(); }
