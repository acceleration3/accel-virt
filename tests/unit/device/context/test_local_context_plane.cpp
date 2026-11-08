#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "device/context/local_context_plane.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/host_blob_table.hpp"
#include "harness/context/guest_blob_maker.hpp"
#include "device/recording/wire_recorder.hpp"
#include "proto/resources/surface_pair.hpp"
#include "proto/resources/surface_point.hpp"
#include "harness/context/local_context_plane_probe.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/context/ring_blob_writer.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"

#include <cstdint>
#include <cstring>

using av::BlobTable;
using av::FenceInterrupts;
using av::LocalContextPlane;
using av::LocalContextPlaneProbe;
using av::MockExecutorOps;
using av::MockHost;
using av::Status;

namespace {

constexpr std::uint32_t vulkan_only = 1u << static_cast<std::uint32_t>(av::SubApi::vulkan);
constexpr std::uint32_t ring = 1u;
constexpr std::uint32_t fence = 2u;

class Fixture {
public:
    Fixture()
        : blobs_(host_), maker_(host_, blobs_), owners_(host_), interrupts_(host_), recorder_(host_), ops_(vulkan_only),
          plane_(host_, blobs_, owners_, interrupts_, recorder_)
    {
        host_.set_hostmem_size(0x10000u);
        static_cast<void>(blobs_.initialize());
        static_cast<void>(maker_.create(ring, 0x2000u));
        static_cast<void>(maker_.create(fence, 0x1000u));
        av::RingBlobWriter(blobs_).write_header(ring, 12u);
    }
    ~Fixture() { plane_.destroy_all(); }

    MockHost          &host() { return host_; }
    LocalContextPlane &plane() { return plane_; }
    MockExecutorOps   &ops() { return ops_; }

private:
    MockHost          host_;
    BlobTable         blobs_;
    av::GuestBlobMaker maker_;
    av::HostBlobTable owners_;
    FenceInterrupts   interrupts_;
    av::WireRecorder  recorder_;
    MockExecutorOps   ops_;
    LocalContextPlane plane_;
};

const std::uint8_t uuid[av::GpuIdentity::uuid_bytes] = {7u};

av::CapsetInfo capset()
{
    return av::CapsetInfo(av::ProtocolVersion(1u, 0u), 1u, 64u, 12u, 24u, 0u,
                          av::GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, ""));
}

av::ContextSetup setup(std::uint32_t doorbell)
{
    return av::ContextSetup(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, ring, 12u, fence, doorbell, 0u);
}

}

TEST(local_context_plane, answers_what_the_library_it_accepted_offers)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().accept(nullptr), Status::ok);
  CHECK_EQ(fixture.plane().sub_apis(), 0u);
  fixture.ops().editable().abi_version = 0u;
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::invalid_parameter);
  fixture.ops().editable().abi_version = AV_EXECUTOR_ABI_VERSION;
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK_EQ(fixture.plane().sub_apis(), vulkan_only);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::invalid_state); }

TEST(local_context_plane, starting_opens_the_domain_with_the_capset_s_vram_reserve)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  const std::uint8_t other_gpu[16] = {1u};
  av::CapsetInfo capset;
  capset.reserve_vram(4096u);
  CHECK_STATUS(fixture.plane().start(other_gpu, capset), Status::ok);
  CHECK_EQ(fixture.ops().last_vram_reserve_mib(), 4096u); }

TEST(local_context_plane, starting_opens_the_domain_with_the_host_ram_the_capset_declares)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  const std::uint8_t other_gpu[16] = {1u};
  const av::CapsetInfo capset(av::ProtocolVersion(av::ProtocolVersion::current_major, av::ProtocolVersion::current_minor),
                              1u, 64u, 12u, 24u, 0u,
                              av::GpuIdentity(0x1002u, 0x7550u, av::GpuIdentity::discrete_gpu, other_gpu, 1u << 30,
                                              13ull << 30, "gpu"));
  CHECK_STATUS(fixture.plane().start(other_gpu, capset), Status::ok);
  CHECK_EQ(fixture.ops().last_host_ram_bytes(), 13ull << 30); }

TEST(local_context_plane, starting_hands_the_executor_the_host_ram_floor_the_capset_carries)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  const std::uint8_t other_gpu[16] = {1u};
  av::CapsetInfo capset(av::ProtocolVersion(av::ProtocolVersion::current_major, av::ProtocolVersion::current_minor),
                        1u, 64u, 12u, 24u, 0u,
                        av::GpuIdentity(0x1002u, 0x7550u, av::GpuIdentity::discrete_gpu, other_gpu, 1u << 30,
                                        13ull << 30, "gpu"));
  capset.floor_host_ram(7ull << 30);
  CHECK_STATUS(fixture.plane().start(other_gpu, capset), Status::ok);
  CHECK_EQ(fixture.ops().last_host_ram_floor_bytes(), 7ull << 30); }

