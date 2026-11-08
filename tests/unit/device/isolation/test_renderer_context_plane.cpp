#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "device/context/local_context_plane.hpp"
#include "device/control/control_request.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/isolation/renderer_context_plane.hpp"
#include "device/memory/blob_table.hpp"
#include "device/renderer/renderer_host_blobs.hpp"
#include "device/renderer/renderer_service.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/surface_pair.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/loopback_renderer_peer.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/scripted_renderer_peer.hpp"
#include "harness/context/guest_ring.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/renderer/allocated_memory_message.hpp"
#include "proto/renderer/exported_image_message.hpp"
#include "proto/renderer/renderer_message_header.hpp"
#include "proto/resources/surface_point.hpp"
#include "proto/submission/fence_page.hpp"

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>

using av::MockExecutorOps;
using av::MockHost;
using av::RendererContextPlane;
using av::Status;

namespace av {

struct RendererServiceProbe {
    static BlobTable         &blobs(RendererService &service) { return service.blobs_; }
    static RendererHostBlobs &owners(RendererService &service) { return service.owners_; }
    static LocalContextPlane &plane(RendererService &service) { return service.plane_; }
};

struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_capacity(GrowableArray<T> &array, std::uint32_t capacity) { array.capacity_ = capacity; }
};

struct RemoteContextProbe {
    static void unbind_binding(RemoteContext &context) { context.binding_.unbind(); }
    static void set_lost(RemoteContext &context, bool lost) { context.lost_ = lost; }
};

struct RendererContextPlaneProbe {
    static void count_losses(RendererContextPlane &plane, std::uint32_t count) { plane.loss_count_ = count; }
    static void unconfigure(RendererContextPlane &plane) { plane.configured_ = false; }
    static void corrupt_mirrored(RendererContextPlane &plane, std::uint32_t capacity)
    {
        GrowableArrayProbe::corrupt_capacity(plane.mirrored_, capacity);
    }
    static void strand_context(RendererContextPlane &plane, std::uint32_t id)
    {
        RemoteContextProbe::unbind_binding(plane.slot_of(id)->object());
    }
    static void lose_context(RendererContextPlane &plane, std::uint32_t id)
    {
        RemoteContextProbe::set_lost(plane.slot_of(id)->object(), true);
    }
};

}

using av::RendererContextPlaneProbe;
using av::RendererServiceProbe;

namespace {

constexpr std::uint32_t both = AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENGL;
constexpr char          renderer_path[] = "/usr/libexec/av-renderer";
constexpr char          executor_path[] = "/opt/av/libav_executor.so";
constexpr std::uint8_t  uuid[av::GpuIdentity::uuid_bytes] = {0x42u, 7u};
constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint64_t ram_size = 0x10000u;
constexpr std::uint32_t ring = 1u, fence = 2u, other_ring = 3u, other_fence = 4u;
constexpr std::uint64_t fence_offset = 0x2000u, other_ring_offset = 0x4000u, other_fence_offset = 0x6000u;
constexpr std::uint32_t lines = 64u;

struct Published {
    bool             found;
    AvExportedImage image;
};
Published published = {};

std::uint32_t export_published(void *, std::uint32_t, std::uint64_t key, AvExportedImage *image)
{
    if (!published.found || key != 0x77u) return AV_UNKNOWN;
    *image = published.image;
    return AV_EXECUTED;
}

int fresh_descriptor()
{
    int ends[2] = {-1, -1};
    if (::pipe2(ends, O_CLOEXEC) != 0) return -1;
    ::close(ends[1]);
    return ends[0];
}

av::CapsetInfo capset()
{
    return av::CapsetInfo(av::ProtocolVersion(av::ProtocolVersion::current_major, av::ProtocolVersion::current_minor),
                          both, 64u, 12u, 24u, 0u,
                          av::GpuIdentity(0x1002u, 0x7550u, av::GpuIdentity::discrete_gpu, uuid, 1u, 1u, "mock gpu"));
}

av::ContextSetup setup(std::uint32_t doorbell, av::SubApi api = av::SubApi::vulkan, std::uint32_t ring_id = ring,
                       std::uint32_t fence_id = fence)
{
    return av::ContextSetup(av::ProtocolVersion(av::ProtocolVersion::current_major, 0u), api, ring_id, 12u, fence_id, doorbell, 0u);
}

class Fixture {
public:
    explicit Fixture(bool shared_ram = true)
        : ops_(both), peer_(qemu_, ops_.table()), blobs_(qemu_), interrupts_(qemu_), plane_(qemu_, blobs_, interrupts_)
    {
        qemu_.add_guest_ram(ram, ram_size);
        if (shared_ram) qemu_.add_guest_region(0u, qemu_.guest_ram(ram), ram_size);
        qemu_.set_hostmem_size(0x10000u);
        qemu_.connect_renderer(peer_);
        static_cast<void>(blobs_.initialize());
        static_cast<void>(interrupts_.initialize());
    }
    ~Fixture()
    {
        plane_.destroy_all();
        blobs_.release_all();
    }

    MockHost                 &qemu() { return qemu_; }
    MockExecutorOps          &ops() { return ops_; }
    av::LoopbackRendererPeer &peer() { return peer_; }
    av::BlobTable            &blobs() { return blobs_; }
    RendererContextPlane     &plane() { return plane_; }
    av::BlobTable            &renderer_blobs() { return RendererServiceProbe::blobs(peer_.service()); }
    av::LocalContextPlane    &renderer_plane() { return RendererServiceProbe::plane(peer_.service()); }
    av::RendererHostBlobs    &renderer_owners() { return RendererServiceProbe::owners(peer_.service()); }

