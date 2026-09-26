/* The contexts a device runs in QEMU's own process: every answer is the
 * table's or the library's, and a reset takes the shared objects with the
 * contexts. */
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "device/context/local_context_plane.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/memory/blob_table.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/context/local_context_plane_probe.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/ring/ring_blob_writer.hpp"
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
    Fixture() : blobs_(host_), interrupts_(host_), recorder_(host_), plane_(host_, blobs_, interrupts_, recorder_), ops_(vulkan_only)
    {
        host_.set_hostmem_size(0x10000u);
        static_cast<void>(blobs_.initialize());
        static_cast<void>(blobs_.create_host(ring, 0x2000u));
        static_cast<void>(blobs_.create_host(fence, 0x1000u));
        av::RingBlobWriter(blobs_).write_header(ring, 12u);
    }
    ~Fixture() { plane_.destroy_all(); }

    MockHost          &host() { return host_; }
    LocalContextPlane &plane() { return plane_; }
    MockExecutorOps   &ops() { return ops_; }

private:
    MockHost          host_;
    BlobTable         blobs_;
    FenceInterrupts   interrupts_;
    av::WireRecorder  recorder_;
    LocalContextPlane plane_;
    MockExecutorOps   ops_;
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

}  // namespace

/* What the library was given is what the capset is described from; no
 * library is a device with nothing to offer, and a table of another ABI is
 * refused. */
TEST(local_context_plane, answers_what_the_library_it_accepted_offers)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().accept(nullptr), Status::ok);
  CHECK_EQ(fixture.plane().sub_apis(), 0u);
  fixture.ops().editable().abi_version = 0u;
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::invalid_parameter);
  fixture.ops().editable().abi_version = RVK_EXECUTOR_ABI_VERSION;
  fixture.ops().set_import_kinds(RVK_BLOB_HOST_POINTER);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK_EQ(fixture.plane().sub_apis(), vulkan_only);
  CHECK_EQ(fixture.plane().import_kinds(av::SubApi::vulkan, uuid), static_cast<std::uint32_t>(RVK_BLOB_HOST_POINTER));
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::invalid_state); }

/* Either lock missing is no_memory; a second initialize after both were made
 * is not a failure. */
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

/* Create, find, bind and destroy are the table's, reached as the device
 * reaches them, through the ContextPlane; a bind for a context that is not
 * there is refused before any setup is looked at. */
TEST(local_context_plane, runs_contexts_through_its_table)
{ Fixture fixture;
  av::ContextPlane &plane = fixture.plane();
  CHECK_STATUS(plane.initialize(), Status::ok);
  CHECK_STATUS(plane.accept(&fixture.ops().table()), Status::ok);
  CHECK_STATUS(fixture.plane().create(0u), Status::invalid_parameter);
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
  CHECK_STATUS(fixture.plane().create(3u), Status::conflict);
  CHECK(fixture.plane().holds(3u));
  CHECK(!fixture.plane().holds(4u));
  CHECK_EQ(fixture.plane().live(), 1u);
  CHECK_STATUS(fixture.plane().bind(4u, setup(5u), capset()), Status::invalid_parameter);
  CHECK(!fixture.plane().holds_doorbell(5u));
  CHECK_STATUS(fixture.plane().bind(3u, setup(5u), capset()), Status::ok);
  CHECK(LocalContextPlaneProbe::table(fixture.plane()).find(3u)->binding().bound());
  CHECK(fixture.plane().holds_doorbell(5u));
  /* Vulkan is no OpenGL: nothing may share it. */
  CHECK_STATUS(fixture.plane().create(6u), Status::ok);
  CHECK(!fixture.plane().can_share(6u, 3u));
  CHECK(fixture.plane().invariants_hold());
  CHECK_STATUS(fixture.plane().destroy(3u), Status::ok);
  CHECK_STATUS(fixture.plane().destroy(3u), Status::not_found);
  CHECK(!fixture.plane().holds_doorbell(5u));
  CHECK_EQ(fixture.plane().live(), 1u); }

/* A reset leaves no context to hold a shared object, so the objects go with
 * the contexts. */
TEST(local_context_plane, destroy_all_takes_the_shared_objects_with_the_contexts)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().initialize(), Status::ok);
  CHECK_STATUS(fixture.plane().create(1u), Status::ok);
  CHECK_STATUS(fixture.plane().create(2u), Status::ok);
  const int descriptor = fixture.host().open_descriptor();
  CHECK_STATUS(LocalContextPlaneProbe::shared(fixture.plane())
                   .publish(7u, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 4096u, descriptor),
               Status::ok);
  CHECK_EQ(LocalContextPlaneProbe::shared(fixture.plane()).live(), 1u);
  fixture.plane().destroy_all();
  CHECK_EQ(fixture.plane().live(), 0u);
  CHECK_EQ(LocalContextPlaneProbe::shared(fixture.plane()).live(), 0u);
  CHECK_EQ(fixture.host().live_threads(), 0u); }

/* An image nothing published is no image, from either path. */
TEST(local_context_plane, an_image_nothing_published_is_neither_exported_nor_read)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().initialize(), Status::ok);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  RvkExportedImage image = {};
  CHECK(!fixture.plane().export_image(uuid, 9u, image));
  std::uint32_t pixels[4] = {};
  CHECK(!fixture.plane().read_image(uuid, 9u, pixels, 8u, 2u, 2u)); }

/* The shared objects' line, then the contexts'. */
TEST(local_context_plane, reports_its_shared_objects_then_its_contexts)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().initialize(), Status::ok);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  writer.open("device", 0u);
  fixture.plane().report(writer);
  CHECK(std::strstr(writer.dump(), "device/0/shared/0") != nullptr);
  CHECK(std::strstr(writer.dump(), "shared/0") < std::strstr(writer.dump(), "contexts/0 live=0")); }

/* Contexts in QEMU's own process read QEMU's own blob table and raise QEMU's
 * own interrupts, and there is no renderer to lose: the hooks a renderer
 * needs change nothing here. */
TEST(local_context_plane, the_renderers_hooks_change_nothing_in_process)
{ Fixture fixture;
  CHECK_STATUS(fixture.plane().initialize(), Status::ok);
  CHECK_STATUS(fixture.plane().accept(&fixture.ops().table()), Status::ok);
  CHECK_STATUS(fixture.plane().create(3u), Status::ok);
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
