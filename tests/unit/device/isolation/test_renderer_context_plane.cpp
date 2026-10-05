/* Contexts that run in the renderer, driven end to end: QEMU's plane over
 * MockHost's renderer socket, and on its far end the real RendererService -
 * a LocalContextPlane whose workers really run - reached only through the
 * bytes and descriptors the socket carries. A renderer that dies or falls
 * silent takes its contexts with it, the guest is told through the fence
 * pages QEMU can reach, and a new renderer is started and told everything
 * again - unless they keep dying. */
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "device/context/local_context_plane.hpp"
#include "device/control/control_request.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/isolation/renderer_context_plane.hpp"
#include "device/memory/blob_table.hpp"
#include "device/renderer/renderer_service.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/loopback_renderer_peer.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/host/scripted_renderer_peer.hpp"
#include "harness/context/guest_ring.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
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
    static LocalContextPlane &plane(RendererService &service) { return service.plane_; }
};

struct RendererContextPlaneProbe {
    static void count_losses(RendererContextPlane &plane, std::uint32_t count) { plane.loss_count_ = count; }
    static void count_blobs(RendererContextPlane &plane, std::uint32_t count) { plane.mirrored_count_ = count; }
    static void unconfigure(RendererContextPlane &plane) { plane.configured_ = false; }
};

}  // namespace av

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

/* What the renderer's executor has published under key 0x77, which a test
 * sets. */
struct Published {
    bool             found;
    AvExportedImage image;
};
Published published = {};

std::uint32_t export_published(void *, const std::uint8_t *, std::uint64_t key, AvExportedImage *image)
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
    return av::ContextSetup(av::ProtocolVersion(1u, 0u), api, ring_id, 12u, fence_id, doorbell, 0u);
}

/* QEMU's side - guest RAM shared with the renderer as region 0, its blob
 * table, its fence interrupts and the plane - and the renderer's, one socket
 * away. */
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

    Status start()
    {
        const Status configured = plane_.configure(renderer_path, executor_path);
        if (configured != Status::ok) return configured;
        const Status initialized = plane_.initialize();
        if (initialized != Status::ok) return initialized;
        return plane_.start(uuid, capset());
    }
    /* A guest blob of `size` bytes at `offset` of guest RAM, made as the
     * device's blob commands make it and announced to the plane. */
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
    Status host_blob(std::uint32_t id, std::uint64_t size)
    {
        const Status made = blobs_.create_host(id, size);
        if (made == Status::ok) plane_.blob_created(id);
        return made;
    }
    void release(std::uint32_t id)
    {
        plane_.blob_released(id);
        static_cast<void>(blobs_.retire(id));
    }
    /* The rings two contexts bind, each with a fresh header. */
    void rings()
    {
        av::GuestRing(qemu_.guest_ram(ram), 12u);
        av::GuestRing(qemu_.guest_ram(ram + other_ring_offset), 12u);
        CHECK_STATUS(guest_blob(ring, 0u, 0x2000u), Status::ok);
        CHECK_STATUS(guest_blob(fence, fence_offset, 0x1000u), Status::ok);
        CHECK_STATUS(guest_blob(other_ring, other_ring_offset, 0x2000u), Status::ok);
        CHECK_STATUS(guest_blob(other_fence, other_fence_offset, 0x1000u), Status::ok);
    }
    /* The fence interrupts connected, as the guest's DRIVER_OK connects them. */
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
    /* Waits for the renderer's worker to raise `line`. */
    bool raised_in_renderer(std::uint32_t line)
    {
        for (std::uint32_t spins = 0u; spins < 50000000u && peer_.host().raised(line) == 0u; spins++) sched_yield();
        return peer_.host().raised(line) != 0u;
    }
    /* The clock moved on by `nanoseconds`, and then still again. */
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

}  // namespace