    Status start()
    {
        const Status configured = plane_.configure(renderer_path, executor_path);
        if (configured != Status::ok) return configured;
        const Status initialized = plane_.initialize();
        if (initialized != Status::ok) return initialized;
        return plane_.start(uuid, capset());
    }
    Status guest_blob(std::uint32_t id, std::uint64_t offset, std::uint64_t size)
    {
        av::GpuRequestBuilder entries(0u);
        entries.u64(ram + offset).u32(static_cast<std::uint32_t>(size)).u32(0u);
        av::MockRequest    mock(entries.bytes(), entries.size());
        av::ControlRequest request(qemu_, &mock);
        const Status       made = blobs_.create_guest(id, size, request, 24u, 1u);
        if (made == Status::ok) plane_.blob_created(id);
        return made;
    }
    void release(std::uint32_t id)
    {
        plane_.blob_released(id);
        static_cast<void>(blobs_.retire(id));
    }
    void rings()
    {
        av::GuestRing(qemu_.guest_ram(ram), 12u);
        av::GuestRing(qemu_.guest_ram(ram + other_ring_offset), 12u);
        CHECK_STATUS(guest_blob(ring, 0u, 0x2000u), Status::ok);
        CHECK_STATUS(guest_blob(fence, fence_offset, 0x1000u), Status::ok);
        CHECK_STATUS(guest_blob(other_ring, other_ring_offset, 0x2000u), Status::ok);
        CHECK_STATUS(guest_blob(other_fence, other_fence_offset, 0x1000u), Status::ok);
    }
    void connect_interrupts()
    {
        CHECK_STATUS(interrupts_.connect(), Status::ok);
        plane_.interrupts_changed(true);
    }
    av::FencePage page(std::uint64_t offset = fence_offset)
    {
        av::FencePage read;
        static_cast<void>(read.decode(qemu_.guest_ram(ram + offset), av::FencePage::encoded_size));
        return read;
    }
    bool raised_in_renderer(std::uint32_t line)
    {
        for (std::uint32_t spins = 0u; spins < 50000000u && peer_.host().raised(line) == 0u; spins++) sched_yield();
        return peer_.host().raised(line) != 0u;
    }
    void wait(std::uint64_t nanoseconds)
    {
        qemu_.advance_clock_per_read(nanoseconds);
        static_cast<void>(qemu_.monotonic_nanoseconds());
        qemu_.advance_clock_per_read(0u);
    }

private:
    MockHost                 qemu_;
    MockExecutorOps          ops_;
    av::LoopbackRendererPeer peer_;
    av::BlobTable            blobs_;
    av::FenceInterrupts      interrupts_;
    RendererContextPlane     plane_;
};

}

TEST(renderer_context_plane_configure, refuses_a_missing_empty_or_too_long_path)
{ Fixture fixture;
  RendererContextPlane &plane = fixture.plane();
  CHECK_STATUS(plane.initialize(), Status::invalid_state);
  CHECK_STATUS(plane.configure(nullptr, executor_path), Status::invalid_parameter);
  CHECK_STATUS(plane.configure(renderer_path, nullptr), Status::invalid_parameter);
  CHECK_STATUS(plane.configure("", executor_path), Status::invalid_parameter);
  CHECK_STATUS(plane.configure(renderer_path, ""), Status::invalid_parameter);
  static char longest[RendererContextPlane::max_path + 2u];
  std::memset(longest, 'a', RendererContextPlane::max_path + 1u);
  longest[RendererContextPlane::max_path + 1u] = '\0';
  CHECK_STATUS(plane.configure(longest, executor_path), Status::invalid_parameter);
  CHECK_STATUS(plane.configure(renderer_path, longest), Status::invalid_parameter);
  CHECK_STATUS(plane.initialize(), Status::invalid_state);
  longest[RendererContextPlane::max_path] = '\0';
  CHECK_STATUS(plane.configure(longest, longest), Status::ok);
  CHECK_STATUS(plane.initialize(), Status::ok);
  CHECK_STATUS(plane.configure("", executor_path), Status::invalid_parameter);
  CHECK_STATUS(plane.initialize(), Status::invalid_state);
  CHECK(plane.invariants_hold()); }

TEST(renderer_context_plane_accept, takes_no_executor_table_of_its_own)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().accept(nullptr), Status::ok);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::invalid_parameter); }

TEST(renderer_context_plane_start, starts_greets_and_hands_over_guest_memory)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(fixture.plane().alive());
  CHECK_EQ(fixture.plane().epoch(), 1u);
  CHECK_EQ(fixture.qemu().renderer_starts(), 1u);
  CHECK(std::strcmp(fixture.qemu().started_renderer(), renderer_path) == 0);
  CHECK(fixture.peer().service().ready());
  CHECK(std::strcmp(fixture.peer().resources().executor_path(), executor_path) == 0);
  CHECK_EQ(fixture.peer().memory().count(), 1u);
  CHECK_EQ(fixture.qemu().live_descriptors(), 0u);
  CHECK_EQ(fixture.plane().sub_apis(), both);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_start, refuses_before_configure_or_without_a_gpu)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().start(uuid, capset()), Status::invalid_state);
  CHECK_STATUS(fixture.plane().configure(renderer_path, executor_path), Status::ok);
  CHECK_STATUS(fixture.plane().start(nullptr, capset()), Status::invalid_state);
  CHECK_EQ(fixture.qemu().renderer_starts(), 0u);
  CHECK(!fixture.plane().alive()); }

TEST(renderer_context_plane_start, a_renderer_that_does_not_start_answer_or_get_memory_is_not_alive)
{ { Fixture fixture;
    fixture.qemu().fail_renderer_start_at(1u);
    CHECK_STATUS(fixture.start(), Status::not_found);
    CHECK(!fixture.plane().alive());
    CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
    CHECK_EQ(fixture.plane().epoch(), 0u); }
  { Fixture fixture;
    fixture.peer().fall_silent();
    CHECK_STATUS(fixture.start(), Status::invalid_state);
    CHECK(!fixture.plane().alive());
    CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
    CHECK(!fixture.qemu().renderer_running());
    CHECK_EQ(fixture.plane().sub_apis(), 0u); }
  Fixture fixture(false);
  static std::uint8_t rom[0x1000];
  fixture.qemu().add_guest_region(1u, nullptr, sizeof rom);
  CHECK_STATUS(fixture.start(), Status::not_supported);
  CHECK_EQ(fixture.peer().memory().count(), 0u);
  CHECK(!fixture.plane().alive());
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane, a_context_is_created_bound_and_run_in_the_renderer)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.connect_interrupts();
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(0u, "test"), Status::invalid_parameter);
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::conflict);
  CHECK(fixture.plane().holds(3u));
  CHECK(!fixture.plane().holds(4u));
  CHECK_EQ(fixture.plane().live(), 1u);
  CHECK(fixture.renderer_plane().holds(3u));
  CHECK_STATUS(fixture.plane().bind(4u, setup(5u), capset()), Status::invalid_parameter);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok);
  CHECK(fixture.plane().holds_doorbell(5u));
  CHECK(!fixture.plane().holds_doorbell(6u));
  CHECK(fixture.renderer_plane().holds_doorbell(5u));
  CHECK_EQ(fixture.qemu().live_doorbells(), 1u);
  CHECK(fixture.plane().invariants_hold());
  av::GuestRing guest(fixture.qemu().guest_ram(ram), 12u);
  guest.write_batch(1u, 8u);
  guest.publish();
  CHECK(fixture.peer().host().ring_doorbell(5u));
  CHECK(fixture.raised_in_renderer(5u));
  CHECK_EQ(fixture.page().completed_fence(), 1u);
  CHECK(!fixture.page().faulted());
  CHECK_EQ(fixture.ops().executed(), 1u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u); }

