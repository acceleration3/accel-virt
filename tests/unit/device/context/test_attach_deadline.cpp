#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/context/d3d_ordering.hpp"
#include "device/context/device_context.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/fence/device_pulse.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/host_blob_table.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/context/guest_blob_maker.hpp"
#include "harness/context/guest_ring.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/fence_page.hpp"

#include <cstdint>
#include <cstring>
#include <sched.h>

using av::BlobTable;
using av::CapsetInfo;
using av::ContextSetup;
using av::DeviceContext;
using av::DevicePulse;
using av::FencePage;
using av::FenceReporter;
using av::GpuIdentity;
using av::GuestRing;
using av::MockHost;
using av::ProtocolVersion;
using av::Status;
using av::SubApi;

namespace {

constexpr std::uint8_t  first_gpu[GpuIdentity::uuid_bytes] = {7};
constexpr std::uint8_t  second_gpu[GpuIdentity::uuid_bytes] = {9};
constexpr std::uint8_t  order = 12u;
constexpr std::uint32_t spins = 50000000u;

CapsetInfo capset(SubApi api, const std::uint8_t *uuid)
{
    return CapsetInfo(ProtocolVersion(1u, 0u), static_cast<std::uint32_t>(api), 64u, 12u, 24u, 0u,
                      GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 3u, 4u, "gpu"));
}

struct Lane {
    std::uint32_t ring;
    std::uint32_t fence;
    std::uint32_t doorbell;
    std::uint8_t *pages;
};

class Fixture {
public:
    Fixture()
        : blobs_(host_), maker_(host_, blobs_), interrupts_(host_),
          ops_(AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENGL | AV_EXECUTOR_SUB_API_D3D), library_(),
          shares_(host_, library_), recorder_(host_), ordering_(host_), first_pulse_(host_), second_pulse_(host_),
          plane_(), host_blobs_(host_),
          gl_(host_, blobs_, host_blobs_, interrupts_, library_, shares_, recorder_, ordering_, first_pulse_, 5u, "gl"),
          d3d_(host_, blobs_, host_blobs_, interrupts_, library_, shares_, recorder_, ordering_, first_pulse_, 6u, "d3d"),
          vulkan_(host_, blobs_, host_blobs_, interrupts_, library_, shares_, recorder_, ordering_, second_pulse_, 7u,
                  "vulkan"),
          lanes_{}, rings_{}
    {
        static_cast<void>(first_pulse_.initialize());
        static_cast<void>(second_pulse_.initialize());
        plane_.allocate_through(library_);
        static_cast<void>(ordering_.initialize());
        static_cast<void>(blobs_.initialize());
        static_cast<void>(shares_.initialize());
        static_cast<void>(library_.accept(&ops_.table()));
        static_cast<void>(library_.open_domain(first_gpu, 0u, 0u, 0u, 0u));
        static_cast<void>(library_.open_domain(second_gpu, 0u, 0u, 0u, 0u));
        for (std::uint32_t index = 0u; index < 3u; index++) {
            Lane &lane = lanes_[index];
            lane = Lane{10u * index + 1u, 10u * index + 2u, 3u + index, nullptr};
            CHECK_STATUS(maker_.create(lane.ring, 0x2000u), Status::ok);
            lane.pages = maker_.last_pages();
            CHECK_STATUS(maker_.create(lane.fence, 0x1000u), Status::ok);
        }
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(interrupts_.connect());
        CHECK_STATUS(gl_.start(), Status::ok);
        CHECK_STATUS(d3d_.start(), Status::ok);
        CHECK_STATUS(vulkan_.start(), Status::ok);
    }
    ~Fixture() { ops_.release_creates(); }

    MockHost            &host() { return host_; }
    av::MockExecutorOps &ops() { return ops_; }
    DevicePulse         &first_pulse() { return first_pulse_; }
    DevicePulse         &second_pulse() { return second_pulse_; }

    Status bind(DeviceContext &context, std::uint32_t lane, SubApi api, const std::uint8_t *uuid)
    {
        CHECK_STATUS(rings_[lane].construct(lanes_[lane].pages, order), Status::ok);
        const ContextSetup setup(ProtocolVersion(1u, 0u), api, lanes_[lane].ring, order, lanes_[lane].fence,
                                 lanes_[lane].doorbell, 0u);
        return context.bind(setup, capset(api, uuid));
    }

    DeviceContext &gl() { return gl_; }
    DeviceContext &d3d() { return d3d_; }
    DeviceContext &vulkan() { return vulkan_; }

    bool runs(std::uint32_t lane, std::uint64_t fence)
    {
        GuestRing &guest = rings_[lane].object();
        guest.write_batch(fence, 8u);
        guest.publish();
        if (!host_.ring_doorbell(lanes_[lane].doorbell)) return false;
        for (std::uint32_t spin = 0u; spin < spins && page(lane).completed_fence() != fence; spin++) sched_yield();
        return page(lane).completed_fence() == fence;
    }