TEST(local_context_plane, allocates_and_frees_host_memory_in_the_device_s_domain)
{ Fixture fixture;
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0x01000000u, 0x2000u, 1u, memory), Status::no_memory);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0x01000000u, 0x2000u, 1u, memory), Status::no_memory);
  CHECK_EQ(fixture.ops().memory_allocations(), 0u);
  fixture.plane().free_memory(0x01000000u);
  CHECK_EQ(fixture.ops().memory_frees(), 0u);
  CHECK_STATUS(fixture.plane().start(uuid, capset()), Status::ok);
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0x01000000u, 0x2000u, 1u, memory), Status::ok);
  CHECK(fixture.host().descriptor_open(memory.fd));
  CHECK_EQ(memory.size, 0x2000u);
  CHECK_EQ(memory.caching, static_cast<std::uint32_t>(AV_MEMORY_WRITE_BACK));
  const MockExecutorOps::MemoryRequest asked = fixture.ops().last_memory_request();
  CHECK_EQ(asked.domain, fixture.ops().last_opened_domain());
  CHECK_EQ(asked.key, 0x01000000u);
  CHECK_EQ(asked.size, 0x2000u);
  CHECK_EQ(asked.request, 1u);
  fixture.ops().refuse_memory_at(1u);
  AvHostMemory refused = {-1, 0u, 0u};
  CHECK_STATUS(fixture.plane().allocate_memory(3u, 0x01000001u, 0x1000u, 1u, refused), Status::no_memory);
  CHECK_EQ(refused.fd, -1);
  fixture.plane().free_memory(0x01000000u);
  CHECK_EQ(fixture.ops().memory_frees(), 1u);
  CHECK_EQ(fixture.ops().live_memory(), 0u);
  fixture.host().close_descriptor(memory.fd); }

TEST(local_context_plane, reads_staged_frames_through_its_library)
{ Fixture fixture;
  const av::SurfacePoint frame(3u, 9u);
  CHECK(!fixture.plane().reads_frames());
  AvFrameView view = {};
  CHECK(!fixture.plane().map_frame(uuid, frame, 1u, view));
  static const std::uint8_t pixels[16] = {};
  fixture.ops().serve_frame(3u, 9u, AvFrameView{pixels, 16u, 4u, 1u, 0u});
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK(fixture.plane().reads_frames());
  CHECK(!fixture.plane().map_frame(uuid, frame, 1u, view));
  CHECK_STATUS(fixture.plane().start(uuid, capset()), Status::ok);
  CHECK(!fixture.plane().map_frame(uuid, av::SurfacePoint(3u, 8u), 1u, view));
  CHECK(fixture.plane().map_frame(uuid, frame, 1u, view));
  CHECK(view.pixels == pixels);
  CHECK_EQ(view.width, 4u);
  fixture.plane().unmap_frame(uuid, frame);
  CHECK_EQ(fixture.ops().frame_unmaps(), 1u);
  CHECK(!fixture.plane().map_frame(uuid, frame, 1u, view)); }

TEST(local_context_plane, initialize_fails_only_when_a_lock_cannot_be_made)
{ { Fixture fixture;
    fixture.host().fail_mutex_at(1u);
    CHECK_STATUS(fixture.plane().initialize(), Status::no_memory); }
  { Fixture fixture;
    fixture.host().fail_mutex_at(2u);
    CHECK_STATUS(fixture.plane().initialize(), Status::no_memory); }
  Fixture fixture;
  CHECK_STATUS(fixture.plane().initialize(), Status::ok);
  CHECK_STATUS(fixture.plane().initialize(), Status::ok);
  CHECK(fixture.plane().invariants_hold()); }