TEST(renderer_context_plane_bind, a_guests_refused_bind_ends_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u, av::SubApi::vulkan, 77u), capset()), Status::not_found);
  const av::ContextSetup too_many(av::ProtocolVersion(av::ProtocolVersion::current_major, 0u), av::SubApi::vulkan, ring, 12u, fence, 64u, 0u);
  CHECK_STATUS(fixture.plane().bind(3u, too_many, capset()), Status::not_supported);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok);
  CHECK_STATUS(fixture.plane().bind(3u, setup(6u), capset()), Status::invalid_state);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  CHECK_EQ(fixture.plane().epoch(), 1u);
  CHECK(fixture.plane().alive());
  CHECK(fixture.plane().holds_doorbell(5u));
  CHECK(!fixture.plane().holds_doorbell(6u));
  CHECK_EQ(fixture.qemu().live_doorbells(), 1u); }

TEST(renderer_context_plane_bind, the_renderers_refusal_is_the_answer_and_unbinds_qemus_side)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  fixture.peer().host().fail_allocation_at(1u);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::no_memory);
  CHECK(!fixture.plane().holds_doorbell(5u));
  CHECK_EQ(fixture.qemu().live_doorbells(), 0u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok); }

TEST(renderer_context_plane_bind, a_silent_renderer_at_bind_is_given_up_on)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  fixture.peer().fall_silent();
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::invalid_state);
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK_EQ(fixture.qemu().renderer_starts(), 2u);
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK(fixture.plane().alive());
  CHECK(!fixture.plane().holds_doorbell(5u));
  CHECK_EQ(fixture.qemu().live_doorbells(), 0u);
  CHECK(std::strstr(fixture.qemu().last_log(), "did not answer") != nullptr);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_create, a_silent_renderer_or_none_makes_no_context)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::invalid_state);
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.peer().fall_silent();
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::invalid_state);
  CHECK(!fixture.plane().holds(3u));
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  CHECK(fixture.renderer_plane().holds(3u)); }

TEST(renderer_context_plane_create, the_renderers_refusal_leaves_no_slot)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.peer().host().fail_thread_start_at(1u);
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::no_memory);
  CHECK(!fixture.plane().holds(3u));
  CHECK_EQ(fixture.plane().live(), 0u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u); }

TEST(renderer_context_plane_create, a_full_plane_has_no_space)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  for (std::uint32_t id = 1u; id <= RendererContextPlane::max_contexts; id++)
      CHECK_STATUS(fixture.plane().create(id, "test"), Status::ok);
  const std::uint32_t sent = fixture.peer().delivered();
  CHECK_STATUS(fixture.plane().create(RendererContextPlane::max_contexts + 1u, "test"), Status::no_space);
  CHECK_EQ(fixture.peer().delivered(), sent);
  CHECK_EQ(fixture.plane().live(), RendererContextPlane::max_contexts); }

TEST(renderer_context_plane_destroy, tells_the_renderer_unless_the_context_was_lost)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.rings();
  CHECK_STATUS(fixture.plane().destroy(3u), Status::not_found);
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().create(4u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok);
  CHECK_STATUS(fixture.plane().destroy(3u), Status::ok);
  CHECK(!fixture.plane().holds(3u));
  CHECK(!fixture.renderer_plane().holds(3u));
  CHECK_EQ(fixture.qemu().live_doorbells(), 0u);
  fixture.peer().fall_silent();
  CHECK_STATUS(fixture.plane().destroy(4u), Status::ok);
  CHECK(!fixture.plane().holds(4u));
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK_STATUS(fixture.plane().create(6u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().bind(6u, setup(7u), capset()), Status::ok);
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  const std::uint32_t sent = fixture.peer().delivered();
  CHECK_STATUS(fixture.plane().destroy(6u), Status::ok);
  CHECK_EQ(fixture.peer().delivered(), sent);
  CHECK_EQ(fixture.plane().live(), 0u); }

TEST(renderer_context_plane_can_share, only_a_bound_living_opengl_context)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().create(4u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().create(8u, "test"), Status::ok);
  CHECK(!fixture.plane().can_share(8u, 3u));
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u, av::SubApi::opengl), capset()), Status::ok);
  CHECK_STATUS(fixture.plane().bind(4u, setup(6u, av::SubApi::vulkan, other_ring, other_fence), capset()), Status::ok);
  CHECK(fixture.plane().can_share(8u, 3u));
  CHECK(!fixture.plane().can_share(3u, 3u));
  CHECK(!fixture.plane().can_share(8u, 4u));
  CHECK(!fixture.plane().can_share(8u, 9u));
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  CHECK(!fixture.plane().can_share(8u, 3u)); }

TEST(renderer_context_plane_renderer_lost, reports_every_bound_context_lost_and_replays_everything)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.connect_interrupts();
  fixture.rings();
  CHECK_STATUS(fixture.guest_blob(9u, 0x8000u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().create(4u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().create(8u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok);
  CHECK_STATUS(fixture.plane().bind(4u, setup(6u, av::SubApi::vulkan, other_ring, other_fence), capset()), Status::ok);
  av::GuestRing guest(fixture.qemu().guest_ram(ram), 12u);
  guest.write_batch(1u, 8u);
  guest.publish();
  CHECK(fixture.peer().host().ring_doorbell(5u));
  CHECK(fixture.raised_in_renderer(5u));
  CHECK_EQ(fixture.page().completed_fence(), 1u);
  const std::uint64_t progress = fixture.page().progress();

  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  const av::FencePage first = fixture.page();
  CHECK(first.faulted());
  CHECK_EQ(first.fault_code(), 7u);
  CHECK_EQ(first.completed_fence(), 1u);
  CHECK_EQ(first.progress(), progress + 1u);
  const av::FencePage second = fixture.page(other_fence_offset);
  CHECK(second.faulted());
  CHECK_EQ(second.completed_fence(), 0u);
  CHECK_EQ(second.progress(), 1u);
  CHECK_EQ(fixture.qemu().raised(5u), 1u);
  CHECK_EQ(fixture.qemu().raised(6u), 1u);
  CHECK_EQ(fixture.qemu().live_doorbells(), 0u);
  CHECK(fixture.plane().holds_doorbell(5u));
  CHECK_EQ(fixture.plane().live(), 3u);

  CHECK(fixture.plane().alive());
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK_EQ(fixture.qemu().renderer_starts(), 2u);
  CHECK_EQ(fixture.peer().generation(), 2u);
  CHECK(fixture.peer().service().ready());
  CHECK_EQ(fixture.peer().memory().count(), 1u);
  CHECK(fixture.renderer_blobs().contains(ring));
  CHECK(fixture.renderer_blobs().contains(fence));
  CHECK(fixture.renderer_blobs().contains(other_ring));
  CHECK(fixture.renderer_blobs().contains(other_fence));
  CHECK(fixture.renderer_blobs().contains(9u));
  CHECK_EQ(fixture.renderer_blobs().live(), 5u);
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), lines);
  CHECK(fixture.peer().host().fence_interrupts_connected());
  CHECK_EQ(fixture.renderer_plane().live(), 0u);
  CHECK_EQ(fixture.qemu().live_descriptors(), 0u);
  CHECK(fixture.plane().invariants_hold());
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::invalid_state);
  CHECK_STATUS(fixture.plane().destroy(3u), Status::ok);
  CHECK(!fixture.plane().holds_doorbell(5u));
  av::GuestRing(fixture.qemu().guest_ram(ram), 12u);
  CHECK_STATUS(fixture.plane().create(10u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().bind(10u, setup(5u), capset()), Status::ok);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  fixture.qemu().fail_renderer_start_at(1u);
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  CHECK(!fixture.plane().alive());
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK_EQ(fixture.qemu().raised(5u), 2u);
  fixture.plane().renderer_lost();
  CHECK_EQ(fixture.qemu().raised(5u), 2u);
  CHECK_STATUS(fixture.plane().create(11u, "test"), Status::invalid_state);
  CHECK_STATUS(fixture.plane().bind(10u, setup(5u), capset()), Status::invalid_state); }