    FencePage page(std::uint32_t lane)
    {
        av::BlobWindow window;
        std::uint8_t   bytes[FencePage::encoded_size] = {};
        FencePage      fences;
        if (blobs_.window(lanes_[lane].fence, window) == Status::ok && window.read(0u, bytes, sizeof bytes) == Status::ok)
            static_cast<void>(fences.decode(bytes, sizeof bytes));
        return fences;
    }

    std::uint32_t doorbell(std::uint32_t lane) const { return lanes_[lane].doorbell; }

private:
    MockHost                  host_;
    BlobTable                 blobs_;
    av::GuestBlobMaker        maker_;
    av::FenceInterrupts       interrupts_;
    av::MockExecutorOps       ops_;
    av::ExecutorLibrary       library_;
    av::ExecutorShares        shares_;
    av::WireRecorder          recorder_;
    av::D3dOrdering           ordering_;
    DevicePulse               first_pulse_;
    DevicePulse               second_pulse_;
    av::RecordingContextPlane plane_;
    av::HostBlobTable         host_blobs_;
    DeviceContext             gl_;
    DeviceContext             d3d_;
    DeviceContext             vulkan_;
    Lane                      lanes_[3];
    av::ObjectStorage<GuestRing> rings_[3];
};

constexpr std::uint32_t gl_lane = 0u, d3d_lane = 1u, vulkan_lane = 2u;

}

TEST(attach_deadline, an_attach_stuck_in_the_host_driver_is_refused_at_its_deadline_and_other_contexts_run_on)
{ Fixture fixture;
  fixture.ops().hold_creates_of(AV_EXECUTOR_SUB_API_OPENGL);
  CHECK_STATUS(fixture.bind(fixture.gl(), gl_lane, SubApi::opengl, first_gpu), Status::ok);
  for (std::uint32_t spin = 0u; spin < spins && fixture.ops().creates_waiting() == 0u; spin++) sched_yield();
  CHECK_EQ(fixture.ops().creates_waiting(), 1u);
  CHECK_STATUS(fixture.bind(fixture.d3d(), d3d_lane, SubApi::d3d, first_gpu), Status::ok);
  CHECK_STATUS(fixture.bind(fixture.vulkan(), vulkan_lane, SubApi::vulkan, second_gpu), Status::ok);
  CHECK(fixture.runs(d3d_lane, 1u));
  CHECK(fixture.runs(vulkan_lane, 1u));
  CHECK(!fixture.page(gl_lane).faulted());
  CHECK_EQ(fixture.first_pulse().refused_attaches(), 0u);

  const std::uint32_t raised = fixture.host().raised(fixture.doorbell(gl_lane));
  fixture.host().set_clock(fixture.host().monotonic_nanoseconds() + FenceReporter::attach_limit_ns + 1u);
  for (std::uint32_t spin = 0u; spin < spins && !fixture.page(gl_lane).faulted(); spin++) sched_yield();
  CHECK(fixture.page(gl_lane).faulted());
  CHECK_EQ(fixture.page(gl_lane).fault_code(), static_cast<std::uint32_t>(av::FaultCode::no_executor));
  for (std::uint32_t spin = 0u; spin < spins && fixture.host().raised(fixture.doorbell(gl_lane)) == raised; spin++)
      sched_yield();
  CHECK_EQ(fixture.host().raised(fixture.doorbell(gl_lane)), raised + 1u);
  for (std::uint32_t spin = 0u; spin < spins && fixture.first_pulse().refused_attaches() == 0u; spin++) sched_yield();
  CHECK_EQ(fixture.first_pulse().refused_attaches(), 1u);
  CHECK_EQ(fixture.second_pulse().refused_attaches(), 0u);
  CHECK(std::strstr(fixture.host().last_log(), "context 5 was refused") != nullptr);
  CHECK_EQ(fixture.ops().creates_waiting(), 1u);

  CHECK(fixture.runs(d3d_lane, 2u));
  CHECK(fixture.runs(vulkan_lane, 2u));

  const std::uint32_t destroyed = fixture.ops().destroyed();
  fixture.ops().release_creates();
  for (std::uint32_t spin = 0u; spin < spins && fixture.ops().destroyed() == destroyed; spin++) sched_yield();
  CHECK_EQ(fixture.ops().destroyed(), destroyed + 1u);
  CHECK_EQ(fixture.ops().live(), 2u);
  CHECK_EQ(fixture.page(gl_lane).completed_fence(), 0u);
  CHECK_EQ(fixture.host().raised(fixture.doorbell(gl_lane)), raised + 1u);
  CHECK_EQ(fixture.first_pulse().refused_attaches(), 1u);
  CHECK(fixture.runs(d3d_lane, 3u)); }

TEST(attach_deadline, an_attach_that_comes_up_in_time_is_never_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.bind(fixture.gl(), gl_lane, SubApi::opengl, first_gpu), Status::ok);
  CHECK(fixture.runs(gl_lane, 1u));
  fixture.host().set_clock(fixture.host().monotonic_nanoseconds() + FenceReporter::attach_limit_ns + 1u);
  bool working = false;
  CHECK(fixture.first_pulse().tick(working));
  CHECK(!fixture.page(gl_lane).faulted());
  CHECK_EQ(fixture.first_pulse().refused_attaches(), 0u);
  CHECK(fixture.runs(gl_lane, 2u)); }
