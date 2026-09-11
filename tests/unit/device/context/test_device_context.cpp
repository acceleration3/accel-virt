/* Binding one context to its ring: the blobs pinned, the ring prepared, the
 * doorbell connected and the ring attached on the worker - all of it, or none. */
#include "core/support/status.hpp"
#include "device/context/device_context.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/control/control_request.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "device/threading/worker.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/ring/guest_ring.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/fence_page.hpp"

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

ContextSetup setup(std::uint32_t doorbell = 3u)
{
    return ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, ring, 12u, fence, doorbell);
}

/* A guest-backed ring the test writes as the guest, and a host fence blob. */
class Fixture {
public:
    Fixture() : blobs_(host_), interrupts_(host_), context_(host_, blobs_, interrupts_, 5u)
    {
        host_.add_guest_ram(ram, 0x2000u);
        GpuRequestBuilder entries(0u);
        entries.u64(ram).u32(0x2000u).u32(0u);
        MockRequest    mock(entries.bytes(), entries.size());
        ControlRequest request(host_, &mock);
        static_cast<void>(blobs_.create_guest(ring, 0x2000u, request, 24u, 1u));
        static_cast<void>(blobs_.create_host(fence, 0x1000u));
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(interrupts_.connect());
        static_cast<void>(context_.start());
    }

    MockHost      &host() { return host_; }
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
  CHECK_EQ(fixture.page().progress(), 2u);
  CHECK_EQ(fixture.host().raised(3u), 1u);
  CHECK_STATUS(fixture.context().bind(setup(), capset()), Status::invalid_state);
  CHECK_EQ(fixture.host().live_doorbells(), 1u); }

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