TEST(renderer_context_plane_renderer_lost, stops_restarting_a_renderer_that_keeps_dying)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  for (std::uint32_t loss = 0u; loss < RendererContextPlane::max_losses; loss++) {
      fixture.qemu().renderer_gone();
      fixture.plane().renderer_lost();
      CHECK(fixture.plane().alive());
  }
  CHECK_EQ(fixture.qemu().renderer_starts(), RendererContextPlane::max_losses + 1u);
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  CHECK(!fixture.plane().alive());
  CHECK_EQ(fixture.qemu().renderer_starts(), RendererContextPlane::max_losses + 1u);
  CHECK(std::strstr(fixture.qemu().last_log(), "keeps dying") != nullptr);
  CHECK(fixture.plane().invariants_hold());
  fixture.wait(RendererContextPlane::loss_window_ns);
  fixture.plane().renderer_lost();
  CHECK(!fixture.plane().alive());
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::invalid_state); }

TEST(renderer_context_plane_renderer_lost, a_death_a_whole_window_after_the_others_is_restarted)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  for (std::uint32_t loss = 0u; loss < RendererContextPlane::max_losses; loss++) {
      fixture.qemu().renderer_gone();
      fixture.plane().renderer_lost();
  }
  fixture.wait(RendererContextPlane::loss_window_ns);
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  CHECK(fixture.plane().alive());
  CHECK_EQ(fixture.qemu().renderer_starts(), RendererContextPlane::max_losses + 2u);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_renderer_lost, a_death_a_nanosecond_inside_the_window_still_counts)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  for (std::uint32_t loss = 0u; loss < RendererContextPlane::max_losses; loss++) {
      fixture.qemu().renderer_gone();
      fixture.plane().renderer_lost();
  }
  fixture.wait(RendererContextPlane::loss_window_ns - 1u);
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  CHECK(!fixture.plane().alive());
  CHECK_EQ(fixture.qemu().renderer_starts(), RendererContextPlane::max_losses + 1u); }

TEST(renderer_context_plane_blobs, mirrors_guest_blobs_and_their_release)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.qemu().guest_ram(ram + 0x8000u)[0] = 0x3Cu;
  CHECK_STATUS(fixture.guest_blob(20u, 0x8000u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.guest_blob(21u, 0x9000u, 0x1000u), Status::ok);
  CHECK(fixture.renderer_blobs().contains(20u));
  CHECK(fixture.renderer_blobs().contains(21u));
  av::BlobWindow pages;
  CHECK_STATUS(fixture.renderer_blobs().backing(20u, pages), Status::ok);
  std::uint64_t bytes = 0u;
  CHECK(pages.extent(0u, bytes) == fixture.qemu().guest_ram(ram + 0x8000u));
  CHECK_EQ(bytes, 0x1000u);
  CHECK_EQ(fixture.peer().resources().live_descriptors(), 1u);
  CHECK_EQ(fixture.qemu().live_descriptors(), 0u);
  fixture.release(20u);
  CHECK(!fixture.renderer_blobs().contains(20u));
  CHECK(fixture.renderer_blobs().contains(21u));
  fixture.release(21u);
  CHECK(!fixture.renderer_blobs().contains(21u));
  const std::uint32_t sent = fixture.peer().delivered();
  fixture.plane().blob_released(99u);
  CHECK_EQ(fixture.peer().delivered(), sent);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_memory, is_allocated_in_the_renderer_and_its_dma_buf_crosses_to_qemu)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.ops().set_memory_caching(AV_MEMORY_WRITE_COMBINED);
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0x01000002u, 0x2000u, 0x101u, memory), Status::ok);
  CHECK(memory.fd >= 0);
  CHECK_EQ(memory.size, 0x2000u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  const MockExecutorOps::MemoryRequest asked = fixture.ops().last_memory_request();
  CHECK_EQ(asked.key, 0x01000002u);
  CHECK_EQ(asked.size, 0x2000u);
  CHECK_EQ(asked.request, 0x101u);
  CHECK(fixture.renderer_owners().owns(3u, 0x01000002u));
  CHECK(!fixture.renderer_owners().owns(4u, 0x01000002u));
  CHECK_EQ(fixture.qemu().place_in_hostmem(memory.fd, 0x2000u, 0u), Status::ok);
  static_cast<std::uint8_t *>(fixture.qemu().placed_at(0u))[0x1001] = 0x4Bu;
  CHECK_EQ(fixture.ops().memory_bytes(asked.domain, 0x01000002u)[0x1001], 0x4Bu);
  fixture.qemu().remove_from_hostmem(0u, 0x2000u);
  CHECK_EQ(fixture.qemu().live_descriptors(), 1u);
  fixture.qemu().close_descriptor(memory.fd);
  CHECK_EQ(fixture.peer().resources().live_descriptors(), 1u);
  fixture.plane().free_memory(0x01000002u);
  CHECK_EQ(fixture.ops().memory_frees(), 1u);
  CHECK_EQ(fixture.ops().held_memory(), 0u);
  CHECK(!fixture.renderer_owners().owns(3u, 0x01000002u));
  CHECK(fixture.plane().alive());
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_memory, memory_the_renderer_does_not_give_is_out_of_memory_and_ends_nothing)
{ Fixture fixture;
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0x01000002u, 0x1000u, 1u, memory), Status::no_memory);
  fixture.plane().free_memory(0x01000002u);
  CHECK_EQ(fixture.peer().delivered(), 0u);
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.ops().refuse_memory_at(1u);
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0x01000002u, 0x1000u, 1u, memory), Status::no_memory);
  CHECK_EQ(memory.fd, -1);
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 77u, 0x1000u, 1u, memory), Status::no_memory);
  CHECK_EQ(fixture.ops().memory_frees(), 1u);
  CHECK_EQ(fixture.ops().held_memory(), 0u);
  fixture.ops().withhold_memory_descriptor_at(1u);
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0x01000003u, 0x1000u, 1u, memory), Status::no_memory);
  CHECK_EQ(fixture.ops().held_memory(), 0u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  CHECK(fixture.plane().alive());
  CHECK_EQ(fixture.qemu().live_descriptors(), 0u); }

