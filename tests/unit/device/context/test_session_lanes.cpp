#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/context/d3d_ordering.hpp"
#include "device/context/device_context.hpp"
#include "device/control/control_request.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/fence/device_pulse.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/host_blob_table.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/context/guest_blob_maker.hpp"
#include "harness/context/guest_ring.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/memory/host_blob_maker.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/fence_page.hpp"

#include <cstdint>
#include <cstring>
#include <sched.h>
#include <vector>

using av::CapsetInfo;
using av::CommandHeader;
using av::ContextSetup;
using av::ControlRequest;
using av::DeviceContext;
using av::GpuIdentity;
using av::GpuRequestBuilder;
using av::GuestRing;
using av::MockRequest;
using av::ProtocolVersion;
using av::Status;
using av::SubApi;

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint64_t ram_stride = 0x10000u;
constexpr std::uint32_t contexts = 3u;
constexpr std::uint32_t session_id = 5u;
constexpr std::uint8_t  uuid[GpuIdentity::uuid_bytes] = {7};
constexpr std::uint32_t select_id = 0x40003u;
constexpr std::uint32_t command_id = 0x40005u;

CapsetInfo capset()
{
    return CapsetInfo(ProtocolVersion(1u, 0u), static_cast<std::uint32_t>(SubApi::d3d), 64u, 12u, 24u, 0u,
                      GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 3u, 4u, "gpu"));
}

class Commands {
public:
    Commands &add(std::uint32_t id, std::uint32_t value, std::uint8_t flags = 0u)
    {
        static_cast<void>(CommandHeader(id, CommandHeader::encoded_size + 4u, flags).encode(bytes_ + size_, CommandHeader::encoded_size));
        std::memcpy(bytes_ + size_ + CommandHeader::encoded_size, &value, sizeof value);
        size_ += CommandHeader::encoded_size + 4u;
        return *this;
    }
    const std::uint8_t *bytes() const { return bytes_; }
    std::uint32_t       size() const { return size_; }

private:
    std::uint8_t  bytes_[256] = {};
    std::uint32_t size_ = 0u;
};

class Lanes {
public:
    Lanes()
        : blobs_(host_), maker_(host_, blobs_), interrupts_(host_), ops_(AV_EXECUTOR_SUB_API_D3D), library_(),
          shares_(host_, library_), recorder_(host_), ordering_(host_), pulse_(host_), plane_(), host_blobs_(host_),
          host_maker_(host_, host_blobs_)
    {
        static_cast<void>(pulse_.initialize());
        plane_.allocate_through(library_);
        static_cast<void>(host_maker_.initialize(plane_));
        static_cast<void>(ordering_.initialize());
        static_cast<void>(blobs_.initialize());
        static_cast<void>(shares_.initialize());
        static_cast<void>(library_.accept(&ops_.table()));
        static_cast<void>(library_.open_domain(capset().gpu().uuid(), 0u, 0u, 0u, 0u));
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(interrupts_.connect());
        for (std::uint32_t index = 0u; index < contexts; index++) {
            host_.add_guest_ram(base(index), 0x2000u);
            GpuRequestBuilder entries(0u);
            entries.u64(base(index)).u32(0x2000u).u32(0u);
            MockRequest    mock(entries.bytes(), entries.size());
            ControlRequest request(host_, &mock);
            static_cast<void>(blobs_.create_guest(ring(index), 0x2000u, request, 24u, 1u));
            static_cast<void>(maker_.create(fence(index), 0x1000u));
            static_cast<void>(maker_.create(reply(index), 0x1000u));
            rings_.emplace_back(host_.guest_ram(base(index)), 12u);
            static_cast<void>(contexts_[index].construct(host_, blobs_, host_blobs_, interrupts_, library_, shares_,
                                                         recorder_, ordering_, pulse_, session_id + index, "lane"));
            static_cast<void>(contexts_[index].object().start());
        }
    }
    ~Lanes()
    {
        for (std::uint32_t index = contexts; index != 0u; index--) contexts_[index - 1u].destroy();
    }