/* Both paths are needed, and each must fit the hello with a byte to spare
 * for the terminator dlopen is given. */
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
  /* A refused configure undoes an earlier one. */
  CHECK_STATUS(plane.configure("", executor_path), Status::invalid_parameter);
  CHECK_STATUS(plane.initialize(), Status::invalid_state);
  CHECK(plane.invariants_hold()); }

/* The renderer loads the library; a device that hands the plane one too
 * asked for both, which is refused. */
TEST(renderer_context_plane_accept, takes_no_executor_table_of_its_own)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().accept(nullptr), Status::ok);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::invalid_parameter); }

/* Started, greeted with the library to load and the GPU, and handed guest
 * RAM - QEMU's own duplicate closed again - before any context. */
TEST(renderer_context_plane_start, starts_greets_and_hands_over_guest_memory)
{ Fixture fixture;
  fixture.ops().set_import_kinds(AV_BLOB_HOST_POINTER);
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(fixture.plane().alive());
  CHECK_EQ(fixture.plane().epoch(), 1u);
  CHECK_EQ(fixture.qemu().renderer_starts(), 1u);
  CHECK(std::strcmp(fixture.qemu().started_renderer(), renderer_path) == 0);
  CHECK(fixture.peer().service().ready());
  CHECK(std::strcmp(fixture.peer().resources().executor_path(), executor_path) == 0);
  CHECK_EQ(fixture.peer().memory().count(), 1u);
  CHECK_EQ(fixture.qemu().live_dmabufs(), 0u);
  /* What the Ready said, by the sub-API's own lane. */
  CHECK_EQ(fixture.plane().sub_apis(), both);
  CHECK_EQ(fixture.plane().import_kinds(av::SubApi::vulkan, uuid), static_cast<std::uint32_t>(AV_BLOB_HOST_POINTER));
  CHECK_EQ(fixture.plane().import_kinds(av::SubApi::opengl, uuid), static_cast<std::uint32_t>(AV_BLOB_HOST_POINTER));
  CHECK_EQ(fixture.plane().import_kinds(av::SubApi::opencl, uuid), 0u);
  CHECK_EQ(fixture.plane().import_kinds(static_cast<av::SubApi>(3u), uuid), 0u);
  CHECK_EQ(fixture.plane().import_kinds(static_cast<av::SubApi>(0x100u), uuid), 0u);
  CHECK(fixture.plane().invariants_hold()); }

TEST(renderer_context_plane_start, refuses_before_configure_or_without_a_gpu)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().start(uuid, capset()), Status::invalid_state);
  CHECK_STATUS(fixture.plane().configure(renderer_path, executor_path), Status::ok);
  CHECK_STATUS(fixture.plane().start(nullptr, capset()), Status::invalid_state);
  CHECK_EQ(fixture.qemu().renderer_starts(), 0u);
  CHECK(!fixture.plane().alive()); }

/* A renderer that does not spawn, does not answer its hello, or is given no
 * guest RAM to map is not alive - and one that did start is ended. */
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
  /* RAM with no memfd behind it is no region to hand over. */
  static std::uint8_t rom[0x1000];
  fixture.qemu().add_guest_region(1u, nullptr, sizeof rom);
  CHECK_STATUS(fixture.start(), Status::not_supported);
  CHECK_EQ(fixture.peer().memory().count(), 0u);
  CHECK(!fixture.plane().alive());
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK(fixture.plane().invariants_hold()); }

/* A context is the renderer's as much as QEMU's: made there, bound there,
 * and it runs there - a guest store to its doorbell wakes the renderer's
 * worker, which serves the ring in guest RAM and publishes the fence. */
TEST(renderer_context_plane, a_context_is_created_bound_and_run_in_the_renderer)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.connect_interrupts();
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(0u), Status::invalid_parameter);
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
  CHECK_STATUS(fixture.plane().create(3u), Status::conflict);
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

/* A guest's mistakes at bind are refused as they are in QEMU's own process,
 * and end nothing: no renderer is killed for a ring that is not there, a
 * setup the capset refuses, or a context bound twice. */