TEST(renderer_context_plane_memory, a_renderer_that_does_not_answer_or_cannot_be_told_is_given_up_on)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.peer().fall_silent();
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0x01000002u, 0x1000u, 1u, memory), Status::no_memory);
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK_EQ(fixture.plane().epoch(), 2u);
  fixture.qemu().renderer_gone();
  fixture.plane().free_memory(0x01000002u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 2u);
  CHECK_EQ(fixture.plane().epoch(), 3u);
  CHECK_EQ(fixture.qemu().live_descriptors(), 0u); }

TEST(renderer_context_plane_memory, a_garbled_answer_closes_what_came_with_it_and_ends_the_renderer)
{ MockHost qemu;
  av::ScriptedRendererPeer peer(qemu);
  av::BlobTable blobs(qemu);
  av::FenceInterrupts interrupts(qemu);
  RendererContextPlane plane(qemu, blobs, interrupts);
  static std::uint8_t region[0x1000];
  qemu.add_guest_region(0u, region, sizeof region);
  qemu.connect_renderer(peer);
  CHECK_STATUS(plane.configure(renderer_path, executor_path), Status::ok);
  CHECK_STATUS(plane.start(uuid, capset()), Status::ok);
  std::uint8_t datagram[av::RendererMessageHeader::encoded_size + av::AllocatedMemoryMessage::encoded_size] = {};
  CHECK_STATUS(av::RendererMessageHeader(av::RendererMessageHeader::Kind::allocated_memory,
                                         av::AllocatedMemoryMessage::encoded_size, 3u)
                   .encode(datagram, sizeof datagram),
               Status::ok);
  CHECK_STATUS(av::AllocatedMemoryMessage(AV_MEMORY_WRITE_BACK, 0x1000u)
                   .encode(datagram + av::RendererMessageHeader::encoded_size, av::AllocatedMemoryMessage::encoded_size),
               Status::ok);
  peer.queue(datagram, sizeof datagram, 0u);
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(plane.allocate_memory(3u, 0x01000002u, 0x1000u, 1u, memory), Status::no_memory);
  CHECK_EQ(qemu.renderer_kills(), 1u);
  CHECK_EQ(memory.fd, -1);
  std::uint8_t nothing[av::RendererMessageHeader::encoded_size + av::AllocatedMemoryMessage::encoded_size] = {};
  CHECK_STATUS(av::RendererMessageHeader(av::RendererMessageHeader::Kind::allocated_memory,
                                         av::AllocatedMemoryMessage::encoded_size, 6u)
                   .encode(nothing, sizeof nothing),
               Status::ok);
  peer.queue(nothing, sizeof nothing, 1u);
  CHECK_STATUS(plane.allocate_memory(3u, 0x01000002u, 0x1000u, 1u, memory), Status::no_memory);
  CHECK_EQ(qemu.renderer_kills(), 2u);
  CHECK_EQ(qemu.live_descriptors(), 0u);
  CHECK_STATUS(av::RendererMessageHeader(av::RendererMessageHeader::Kind::allocated_memory,
                                         av::AllocatedMemoryMessage::encoded_size, 9u)
                   .encode(nothing, sizeof nothing),
               Status::ok);
  peer.queue(nothing, sizeof nothing, 0u);
  CHECK_STATUS(plane.allocate_memory(3u, 0x01000002u, 0x1000u, 1u, memory), Status::no_memory);
  CHECK_EQ(qemu.renderer_kills(), 2u);
  CHECK(plane.alive()); }

TEST(renderer_context_plane_shared, a_guest_release_reaches_the_renderer_s_executor)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  const std::uint32_t before = fixture.ops().shared_releases();
  fixture.plane().shared_released(nullptr, 0x2Au);
  for (std::uint32_t spins = 0u; spins < 50000000u && fixture.ops().shared_releases() == before; spins++)
      sched_yield();
  CHECK_EQ(fixture.ops().shared_releases(), before + 1u);
  CHECK(fixture.ops().last_shared_release() == 0x2Au);
  CHECK(fixture.ops().last_domain() != 0u);
  CHECK(fixture.plane().alive());
  fixture.plane().shared_released(nullptr, 0u);
  for (std::uint32_t spins = 0u; spins < 50000000u && fixture.ops().shared_releases() == before + 1u; spins++)
      sched_yield();
  CHECK_EQ(fixture.ops().shared_releases(), before + 2u);
  CHECK(fixture.ops().last_shared_release() == 0u); }

TEST(renderer_context_plane_shared, a_release_with_no_renderer_is_told_to_nobody)
{ Fixture fixture;
  const std::uint32_t sent = fixture.peer().delivered();
  fixture.plane().shared_released(nullptr, 0x2Au);
  CHECK_EQ(fixture.peer().delivered(), sent);
  CHECK_EQ(fixture.ops().shared_releases(), 0u); }