    Status bind(std::uint32_t index)
    {
        const av::GlContextOptions options = av::GlContextOptions().sharing(index == 0u ? 0u : session_id);
        return context(index).bind(ContextSetup(ProtocolVersion(1u, 0u), SubApi::d3d, ring(index), 12u, fence(index),
                                                doorbell(index), reply(index), options),
                                   capset());
    }
    bool ready(std::uint32_t index)
    {
        for (std::uint32_t spins = 0u; spins < 50000000u && ops_.created() == 0u; spins++) sched_yield();
        for (std::uint32_t spins = 0u; spins < 2000000u && !context(index).running(); spins++) sched_yield();
        return ops_.created() == 1u;
    }
    GuestRing &guest(std::uint32_t index) { return rings_[index]; }
    DeviceContext &context(std::uint32_t index) { return contexts_[index].object(); }
    void           retire(std::uint32_t index) { context(index).retire(); }
    bool           ring(std::uint32_t index, GuestRing &guest)
    {
        guest.publish();
        return host_.ring_doorbell(doorbell(index));
    }
    av::FencePage page(std::uint32_t index)
    {
        av::BlobWindow window;
        std::uint8_t   bytes[av::FencePage::encoded_size] = {};
        av::FencePage  page;
        if (blobs_.window(fence(index), window) == Status::ok && window.read(0u, bytes, sizeof bytes) == Status::ok)
            static_cast<void>(page.decode(bytes, sizeof bytes));
        return page;
    }
    bool completes(std::uint32_t index, std::uint64_t fence_id)
    {
        for (std::uint32_t spins = 0u; spins < 50000000u && page(index).completed_fence() < fence_id; spins++)
            sched_yield();
        return page(index).completed_fence() >= fence_id;
    }
    bool faults(std::uint32_t index)
    {
        for (std::uint32_t spins = 0u; spins < 50000000u && !page(index).faulted(); spins++) sched_yield();
        return page(index).faulted();
    }
    bool executes(std::uint32_t count)
    {
        for (std::uint32_t spins = 0u; spins < 50000000u && ops_.executed() < count; spins++) sched_yield();
        return ops_.executed() >= count;
    }
    bool stops(std::uint32_t index)
    {
        for (std::uint32_t spins = 0u; spins < 50000000u && !context(index).worker().exited(); spins++) sched_yield();
        return context(index).worker().exited();
    }
    std::uint32_t imports(std::uint32_t index, std::uint32_t resource, std::uint64_t fence_id)
    {
        const av::HostMemoryRequest upload(av::HostMemoryRequest::Kind::upload, 0u);
        if (host_maker_.create(resource, session_id + index, upload, 0x1000u) != Status::ok) return ~0u;
        std::uint8_t batch[8u + av::BlobImportRequest::encoded_size] = {};
        static_cast<void>(CommandHeader(CommandHeader::import_blob_id, sizeof batch, CommandHeader::generate_reply)
                              .encode(batch, 8u));
        static_cast<void>(av::BlobImportRequest(resource).encode(batch + 8u, av::BlobImportRequest::encoded_size));
        GuestRing &ring_of = guest(index);
        ring_of.write_batch(fence_id, batch, sizeof batch);
        if (!ring(index, ring_of) || !completes(index, fence_id)) return ~0u;
        av::BlobWindow       window;
        std::uint8_t         answer[32u + av::BlobImportAnswer::encoded_size] = {};
        av::BlobImportAnswer imported;
        if (blobs_.window(reply(index), window) != Status::ok || window.read(0u, answer, sizeof answer) != Status::ok ||
            imported.decode(answer + 32u, av::BlobImportAnswer::encoded_size) != Status::ok)
            return ~0u;
        return imported.status();
    }
    void settle()
    {
        for (std::uint32_t spins = 0u; spins < 200000u; spins++) sched_yield();
    }
    av::MockExecutorOps &ops() { return ops_; }

private:
    static std::uint64_t base(std::uint32_t index) { return ram + index * ram_stride; }
    static std::uint32_t ring(std::uint32_t index) { return 1u + index * 3u; }
    static std::uint32_t fence(std::uint32_t index) { return 2u + index * 3u; }
    static std::uint32_t reply(std::uint32_t index) { return 3u + index * 3u; }
    static std::uint32_t doorbell(std::uint32_t index) { return 3u + index; }