TEST(renderer_context_plane_bind, a_guests_refused_bind_ends_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u, av::SubApi::vulkan, 77u), capset()), Status::not_found);
  const av::ContextSetup too_many(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, ring, 12u, fence, 64u, 0u);
  CHECK_STATUS(fixture.plane().bind(3u, too_many, capset()), Status::not_supported);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok);
  CHECK_STATUS(fixture.plane().bind(3u, setup(6u), capset()), Status::invalid_state);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  CHECK_EQ(fixture.plane().epoch(), 1u);
  CHECK(fixture.plane().alive());
  CHECK(fixture.plane().holds_doorbell(5u));
  CHECK(!fixture.plane().holds_doorbell(6u));
  CHECK_EQ(fixture.qemu().live_doorbells(), 1u); }

/* The renderer's own refusal is the bind's answer: QEMU keeps nothing bound
 * and the renderer lives on. */
TEST(renderer_context_plane_bind, the_renderers_refusal_is_the_answer_and_unbinds_qemus_side)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
  fixture.peer().host().fail_allocation_at(1u);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::no_memory);
  CHECK(!fixture.plane().holds_doorbell(5u));
  CHECK_EQ(fixture.qemu().live_doorbells(), 0u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok); }

/* Silence at bind is the renderer's death: it is killed, a new one started,
 * and the context is left unbound. */
TEST(renderer_context_plane_bind, a_silent_renderer_at_bind_is_given_up_on)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
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
  CHECK_STATUS(fixture.plane().create(3u), Status::invalid_state);   /* none started */
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.peer().fall_silent();
  CHECK_STATUS(fixture.plane().create(3u), Status::invalid_state);
  CHECK(!fixture.plane().holds(3u));
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK_EQ(fixture.plane().epoch(), 2u);
  /* The new renderer takes it. */
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
  CHECK(fixture.renderer_plane().holds(3u)); }

/* The renderer's refusal is the create's answer, and QEMU keeps no slot. */
TEST(renderer_context_plane_create, the_renderers_refusal_leaves_no_slot)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.peer().host().fail_thread_start_at(1u);
  CHECK_STATUS(fixture.plane().create(3u), Status::no_memory);
  CHECK(!fixture.plane().holds(3u));
  CHECK_EQ(fixture.plane().live(), 0u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u); }

TEST(renderer_context_plane_create, a_full_plane_has_no_space)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  for (std::uint32_t id = 1u; id <= RendererContextPlane::max_contexts; id++)
      CHECK_STATUS(fixture.plane().create(id), Status::ok);
  const std::uint32_t sent = fixture.peer().delivered();
  CHECK_STATUS(fixture.plane().create(RendererContextPlane::max_contexts + 1u), Status::no_space);
  CHECK_EQ(fixture.peer().delivered(), sent);
  CHECK_EQ(fixture.plane().live(), RendererContextPlane::max_contexts); }

/* A destroy reaches the renderer while it runs the context; a lost one is
 * only QEMU's, and silence on destroy ends the renderer but not the destroy. */
TEST(renderer_context_plane_destroy, tells_the_renderer_unless_the_context_was_lost)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.rings();
  CHECK_STATUS(fixture.plane().destroy(3u), Status::not_found);
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
  CHECK_STATUS(fixture.plane().create(4u), Status::ok);
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
  /* A context lost with the renderer is destroyed without a word to the new
   * one. */
  CHECK_STATUS(fixture.plane().create(6u), Status::ok);
  CHECK_STATUS(fixture.plane().bind(6u, setup(7u), capset()), Status::ok);
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  const std::uint32_t sent = fixture.peer().delivered();
  CHECK_STATUS(fixture.plane().destroy(6u), Status::ok);
  CHECK_EQ(fixture.peer().delivered(), sent);
  CHECK_EQ(fixture.plane().live(), 0u); }