TEST(renderer_context_plane_shared, a_release_the_renderer_cannot_take_gives_it_up)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.qemu().kill_renderer();
  CHECK_EQ(fixture.plane().epoch(), 1u);
  fixture.plane().shared_released(nullptr, 0x2Au);
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK_EQ(fixture.qemu().renderer_starts(), 2u);
  CHECK(fixture.plane().alive());
  CHECK(std::strstr(fixture.qemu().last_log(), "did not answer") != nullptr);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_blobs, blobs_made_before_the_start_are_told_at_the_start)
{ Fixture fixture;
  CHECK_STATUS(fixture.guest_blob(20u, 0x8000u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.guest_blob(22u, 0x9000u, 0x1000u), Status::ok);
  fixture.release(22u);
  CHECK_EQ(fixture.peer().delivered(), 0u);
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(fixture.renderer_blobs().contains(20u));
  CHECK(!fixture.renderer_blobs().contains(22u));
  CHECK_EQ(fixture.renderer_blobs().live(), 1u); }

TEST(renderer_context_plane_blobs, a_blob_outside_every_region_is_left_out_and_the_renderer_goes_on)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.qemu().add_guest_ram(0x800000u, 0x1000u);
  av::GpuRequestBuilder entries(0u);
  entries.u64(0x800000u).u32(0x1000u).u32(0u);
  av::MockRequest    mock(entries.bytes(), entries.size());
  av::ControlRequest request(fixture.qemu(), &mock);
  CHECK_STATUS(fixture.blobs().create_guest(30u, 0x1000u, request, 24u, 1u), Status::ok);
  fixture.plane().blob_created(30u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  CHECK_EQ(fixture.qemu().renderer_starts(), 1u);
  CHECK(fixture.plane().alive());
  CHECK(!fixture.renderer_blobs().contains(30u));
  CHECK(std::strstr(fixture.qemu().last_log(), "not given to it") != nullptr);
  const std::uint32_t delivered = fixture.peer().delivered();
  fixture.release(30u);
  CHECK_EQ(fixture.peer().delivered(), delivered);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_blobs, announcing_a_blob_the_table_does_not_hold_leaves_it_out)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.plane().blob_created(77u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  CHECK(fixture.plane().alive());
  fixture.plane().blob_released(77u);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_blobs, a_start_skips_a_blob_it_cannot_tell_and_tells_the_rest)
{ Fixture fixture;
  fixture.qemu().add_guest_ram(0x800000u, 0x1000u);
  av::GpuRequestBuilder entries(0u);
  entries.u64(0x800000u).u32(0x1000u).u32(0u);
  av::MockRequest    mock(entries.bytes(), entries.size());
  av::ControlRequest request(fixture.qemu(), &mock);
  CHECK_STATUS(fixture.blobs().create_guest(30u, 0x1000u, request, 24u, 1u), Status::ok);
  fixture.plane().blob_created(30u);
  CHECK_STATUS(fixture.guest_blob(20u, 0x8000u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(fixture.plane().alive());
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  CHECK(!fixture.renderer_blobs().contains(30u));
  CHECK(fixture.renderer_blobs().contains(20u)); }

TEST(renderer_context_plane_blobs, a_release_that_cannot_be_sent_ends_the_renderer)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_STATUS(fixture.guest_blob(20u, 0x8000u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.guest_blob(21u, 0x9000u, 0x1000u), Status::ok);
  fixture.qemu().renderer_gone();
  fixture.release(20u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK(fixture.plane().alive());
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK(!fixture.renderer_blobs().contains(20u));
  CHECK(fixture.renderer_blobs().contains(21u)); }

TEST(renderer_context_plane_blobs, remembers_every_blob_it_is_told_of_and_forgets_each_on_release)
{ Fixture fixture;
  constexpr std::uint32_t count = 1100u;
  for (std::uint32_t id = 1u; id <= count; id++) fixture.plane().blob_created(id);
  char            bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.plane().report(writer);
  CHECK(std::strstr(writer.dump(), "blobs=1100") != nullptr);
  CHECK(fixture.plane().invariants_hold());
  for (std::uint32_t id = 1u; id <= count; id++) fixture.plane().blob_released(id);
  char            after[256];
  av::StateWriter reported(after, sizeof after);
  fixture.plane().report(reported);
  CHECK(std::strstr(reported.dump(), "blobs=0") != nullptr);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_interrupts, follows_the_guests_interrupts_on_and_off)
{ Fixture fixture;
  fixture.connect_interrupts();
  CHECK_EQ(fixture.peer().delivered(), 0u);
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), lines);
  CHECK(fixture.peer().host().fence_interrupts_connected());
  CHECK_EQ(fixture.qemu().live_descriptors(), 0u);
  fixture.plane().interrupts_changed(false);
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), 0u);
  CHECK(!fixture.peer().host().fence_interrupts_connected());
  fixture.plane().interrupts_changed(true);
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), lines);
  fixture.qemu().renderer_gone();
  fixture.plane().interrupts_changed(false);
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), 0u);
  CHECK(!fixture.peer().host().fence_interrupts_connected());
  fixture.qemu().renderer_gone();
  fixture.plane().interrupts_changed(true);
  CHECK_EQ(fixture.plane().epoch(), 3u);
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), lines); }

TEST(renderer_context_plane_export_image, keeps_each_slots_dma_buf_until_the_image_changes)
{ Fixture fixture;
  fixture.ops().editable().export_image = &export_published;
  AvExportedImage image = {};
  CHECK(!fixture.plane().export_image(uuid, 0x77u, image));
  CHECK_STATUS(fixture.start(), Status::ok);
  const int executor_image = fresh_descriptor();
  published = Published{true, AvExportedImage{executor_image, 0u, 64u, 32u, 256u, 16u, 0x34325258u, 0u, 5u}};
  CHECK(fixture.plane().export_image(uuid, 0x77u, image));
  CHECK(image.fd >= 0);
  CHECK(image.fd != executor_image);
  CHECK(fixture.qemu().descriptor_open(image.fd));
  CHECK_EQ(image.slot, 0u);
  CHECK_EQ(image.width, 64u);
  CHECK_EQ(image.height, 32u);
  CHECK_EQ(image.stride, 256u);
  CHECK_EQ(image.offset, 16u);
  CHECK_EQ(image.fourcc, 0x34325258u);
  CHECK_EQ(image.modifier, 5u);
  CHECK_EQ(fixture.qemu().live_descriptors(), 1u);
  const int held = image.fd;
  AvExportedImage again = {};
  CHECK(fixture.plane().export_image(uuid, 0x77u, again));
  CHECK_EQ(again.fd, held);
  CHECK_EQ(fixture.qemu().live_descriptors(), 1u);
  published.image.height = 48u;
  CHECK(fixture.plane().export_image(uuid, 0x77u, again));
  CHECK_EQ(again.height, 48u);
  CHECK(fixture.qemu().descriptor_open(again.fd));
  CHECK_EQ(fixture.qemu().live_descriptors(), 1u);
  published.image.slot = 1u;
  CHECK(fixture.plane().export_image(uuid, 0x77u, again));
  CHECK_EQ(again.slot, 1u);
  CHECK_EQ(fixture.qemu().live_descriptors(), 2u);
  published.found = false;
  CHECK(!fixture.plane().export_image(uuid, 0x77u, again));
  CHECK_EQ(fixture.qemu().live_descriptors(), 2u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  CHECK_EQ(fixture.qemu().live_descriptors(), 0u);
  fixture.peer().fall_silent();
  CHECK(!fixture.plane().export_image(uuid, 0x77u, again));
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK_EQ(fixture.plane().epoch(), 3u);
  ::close(executor_image);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_export_image, a_garbled_answer_or_an_image_never_sent_shows_nothing)
{ MockHost qemu;
  av::ScriptedRendererPeer peer(qemu);
  av::BlobTable blobs(qemu);
  av::FenceInterrupts interrupts(qemu);
  RendererContextPlane plane(qemu, blobs, interrupts);
  static std::uint8_t region[0x1000];
  qemu.add_guest_region(0u, region, sizeof region);
  qemu.connect_renderer(peer);
  CHECK_STATUS(plane.configure(renderer_path, executor_path), Status::ok);
  CHECK_STATUS(plane.start(uuid, capset()), Status::ok);
  std::uint8_t datagram[av::RendererMessageHeader::encoded_size + av::ExportedImageMessage::encoded_size] = {};
  CHECK_STATUS(av::RendererMessageHeader(av::RendererMessageHeader::Kind::exported_image,
                                         av::ExportedImageMessage::encoded_size, 3u)
                   .encode(datagram, sizeof datagram),
               Status::ok);
  CHECK_STATUS(av::ExportedImageMessage(false, 1u, 64u, 32u, 256u, 0u, 0x34325258u, 0u)
                   .encode(datagram + av::RendererMessageHeader::encoded_size, av::ExportedImageMessage::encoded_size),
               Status::ok);
  peer.queue(datagram, sizeof datagram, 0u);
  AvExportedImage image = {};
  CHECK(!plane.export_image(uuid, 0x77u, image));
  CHECK_EQ(qemu.renderer_kills(), 0u);
  CHECK_STATUS(av::RendererMessageHeader(av::RendererMessageHeader::Kind::exported_image,
                                         av::ExportedImageMessage::encoded_size, 4u)
                   .encode(datagram, sizeof datagram),
               Status::ok);
  peer.queue(datagram, sizeof datagram, 1u);
  CHECK(!plane.export_image(uuid, 0x77u, image));
  CHECK_EQ(qemu.renderer_kills(), 1u);
  CHECK_EQ(qemu.live_descriptors(), 0u);
  CHECK(plane.alive());
  CHECK_EQ(plane.epoch(), 2u);
  CHECK_STATUS(av::RendererMessageHeader(av::RendererMessageHeader::Kind::exported_image,
                                         av::ExportedImageMessage::encoded_size, 7u)
                   .encode(datagram, sizeof datagram),
               Status::ok);
  datagram[av::RendererMessageHeader::encoded_size] = 2u;
  peer.queue(datagram, sizeof datagram, 0u);
  CHECK(!plane.export_image(uuid, 0x77u, image));
  CHECK_EQ(qemu.renderer_kills(), 2u);
  CHECK_EQ(qemu.live_descriptors(), 0u); }

TEST(renderer_context_plane_interrupts, lines_qemu_has_no_notifier_for_are_sent_absent)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.plane().interrupts_changed(true);
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), 0u);
  CHECK(fixture.peer().host().fence_interrupts_connected());
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u); }