TEST(local_context_plane, runs_contexts_through_its_table)
{ Fixture fixture;
  av::ContextPlane &plane = fixture.plane();
  CHECK_STATUS(plane.initialize(), Status::ok);
  CHECK_STATUS(plane.accept(&fixture.ops().table()), Status::ok);
  CHECK_STATUS(plane.start(uuid, capset()), Status::ok);
  CHECK_STATUS(fixture.plane().create(0u, "test"), Status::invalid_parameter);
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::conflict);
  CHECK(fixture.plane().holds(3u));
  CHECK(!fixture.plane().holds(4u));
  CHECK_EQ(fixture.plane().live(), 1u);
  CHECK_STATUS(fixture.plane().bind(4u, setup(5u), capset()), Status::invalid_parameter);
  CHECK(!fixture.plane().holds_doorbell(5u));
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok);
  CHECK(LocalContextPlaneProbe::table(fixture.plane()).find(3u)->binding().bound());
  CHECK(fixture.plane().holds_doorbell(5u));
  CHECK_STATUS(fixture.plane().create(6u, "test"), Status::ok);
  CHECK(!fixture.plane().can_share(6u, 3u));
  CHECK(fixture.plane().invariants_hold());
  CHECK_STATUS(fixture.plane().destroy(3u), Status::ok);
  CHECK_STATUS(fixture.plane().destroy(3u), Status::not_found);
  CHECK(!fixture.plane().holds_doorbell(5u));
  CHECK_EQ(fixture.plane().live(), 1u); }

TEST(local_context_plane, opens_the_device_s_domain_once_and_closes_it_as_it_goes)
{ MockExecutorOps ops(vulkan_only);
  { MockHost          host;
    BlobTable         blobs(host);
    av::HostBlobTable owners(host);
    FenceInterrupts   interrupts(host);
    av::WireRecorder  recorder(host);
    LocalContextPlane plane(host, blobs, owners, interrupts, recorder);
    CHECK_STATUS(plane.initialize(), Status::ok);
    CHECK_STATUS(plane.accept(&ops.table()), Status::ok);
    CHECK_STATUS(plane.start(uuid, capset()), Status::ok);
    CHECK_STATUS(plane.start(uuid, capset()), Status::ok);
    CHECK_EQ(ops.domains_opened(), 1u);
    CHECK_EQ(ops.last_domain_gpu(), 7u);
    plane.destroy_all();
    CHECK_EQ(ops.domains_closed(), 0u); }
  CHECK_EQ(ops.domains_closed(), 1u);
  MockHost          host;
  BlobTable         blobs(host);
  av::HostBlobTable owners(host);
  FenceInterrupts   interrupts(host);
  av::WireRecorder  recorder(host);
  LocalContextPlane plane(host, blobs, owners, interrupts, recorder);
  CHECK_STATUS(plane.accept(&ops.table()), Status::ok);
  ops.refuse_domain_at(1u);
  CHECK_STATUS(plane.start(uuid, capset()), Status::ok);
  CHECK_EQ(ops.domains_opened(), 1u); }

TEST(local_context_plane, a_guest_release_tells_the_library_in_the_device_s_domain)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK_STATUS(fixture.plane().initialize(), Status::ok);
  CHECK_STATUS(fixture.plane().start(uuid, capset()), Status::ok);
  fixture.plane().shared_released(uuid, 7u);
  CHECK_EQ(fixture.ops().shared_releases(), 1u);
  CHECK(fixture.ops().last_shared_release() == 7u);
  CHECK(fixture.ops().last_domain() != 0u);
  fixture.plane().shared_released(uuid, 0u);
  CHECK_EQ(fixture.ops().shared_releases(), 2u);
  CHECK(fixture.ops().last_shared_release() == 0u); }

TEST(local_context_plane, an_image_nothing_published_is_neither_exported_nor_read)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().initialize(), Status::ok);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  AvExportedImage image = {};
  CHECK(!fixture.plane().export_image(uuid, 9u, image));
  std::uint32_t pixels[4] = {};
  CHECK(!fixture.plane().read_image(uuid, 9u, pixels, 8u, 2u, 2u)); }

TEST(local_context_plane, reports_its_contexts)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().initialize(), Status::ok);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  writer.open("device", 0u);
  fixture.plane().report(writer);
  CHECK(std::strstr(writer.dump(), "contexts/0 live=0") != nullptr);
  CHECK(std::strstr(writer.dump(), "shared/0") == nullptr); }

TEST(local_context_plane, the_renderers_hooks_change_nothing_in_process)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().initialize(), Status::ok);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK_STATUS(fixture.plane().start(uuid, capset()), Status::ok);
  CHECK_STATUS(fixture.plane().create(3u, "test"), Status::ok);
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok);
  const std::uint32_t threads = fixture.host().live_threads();
  av::ContextPlane &plane = fixture.plane();
  plane.blob_created(ring);
  plane.blob_released(fence);
  plane.interrupts_changed(false);
  plane.interrupts_changed(true);
  plane.renderer_lost();
  CHECK_EQ(plane.live(), 1u);
  CHECK(plane.holds(3u));
  CHECK(plane.holds_doorbell(5u));
  CHECK(LocalContextPlaneProbe::table(fixture.plane()).find(3u)->binding().bound());
  CHECK_EQ(fixture.host().live_threads(), threads);
  CHECK_EQ(fixture.host().live_doorbells(), 1u);
  CHECK_EQ(fixture.host().raised(5u), 0u);
  CHECK(plane.invariants_hold()); }