/* Only a bound, living OpenGL context other than the asker may be shared. */
TEST(renderer_context_plane_can_share, only_a_bound_living_opengl_context)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.rings();
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
  CHECK_STATUS(fixture.plane().create(4u), Status::ok);
  CHECK_STATUS(fixture.plane().create(8u), Status::ok);
  CHECK(!fixture.plane().can_share(8u, 3u));   /* not bound */
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u, av::SubApi::opengl), capset()), Status::ok);
  CHECK_STATUS(fixture.plane().bind(4u, setup(6u, av::SubApi::vulkan, other_ring, other_fence), capset()), Status::ok);
  CHECK(fixture.plane().can_share(8u, 3u));
  CHECK(!fixture.plane().can_share(3u, 3u));   /* itself */
  CHECK(!fixture.plane().can_share(8u, 4u));   /* Vulkan */
  CHECK(!fixture.plane().can_share(8u, 9u));   /* nothing */
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  CHECK(!fixture.plane().can_share(8u, 3u)); } /* lost */

/* The renderer's death is every bound context's: each fence page keeps what
 * it had completed, steps once more with device_lost, and its interrupt is
 * raised; a new renderer is started and told the regions, blobs and
 * interrupt lines again before it takes anything. */
TEST(renderer_context_plane_renderer_lost, reports_every_bound_context_lost_and_replays_everything)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.connect_interrupts();
  fixture.rings();
  CHECK_STATUS(fixture.host_blob(9u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
  CHECK_STATUS(fixture.plane().create(4u), Status::ok);
  CHECK_STATUS(fixture.plane().create(8u), Status::ok);   /* never bound */
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
  /* The page stays the lost context's until the guest destroys it. */
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
  CHECK_EQ(fixture.renderer_plane().live(), 0u);   /* the old contexts died with it */
  CHECK_EQ(fixture.qemu().live_dmabufs(), 0u);
  CHECK(fixture.plane().invariants_hold());
  /* A lost context is not bound again; a new one is, on the new renderer. */
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::invalid_state);
  CHECK_STATUS(fixture.plane().destroy(3u), Status::ok);
  CHECK(!fixture.plane().holds_doorbell(5u));
  av::GuestRing(fixture.qemu().guest_ram(ram), 12u);
  CHECK_STATUS(fixture.plane().create(10u), Status::ok);
  CHECK_STATUS(fixture.plane().bind(10u, setup(5u), capset()), Status::ok);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  /* A second report of the same death, while the new one lives, is a new
   * death; one while none lives says nothing. */
  fixture.qemu().fail_renderer_start_at(1u);
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  CHECK(!fixture.plane().alive());
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK_EQ(fixture.qemu().raised(5u), 2u);
  fixture.plane().renderer_lost();
  CHECK_EQ(fixture.qemu().raised(5u), 2u);
  CHECK_STATUS(fixture.plane().create(11u), Status::invalid_state);
  CHECK_STATUS(fixture.plane().bind(10u, setup(5u), capset()), Status::invalid_state); }

/* More deaths than max_losses inside the window and no renderer is started
 * again; a death exactly a window after the others counts alone. */
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
  /* Nothing restarts it: it stays down. */
  fixture.wait(RendererContextPlane::loss_window_ns);
  fixture.plane().renderer_lost();
  CHECK(!fixture.plane().alive());
  CHECK_STATUS(fixture.plane().create(3u), Status::invalid_state); }

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

/* A blob QEMU makes is the renderer's too - over the same guest pages, or
 * the memfd QEMU placed - and one QEMU lets go of is retired there. */