TEST(renderer_context_plane, reads_no_image)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  std::uint32_t pixels[4] = {};
  CHECK(!fixture.plane().read_image(uuid, 9u, pixels, 8u, 2u, 2u)); }

TEST(renderer_context_plane, reads_no_staged_frame)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(!fixture.plane().reads_frames());
  AvFrameView view = {};
  CHECK(!fixture.plane().map_frame(uuid, av::SurfacePoint(9u, 1u), 1u, view));
  CHECK(view.pixels == nullptr);
  fixture.plane().unmap_frame(uuid, av::SurfacePoint(9u, 1u)); }

TEST(renderer_context_plane, reports_its_renderer_and_what_it_holds)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_STATUS(fixture.guest_blob(20u, 0x8000u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  char            bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.plane().report(writer);
  CHECK(std::strcmp(writer.dump(), "renderer/0 alive=true epoch=2 losses=1 blobs=1 contexts=1\n"
                                  "contexts/0 live=1\n"
                                  "contexts/0/context/3 name=test") == 0); }

TEST(renderer_context_plane_invariants, each_broken_promise_is_seen)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(fixture.plane().invariants_hold());
  RendererContextPlaneProbe::count_losses(fixture.plane(), RendererContextPlane::max_losses + 1u);
  CHECK(!fixture.plane().invariants_hold());
  RendererContextPlaneProbe::count_losses(fixture.plane(), RendererContextPlane::max_losses);
  CHECK(fixture.plane().invariants_hold());
  RendererContextPlaneProbe::unconfigure(fixture.plane());
  CHECK(!fixture.plane().invariants_hold()); }

TEST(renderer_context_plane, exports_no_surface)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(!fixture.plane().exports_surfaces());
  AvExportedImage image = {};
  CHECK(!fixture.plane().export_surface(uuid, 9u, 0u, image));
  std::uint64_t writes = 1u;
  int           sync_fd = 1;
  CHECK(!fixture.plane().sync_surface(uuid, 9u, 0u, writes, sync_fd)); }

TEST(renderer_context_plane, owns_no_memory_domain_of_its_own_and_composes_nothing)
{ Fixture fixture;
  CHECK_EQ(fixture.plane().memory_domain(), 0u);
  CHECK(!fixture.plane().composes());
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_EQ(fixture.plane().memory_domain(), 0u);
  CHECK(!fixture.plane().composes()); }

TEST(renderer_context_plane_memory, a_request_the_renderer_could_not_be_asked_is_out_of_memory_and_asks_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  const std::uint32_t delivered = fixture.peer().delivered();
  AvHostMemory        memory = {-1, 0u, 0u};
  CHECK_STATUS(fixture.plane().allocate_memory(0u, 0x01000002u, 0x1000u, 1u, memory), Status::no_memory);
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0u, 0x1000u, 1u, memory), Status::no_memory);
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0x01000002u, 0u, 1u, memory), Status::no_memory);
  CHECK_EQ(fixture.peer().delivered(), delivered);
  CHECK(fixture.plane().alive()); }

TEST(renderer_context_plane_blobs, a_blob_with_no_id_is_kept_until_released_and_its_release_ends_the_renderer)
{ Fixture fixture;
  fixture.plane().blob_created(0u);
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(fixture.plane().alive());
  fixture.plane().blob_released(0u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_blobs, a_blob_the_plane_has_no_room_to_remember_is_logged_and_left_out)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  for (std::uint32_t id = 20u; id < 28u; id++) CHECK_STATUS(fixture.guest_blob(id, 0x8000u + (id - 20u) * 0x1000u, 0x1000u), Status::ok);
  CHECK(fixture.renderer_blobs().contains(27u));
  av::GpuRequestBuilder entries(0u);
  entries.u64(ram + 0x9000u).u32(0x1000u).u32(0u);
  av::MockRequest    mock(entries.bytes(), entries.size());
  av::ControlRequest request(fixture.qemu(), &mock);
  CHECK_STATUS(fixture.blobs().create_guest(40u, 0x1000u, request, 24u, 1u), Status::ok);
  fixture.qemu().fail_allocation_at(1u);
  fixture.plane().blob_created(40u);
  CHECK(fixture.plane().alive());
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  CHECK(!fixture.renderer_blobs().contains(40u));
  CHECK(std::strstr(fixture.qemu().last_log(), "no memory to give a blob") != nullptr);
  CHECK(fixture.plane().invariants_hold());
  fixture.blobs().release_all(); }