TEST(local_context_plane, exports_surfaces_through_its_library)
{ Fixture fixture;
  CHECK(!fixture.plane().exports_surfaces());
  AvExportedImage image = {};
  CHECK(!fixture.plane().export_surface(uuid, 5u, 0u, image));
  fixture.ops().serve_surface(5u, AvExportedImage{11, 0u, 8u, 8u, 32u, 0u, 0x34325241u, 0u, 0u}, 3u, 12);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK(fixture.plane().exports_surfaces());
  CHECK(!fixture.plane().export_surface(uuid, 5u, 0u, image));
  CHECK_STATUS(fixture.plane().start(uuid, capset()), Status::ok);
  CHECK(fixture.plane().export_surface(uuid, 5u, 0u, image));
  CHECK_EQ(image.fd, 11);
  CHECK_EQ(image.width, 8u);
  std::uint64_t writes = 0u;
  int           sync_fd = -1;
  CHECK(!fixture.plane().sync_surface(uuid, 6u, 0u, writes, sync_fd));
  CHECK(fixture.plane().sync_surface(uuid, 5u, 0u, writes, sync_fd));
  CHECK_EQ(writes, 3u);
  CHECK_EQ(sync_fd, 12); }

TEST(local_context_plane, names_the_domain_its_library_opened_and_composes_as_the_library_does)
{ Fixture fixture;
  CHECK_EQ(fixture.plane().memory_domain(), 0u);
  CHECK(!fixture.plane().composes());
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK_EQ(fixture.plane().memory_domain(), 0u);
  CHECK_STATUS(fixture.plane().start(uuid, capset()), Status::ok);
  CHECK(fixture.plane().memory_domain() != 0u);
  CHECK_EQ(fixture.plane().memory_domain(), fixture.ops().last_opened_domain());
  CHECK(!fixture.plane().composes());
  fixture.ops().editable().compose_planes = +[](void *, std::uint32_t, const AvScanoutPlane *, std::uint32_t,
                                                std::uint32_t, std::uint32_t, AvExportedImage *) -> std::uint32_t {
      return AV_EXECUTED;
  };
  CHECK(fixture.plane().composes()); }

TEST(local_context_plane, copies_surfaces_through_its_library_once_it_has_a_domain)
{ Fixture fixture;
  CHECK(!fixture.plane().copies_surfaces());
  const AvCopyRegion regions[1] = {{0u, 0u, 0u, 0u, 4u, 4u}};
  CHECK(!fixture.plane().copy_surface(uuid, av::SurfacePair(5u, 6u), regions, 1u, 50u));
  fixture.ops().serve_copy(5u, 6u, true);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK(fixture.plane().copies_surfaces());
  CHECK(!fixture.plane().copy_surface(uuid, av::SurfacePair(5u, 6u), regions, 1u, 50u));
  CHECK_STATUS(fixture.plane().start(uuid, capset()), Status::ok);
  CHECK(fixture.plane().copy_surface(uuid, av::SurfacePair(5u, 6u), regions, 1u, 50u));
  CHECK_EQ(fixture.ops().copy_wait(), 50u);
  CHECK(!fixture.plane().copy_surface(uuid, av::SurfacePair(5u, 7u), regions, 1u, 50u)); }

TEST(local_context_plane, reads_a_surface_texture_through_its_library_once_it_has_a_domain)
{ Fixture fixture;
  static std::uint32_t pixels[4];
  fixture.ops().serve_texture(5u, AvFrameView{reinterpret_cast<const std::uint8_t *>(pixels), 16u, 4u, 1u, 0u});
  AvFrameView view = {};
  CHECK(!fixture.plane().map_surface(uuid, 5u, 50u, view));
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK(!fixture.plane().map_surface(uuid, 5u, 50u, view));
  CHECK_STATUS(fixture.plane().start(uuid, capset()), Status::ok);
  CHECK(fixture.plane().map_surface(uuid, 5u, 50u, view));
  CHECK(!fixture.plane().map_surface(uuid, 6u, 50u, view));
  fixture.plane().unmap_surface(uuid, 5u);
  CHECK_EQ(fixture.ops().texture_unmaps(), 1u); }