TEST(renderer_context_plane_blobs, mirrors_guest_and_host_blobs_and_their_release)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.qemu().guest_ram(ram + 0x8000u)[0] = 0x3Cu;
  CHECK_STATUS(fixture.guest_blob(20u, 0x8000u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.host_blob(21u, 0x1000u), Status::ok);
  CHECK(fixture.renderer_blobs().contains(20u));
  CHECK(fixture.renderer_blobs().contains(21u));
  av::BlobWindow pages;
  void          *host_memory = nullptr;
  CHECK_STATUS(fixture.renderer_blobs().backing(20u, pages, host_memory), Status::ok);
  std::uint64_t bytes = 0u;
  CHECK(pages.extent(0u, bytes) == fixture.qemu().guest_ram(ram + 0x8000u));
  CHECK_EQ(bytes, 0x1000u);
  CHECK_EQ(fixture.peer().resources().live_descriptors(), 2u);   /* the region's, the offered memfd */
  CHECK_EQ(fixture.qemu().live_dmabufs(), 0u);
  fixture.release(20u);
  CHECK(!fixture.renderer_blobs().contains(20u));
  CHECK(fixture.renderer_blobs().contains(21u));
  fixture.release(21u);
  CHECK(!fixture.renderer_blobs().contains(21u));
  /* One released that was never mirrored is nothing to say. */
  const std::uint32_t sent = fixture.peer().delivered();
  fixture.plane().blob_released(99u);
  CHECK_EQ(fixture.peer().delivered(), sent);
  CHECK(fixture.plane().invariants_hold()); }

/* What was shared lives in the renderer's contexts, so the guest's release
 * reaches the executor there, and so does a reset's release of everything. A
 * renderer that is gone took everything with it and is told nothing. */
TEST(renderer_context_plane_shared, a_guest_release_reaches_the_renderer_s_executor)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  const std::uint32_t before = fixture.ops().shared_releases();
  fixture.plane().shared_released(nullptr, 0x2Au);
  for (std::uint32_t spins = 0u; spins < 50000000u && fixture.ops().shared_releases() == before; spins++)
      sched_yield();
  CHECK_EQ(fixture.ops().shared_releases(), before + 1u);
  CHECK(fixture.ops().last_shared_release() == 0x2Au);
  CHECK(fixture.ops().last_shared_gpu() != nullptr);
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

/* A renderer the release cannot be sent to took what it held with it: it is
 * given up on, as for any message it cannot take, and a new one started. */
TEST(renderer_context_plane_shared, a_release_the_renderer_cannot_take_gives_it_up)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.qemu().kill_renderer();   /* gone underneath, not yet noticed */
  CHECK_EQ(fixture.plane().epoch(), 1u);
  fixture.plane().shared_released(nullptr, 0x2Au);
  CHECK_EQ(fixture.plane().epoch(), 2u);
  CHECK_EQ(fixture.qemu().renderer_starts(), 2u);
  CHECK(fixture.plane().alive());
  CHECK(std::strstr(fixture.qemu().last_log(), "did not answer") != nullptr);
  CHECK(fixture.plane().invariants_hold()); }

/* Blobs made before any renderer runs are told to the first one; one let go
 * before is never told. */
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

/* A blob the renderer cannot be told of - guest pages outside every region
 * it was given - is left out, and the renderer goes on: ending it would only
 * be repeated by every relaunch. */
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
  /* Its release tells the renderer nothing: it never had it. */
  const std::uint32_t delivered = fixture.peer().delivered();
  fixture.release(30u);
  CHECK_EQ(fixture.peer().delivered(), delivered);
  CHECK(fixture.plane().invariants_hold()); }

/* A blob QEMU's table does not hold cannot be told of: it is left out, as
 * one outside the renderer's memory is. */
TEST(renderer_context_plane_blobs, announcing_a_blob_the_table_does_not_hold_leaves_it_out)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.plane().blob_created(77u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  CHECK(fixture.plane().alive());
  fixture.plane().blob_released(77u);
  CHECK(fixture.plane().invariants_hold()); }

/* A blob that cannot be told is skipped at the start too, and the blobs
 * after it are still told. */
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

/* A release the renderer cannot be sent ends it. */
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
  CHECK(!fixture.renderer_blobs().contains(20u));   /* the new one is told only what is left */
  CHECK(fixture.renderer_blobs().contains(21u)); }