TEST(renderer_context_plane_blobs, a_renderer_that_is_gone_when_a_blob_is_told_is_given_up_on)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.qemu().renderer_gone();
  CHECK_STATUS(fixture.guest_blob(20u, 0x8000u, 0x1000u), Status::ok);
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_blobs, a_blob_in_more_runs_than_a_message_can_name_is_left_out_and_the_renderer_goes_on)
{ MockHost                 qemu;
  MockExecutorOps          ops(both);
  av::LoopbackRendererPeer peer(qemu, ops.table());
  av::BlobTable            blobs(qemu);
  av::FenceInterrupts      interrupts(qemu);
  RendererContextPlane     plane(qemu, blobs, interrupts);
  constexpr std::uint64_t  step = 0x1000u;
  constexpr std::uint64_t  runs = av::GuestBlobMessage::max_extents + 1u;
  constexpr std::uint64_t  size = runs * step;
  qemu.add_guest_ram(0x10000000u, size);
  qemu.add_guest_region(0u, qemu.guest_ram(0x10000000u), size);
  qemu.set_hostmem_size(0x10000u);
  qemu.connect_renderer(peer);
  CHECK_STATUS(blobs.initialize(), Status::ok);
  CHECK_STATUS(interrupts.initialize(), Status::ok);
  CHECK_STATUS(plane.configure(renderer_path, executor_path), Status::ok);
  CHECK_STATUS(plane.initialize(), Status::ok);
  CHECK_STATUS(plane.start(uuid, capset()), Status::ok);
  qemu.split_guest_memory_every(step);
  av::GpuRequestBuilder entries(0u);
  entries.u64(0x10000000u).u32(static_cast<std::uint32_t>(size)).u32(0u);
  av::MockRequest    mock(entries.bytes(), entries.size());
  av::ControlRequest request(qemu, &mock);
  CHECK_STATUS(blobs.create_guest(20u, size, request, 24u, 1u), Status::ok);
  plane.blob_created(20u);
  CHECK(plane.alive());
  CHECK_EQ(qemu.renderer_kills(), 0u);
  CHECK(std::strstr(qemu.last_log(), "not given to it") != nullptr);
  plane.destroy_all();
  blobs.release_all(); }

TEST(renderer_context_plane_invariants, a_broken_blob_list_or_context_is_seen)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_STATUS(fixture.guest_blob(20u, 0x8000u, 0x1000u), Status::ok);
  CHECK(fixture.plane().invariants_hold());
  RendererContextPlaneProbe::corrupt_mirrored(fixture.plane(), 12u);
  CHECK(!fixture.plane().invariants_hold());
  RendererContextPlaneProbe::corrupt_mirrored(fixture.plane(), 8u);
  CHECK(fixture.plane().invariants_hold());
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok);
  CHECK(fixture.plane().invariants_hold());
  RendererContextPlaneProbe::lose_context(fixture.plane(), 3u);
  CHECK(!fixture.plane().invariants_hold());
  RendererContextPlaneProbe::strand_context(fixture.plane(), 3u);
  CHECK(!fixture.plane().invariants_hold()); }

namespace {

class ScriptedPlane {
public:
    explicit ScriptedPlane(std::uint32_t regions)
        : peer_(qemu_), blobs_(qemu_), interrupts_(qemu_), plane_(qemu_, blobs_, interrupts_)
    {
        qemu_.add_guest_ram(ram, ram_size);
        for (std::uint32_t region = 0u; region < regions; region++)
            qemu_.add_guest_region(region, qemu_.guest_ram(ram), ram_size);
        qemu_.set_hostmem_size(0x10000u);
        qemu_.connect_renderer(peer_);
        static_cast<void>(blobs_.initialize());
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(plane_.configure(renderer_path, executor_path));
        static_cast<void>(plane_.initialize());
    }
    ~ScriptedPlane()
    {
        plane_.destroy_all();
        blobs_.release_all();
    }

    MockHost                         &qemu() { return qemu_; }
    av::ScriptedRendererPeer         &peer() { return peer_; }
    RendererContextPlane             &plane() { return plane_; }
    Status                            start() { return plane_.start(uuid, capset()); }
    void blob(std::uint32_t id, std::uint64_t offset)
    {
        av::GpuRequestBuilder entries(0u);
        entries.u64(ram + offset).u32(0x1000u).u32(0u);
        av::MockRequest    mock(entries.bytes(), entries.size());
        av::ControlRequest request(qemu_, &mock);
        CHECK_STATUS(blobs_.create_guest(id, 0x1000u, request, 24u, 1u), Status::ok);
        plane_.blob_created(id);
    }

private:
    MockHost                 qemu_;
    av::ScriptedRendererPeer peer_;
    av::BlobTable            blobs_;
    av::FenceInterrupts      interrupts_;
    RendererContextPlane     plane_;
};

}

TEST(renderer_context_plane_start, a_renderer_lost_while_the_blobs_are_told_ends_the_start)
{ ScriptedPlane scripted(1u);
  scripted.blob(20u, 0x8000u);
  scripted.blob(21u, 0x9000u);
  scripted.blob(22u, 0xA000u);
  scripted.peer().die_on_delivery(3u);
  CHECK_STATUS(scripted.start(), Status::not_found);
  CHECK(!scripted.plane().alive());
  CHECK_EQ(scripted.qemu().renderer_kills(), 1u);
  CHECK_EQ(scripted.peer().delivered(), 3u); }

TEST(renderer_context_plane_start, a_region_the_host_cannot_hand_over_or_the_renderer_cannot_take_ends_the_start)
{ { ScriptedPlane failing(1u);
    failing.qemu().fail_guest_region_at(1u);
    CHECK_STATUS(failing.start(), Status::no_memory);
    CHECK(!failing.plane().alive());
    CHECK_EQ(failing.qemu().renderer_kills(), 1u);
    CHECK_EQ(failing.qemu().live_descriptors(), 0u); }
  { ScriptedPlane absurd(0u);
    absurd.qemu().add_guest_region(0x1000000u, absurd.qemu().guest_ram(ram), 0x1000u);
    CHECK_STATUS(absurd.start(), Status::invalid_parameter);
    CHECK(!absurd.plane().alive());
    CHECK_EQ(absurd.qemu().renderer_kills(), 1u);
    CHECK_EQ(absurd.qemu().live_descriptors(), 0u); }
  { ScriptedPlane dying(2u);
    dying.peer().die_on_delivery(2u);
    CHECK_STATUS(dying.start(), Status::not_found);
    CHECK(!dying.plane().alive());
    CHECK_EQ(dying.qemu().renderer_kills(), 1u);
    CHECK_EQ(dying.qemu().live_descriptors(), 0u); } }

TEST(renderer_context_plane, copies_no_surface)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(!fixture.plane().copies_surfaces());
  const AvCopyRegion regions[1] = {{0u, 0u, 0u, 0u, 4u, 4u}};
  CHECK(!fixture.plane().copy_surface(uuid, av::SurfacePair(9u, 10u), regions, 1u, 50u)); }

TEST(renderer_context_plane, reads_no_surface_texture)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  AvFrameView view = {};
  CHECK(!fixture.plane().map_surface(uuid, 9u, 50u, view));
  fixture.plane().unmap_surface(uuid, 9u); }