    av::MockHost                      host_;
    av::BlobTable                     blobs_;
    av::GuestBlobMaker                maker_;
    av::FenceInterrupts               interrupts_;
    av::MockExecutorOps               ops_;
    av::ExecutorLibrary               library_;
    av::ExecutorShares                shares_;
    av::WireRecorder                  recorder_;
    av::D3dOrdering                   ordering_;
    av::DevicePulse                   pulse_;
    av::RecordingContextPlane         plane_;
    av::HostBlobTable                 host_blobs_;
    av::HostBlobMaker                 host_maker_;
    av::ObjectStorage<DeviceContext>  contexts_[contexts];
    std::vector<GuestRing>            rings_;
};

}

TEST(session_lanes, lanes_of_one_session_run_on_its_one_executor_and_complete_on_their_own_pages)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_STATUS(lanes.bind(1u), Status::ok);
  CHECK_STATUS(lanes.bind(2u), Status::ok);
  CHECK_EQ(lanes.context(1u).lane_of(), session_id);
  GuestRing &lane = lanes.guest(1u);
  Commands  work;
  work.add(command_id, 1u);
  lane.write_batch(1u, work.bytes(), work.size());
  CHECK(lanes.ring(1u, lane));
  CHECK(lanes.completes(1u, 1u));
  CHECK_EQ(lanes.ops().created(), 1u);
  CHECK_EQ(lanes.page(0u).completed_fence(), 0u);
  CHECK_EQ(lanes.page(2u).completed_fence(), 0u); }

TEST(session_lanes, a_lane_waiting_on_a_fence_another_lane_signals_later_holds_nothing_of_that_lane)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_STATUS(lanes.bind(1u), Status::ok);
  CHECK_STATUS(lanes.bind(2u), Status::ok);
  GuestRing &waiting = lanes.guest(1u);
  GuestRing &signalling = lanes.guest(2u);
  Commands  signal;
  signal.add(command_id, 20u);
  Commands later;
  later.add(command_id, 21u);
  signalling.write_batch(1u, signal.bytes(), signal.size());
  CHECK(lanes.ring(2u, signalling));
  CHECK(lanes.completes(2u, 1u));
  signalling.write_batch(2u, later.bytes(), later.size());
  CHECK(lanes.ring(2u, signalling));
  CHECK(lanes.completes(2u, 2u));
  CHECK_EQ(lanes.page(1u).completed_fence(), 0u);
  Commands resumed;
  resumed.add(command_id, 10u);
  waiting.write_batch(1u, resumed.bytes(), resumed.size());
  CHECK(lanes.ring(1u, waiting));
  CHECK(lanes.completes(1u, 1u));
  CHECK_EQ(lanes.ops().executed(), 3u); }

TEST(session_lanes, a_lane_batch_runs_only_after_the_session_ran_what_it_follows)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_STATUS(lanes.bind(1u), Status::ok);
  GuestRing &session = lanes.guest(0u);
  GuestRing &lane = lanes.guest(1u);
  Commands   recorded;
  recorded.add(select_id, 1u);
  session.write_batch(1u, recorded.bytes(), recorded.size());
  Commands executed;
  executed.add(command_id, 2u);
  lane.write_batch(1u, executed.bytes(), executed.size(), session.tail());
  CHECK(lanes.ring(1u, lane));
  lanes.settle();
  CHECK_EQ(lanes.ops().executed(), 0u);
  CHECK_EQ(lanes.page(1u).completed_fence(), 0u);
  CHECK(lanes.ring(0u, session));
  CHECK(lanes.completes(1u, 1u));
  CHECK(lanes.completes(0u, 1u));
  CHECK_EQ(lanes.ops().executed(), 2u);
  CHECK_EQ(lanes.ops().id_at(0u), select_id);
  CHECK_EQ(lanes.ops().id_at(1u), command_id);
  Commands next;
  next.add(command_id, 3u);
  session.write_batch(2u, next.bytes(), next.size());
  CHECK(lanes.ring(0u, session));
  CHECK(lanes.completes(0u, 2u)); }