/* Blobs past what the table can hold are not remembered; the count never
 * passes the table's own bound. */
TEST(renderer_context_plane_blobs, remembers_no_more_blobs_than_the_table_holds)
{ Fixture fixture;
  for (std::uint32_t id = 1u; id <= av::BlobTable::max_blobs + 1u; id++) fixture.plane().blob_created(id);
  char            bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.plane().report(writer);
  CHECK(std::strstr(writer.dump(), "blobs=1024") != nullptr);
  for (std::uint32_t id = 1u; id <= av::BlobTable::max_blobs + 1u; id++) fixture.plane().blob_released(id);
  CHECK(fixture.plane().invariants_hold()); }

/* The interrupt lines follow the guest's: every line's notifier sent while
 * connected, interrupts_off when not - and a renderer started later is told
 * the state they are in. */
TEST(renderer_context_plane_interrupts, follows_the_guests_interrupts_on_and_off)
{ Fixture fixture;
  fixture.connect_interrupts();
  CHECK_EQ(fixture.peer().delivered(), 0u);
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), lines);
  CHECK(fixture.peer().host().fence_interrupts_connected());
  CHECK_EQ(fixture.qemu().live_dmabufs(), 0u);   /* QEMU's duplicates closed */
  fixture.plane().interrupts_changed(false);
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), 0u);
  CHECK(!fixture.peer().host().fence_interrupts_connected());
  fixture.plane().interrupts_changed(true);
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), lines);
  /* A renderer gone when they change is given up on and its successor told. */
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

/* A scanout image crosses once per image: QEMU keeps the dma-buf last sent
 * for each slot and answers from it until the renderer says the slot holds
 * another. Its own copies go with the renderer. */
TEST(renderer_context_plane_export_image, keeps_each_slots_dma_buf_until_the_image_changes)
{ Fixture fixture;
  fixture.ops().editable().export_image = &export_published;
  AvExportedImage image = {};
  CHECK(!fixture.plane().export_image(uuid, 0x77u, image));   /* no renderer */
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
  CHECK_EQ(fixture.qemu().live_dmabufs(), 1u);
  const int held = image.fd;
  AvExportedImage again = {};
  CHECK(fixture.plane().export_image(uuid, 0x77u, again));
  CHECK_EQ(again.fd, held);
  CHECK_EQ(fixture.qemu().live_dmabufs(), 1u);
  /* The slot holds another image: the old copy goes, the new one is kept. */
  published.image.height = 48u;
  CHECK(fixture.plane().export_image(uuid, 0x77u, again));
  CHECK_EQ(again.height, 48u);
  CHECK(fixture.qemu().descriptor_open(again.fd));
  CHECK_EQ(fixture.qemu().live_dmabufs(), 1u);
  /* The other slot is kept beside it. */
  published.image.slot = 1u;
  CHECK(fixture.plane().export_image(uuid, 0x77u, again));
  CHECK_EQ(again.slot, 1u);
  CHECK_EQ(fixture.qemu().live_dmabufs(), 2u);
  /* Nothing published: nothing shown, nothing let go. */
  published.found = false;
  CHECK(!fixture.plane().export_image(uuid, 0x77u, again));
  CHECK_EQ(fixture.qemu().live_dmabufs(), 2u);
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u);
  /* The renderer's images go with it. */
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  CHECK_EQ(fixture.qemu().live_dmabufs(), 0u);
  /* A silent renderer is given up on. */
  fixture.peer().fall_silent();
  CHECK(!fixture.plane().export_image(uuid, 0x77u, again));
  CHECK_EQ(fixture.qemu().renderer_kills(), 1u);
  CHECK_EQ(fixture.plane().epoch(), 3u);
  ::close(executor_image);
  CHECK(fixture.plane().invariants_hold()); }

/* An answer no renderer sends ends the renderer, with what came with it
 * closed; an image said to be the one already sent, when none was, is none. */
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
  /* hello 1, region 2: the export is 3. */
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
  /* The same answer with a descriptor it does not announce: export 4. */
  CHECK_STATUS(av::RendererMessageHeader(av::RendererMessageHeader::Kind::exported_image,
                                         av::ExportedImageMessage::encoded_size, 4u)
                   .encode(datagram, sizeof datagram),
               Status::ok);
  peer.queue(datagram, sizeof datagram, 1u);
  CHECK(!plane.export_image(uuid, 0x77u, image));
  CHECK_EQ(qemu.renderer_kills(), 1u);
  CHECK_EQ(qemu.live_dmabufs(), 0u);
  CHECK(plane.alive());
  CHECK_EQ(plane.epoch(), 2u);
  /* A flag that is neither 0 nor 1, with nothing beside it: hello 5, region
   * 6, export 7. */
  CHECK_STATUS(av::RendererMessageHeader(av::RendererMessageHeader::Kind::exported_image,
                                         av::ExportedImageMessage::encoded_size, 7u)
                   .encode(datagram, sizeof datagram),
               Status::ok);
  datagram[av::RendererMessageHeader::encoded_size] = 2u;
  peer.queue(datagram, sizeof datagram, 0u);
  CHECK(!plane.export_image(uuid, 0x77u, image));
  CHECK_EQ(qemu.renderer_kills(), 2u);
  CHECK_EQ(qemu.live_dmabufs(), 0u); }

/* A line QEMU has no notifier for is marked absent, and none is sent. */
TEST(renderer_context_plane_interrupts, lines_qemu_has_no_notifier_for_are_sent_absent)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.plane().interrupts_changed(true);   /* QEMU's own are not connected */
  CHECK_EQ(fixture.peer().resources().interrupt_lines(), 0u);
  CHECK(fixture.peer().host().fence_interrupts_connected());
  CHECK_EQ(fixture.qemu().renderer_kills(), 0u); }

/* Scanout images come through the renderer in a later phase; read_image is
 * not offered. */
TEST(renderer_context_plane, reads_no_image)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  std::uint32_t pixels[4] = {};
  CHECK(!fixture.plane().read_image(uuid, 9u, pixels, 8u, 2u, 2u)); }

/* A frame a renderer's session stages is in the renderer's memory: none is
 * read here, and letting one go does nothing. */
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
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
  fixture.qemu().renderer_gone();
  fixture.plane().renderer_lost();
  char            bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.plane().report(writer);
  CHECK(std::strcmp(writer.dump(), "renderer/0 alive=true epoch=2 losses=1 blobs=1 contexts=1") == 0); }

/* Each of what the plane promises of itself, broken, is seen. */
TEST(renderer_context_plane_invariants, each_broken_promise_is_seen)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(fixture.plane().invariants_hold());
  RendererContextPlaneProbe::count_losses(fixture.plane(), RendererContextPlane::max_losses + 1u);
  CHECK(!fixture.plane().invariants_hold());
  RendererContextPlaneProbe::count_losses(fixture.plane(), RendererContextPlane::max_losses);
  CHECK(fixture.plane().invariants_hold());
  RendererContextPlaneProbe::count_blobs(fixture.plane(), av::BlobTable::max_blobs + 1u);
  CHECK(!fixture.plane().invariants_hold());
  RendererContextPlaneProbe::count_blobs(fixture.plane(), 0u);
  RendererContextPlaneProbe::unconfigure(fixture.plane());
  CHECK(!fixture.plane().invariants_hold()); }

/* A renderer's surfaces are in its process: none is exported from here. */
TEST(renderer_context_plane, exports_no_surface)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(!fixture.plane().exports_surfaces());
  AvExportedImage image = {};
  CHECK(!fixture.plane().export_surface(uuid, 9u, image));
  std::uint64_t writes = 1u;
  int           sync_fd = 1;
  CHECK(!fixture.plane().sync_surface(uuid, 9u, writes, sync_fd)); }