TEST(session_lanes, every_lane_waiting_for_the_session_runs_once_the_session_ran_what_they_follow_with_no_time_passing)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_STATUS(lanes.bind(1u), Status::ok);
  CHECK_STATUS(lanes.bind(2u), Status::ok);
  GuestRing &session = lanes.guest(0u);
  Commands   recorded;
  recorded.add(command_id, 1u);
  session.write_batch(1u, recorded.bytes(), recorded.size());
  for (std::uint32_t index = 1u; index < contexts; index++) {
      GuestRing &lane = lanes.guest(index);
      Commands   executed;
      executed.add(command_id, 1u + index);
      lane.write_batch(1u, executed.bytes(), executed.size(), session.tail());
      CHECK(lanes.ring(index, lane));
  }
  lanes.settle();
  CHECK_EQ(lanes.ops().executed(), 0u);
  CHECK(lanes.ring(0u, session));
  CHECK(lanes.completes(0u, 1u));
  CHECK(lanes.completes(1u, 1u));
  CHECK(lanes.completes(2u, 1u));
  CHECK_EQ(lanes.ops().executed(), 3u);
  CHECK_EQ(lanes.ops().id_at(0u), command_id); }

TEST(session_lanes, a_lane_retired_while_it_waits_for_the_session_stops_waiting_without_a_fault)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_STATUS(lanes.bind(1u), Status::ok);
  GuestRing &lane = lanes.guest(1u);
  Commands  executed;
  executed.add(command_id, 2u);
  lane.write_batch(1u, executed.bytes(), executed.size(), 64u);
  CHECK(lanes.ring(1u, lane));
  lanes.settle();
  lanes.retire(1u);
  CHECK(lanes.stops(1u));
  CHECK(!lanes.page(1u).faulted());
  CHECK_EQ(lanes.ops().executed(), 0u); }

TEST(session_lanes, a_lane_and_its_session_each_import_memory_into_their_one_executor_under_ids_that_never_meet)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_STATUS(lanes.bind(1u), Status::ok);
  CHECK_STATUS(lanes.bind(2u), Status::ok);
  CHECK_EQ(lanes.imports(0u, av::HostBlobId::first, 1u), static_cast<std::uint32_t>(av::BlobImportAnswer::imported));
  CHECK_EQ(lanes.imports(1u, av::HostBlobId::first + 1u, 1u), static_cast<std::uint32_t>(av::BlobImportAnswer::imported));
  CHECK_EQ(lanes.imports(2u, av::HostBlobId::first + 2u, 1u), static_cast<std::uint32_t>(av::BlobImportAnswer::imported));
  CHECK_EQ(lanes.imports(1u, av::HostBlobId::first + 3u, 2u), static_cast<std::uint32_t>(av::BlobImportAnswer::imported));
  CHECK_EQ(lanes.ops().live_imports(), 4u); }

TEST(session_lanes, a_worker_tells_its_executor_library_as_its_last_act_that_it_leaves_and_its_joiner_tells_it_once_the_thread_is_gone)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_EQ(lanes.ops().threads_left(), 0u);
  lanes.retire(0u);
  CHECK(lanes.stops(0u));
  CHECK_EQ(lanes.ops().threads_joined(), 0u);
  lanes.context(0u).worker().join();
  CHECK_EQ(lanes.ops().threads_left(), 1u);
  CHECK_EQ(lanes.ops().threads_joined(), 1u);
  lanes.context(0u).worker().join();
  CHECK_EQ(lanes.ops().threads_joined(), 1u); }

TEST(session_lanes, a_ring_that_takes_the_session_from_another_ring_restores_the_selection_it_left)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_STATUS(lanes.bind(1u), Status::ok);
  GuestRing &session = lanes.guest(0u);
  GuestRing &lane = lanes.guest(1u);
  Commands  first;
  first.add(select_id, 1u, CommandHeader::restores_context).add(command_id, 1u);
  session.write_batch(1u, first.bytes(), first.size());
  CHECK(lanes.ring(0u, session));
  CHECK(lanes.completes(0u, 1u));
  Commands own;
  own.add(select_id, 2u, CommandHeader::restores_context).add(command_id, 2u);
  lane.write_batch(1u, own.bytes(), own.size());
  CHECK(lanes.ring(1u, lane));
  CHECK(lanes.completes(1u, 1u));
  Commands again;
  again.add(command_id, 3u);
  session.write_batch(2u, again.bytes(), again.size());
  CHECK(lanes.ring(0u, session));
  CHECK(lanes.completes(0u, 2u));
  CHECK_EQ(lanes.ops().executed(), 6u);
  const std::uint32_t expected[] = {select_id, command_id, select_id, command_id, select_id, command_id};
  for (std::uint32_t index = 0u; index < 6u; index++) CHECK_EQ(lanes.ops().id_at(index), expected[index]); }

TEST(session_lanes, a_lane_whose_session_goes_while_it_waits_for_it_faults_instead_of_waiting_on)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_STATUS(lanes.bind(1u), Status::ok);
  GuestRing &lane = lanes.guest(1u);
  Commands  executed;
  executed.add(command_id, 2u);
  lane.write_batch(1u, executed.bytes(), executed.size(), 64u);
  CHECK(lanes.ring(1u, lane));
  lanes.settle();
  lanes.retire(0u);
  CHECK(lanes.faults(1u));
  CHECK_EQ(lanes.page(1u).fault_code(), static_cast<std::uint32_t>(av::FaultCode::batch_order));
  CHECK_EQ(lanes.ops().executed(), 0u); }

TEST(session_lanes, a_lane_retired_mid_flight_leaves_its_session_running)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_STATUS(lanes.bind(1u), Status::ok);
  GuestRing &lane = lanes.guest(1u);
  Commands  executed;
  executed.add(command_id, 2u);
  lane.write_batch(1u, executed.bytes(), executed.size(), 64u);
  CHECK(lanes.ring(1u, lane));
  lanes.settle();
  lanes.retire(1u);
  GuestRing &session = lanes.guest(0u);
  Commands  next;
  next.add(command_id, 3u);
  session.write_batch(1u, next.bytes(), next.size());
  CHECK(lanes.ring(0u, session));
  CHECK(lanes.completes(0u, 1u));
  CHECK_EQ(lanes.ops().executed(), 1u); }

TEST(session_lanes, a_host_device_lost_under_a_session_faults_its_lanes_too_and_resolves_their_fences)
{ Lanes lanes;
  CHECK_STATUS(lanes.bind(0u), Status::ok);
  CHECK(lanes.ready(0u));
  CHECK_STATUS(lanes.bind(1u), Status::ok);
  CHECK_STATUS(lanes.bind(2u), Status::ok);
  GuestRing &lane = lanes.guest(1u);
  Commands   executed;
  executed.add(command_id, 2u);
  lane.write_batch(1u, executed.bytes(), executed.size());
  CHECK(lanes.ring(1u, lane));
  CHECK(lanes.completes(1u, 1u));
  lanes.ops().lose_host_device();
  GuestRing &session = lanes.guest(0u);
  Commands   more;
  more.add(command_id, 3u);
  session.write_batch(1u, more.bytes(), more.size());
  CHECK(lanes.ring(0u, session));
  lane.write_batch(2u, more.bytes(), more.size());
  CHECK(lanes.ring(1u, lane));
  GuestRing &other = lanes.guest(2u);
  other.write_batch(1u, more.bytes(), more.size());
  CHECK(lanes.ring(2u, other));
  CHECK(lanes.faults(0u));
  CHECK(lanes.faults(1u));
  CHECK(lanes.faults(2u));
  CHECK_EQ(lanes.page(1u).fault_code(), static_cast<std::uint32_t>(av::FaultCode::device_lost)); }
