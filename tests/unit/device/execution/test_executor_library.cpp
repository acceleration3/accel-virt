/* The table an executor library hands over: accepted only whole and of this
 * ABI, what it offers limited to what the protocol knows, and every call the
 * device makes of it named by the domain this device opened. */
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/executor_library.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/surface_point.hpp"

#include <cstdint>

using av::ExecutorLibrary;
using av::MockExecutorOps;
using av::Status;
using av::SubApi;
using av::SurfacePoint;

namespace {

constexpr std::uint8_t some_gpu[16] = {0x42, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

/* A library accepted, with this device's domain open in it. */
class Opened {
public:
    explicit Opened(std::uint32_t sub_apis) : ops(sub_apis)
    {
        CHECK_STATUS(library.accept(&ops.table()), Status::ok);
        CHECK_STATUS(library.open_domain(some_gpu), Status::ok);
    }

    MockExecutorOps ops;
    ExecutorLibrary library;
};

}  // namespace

TEST(executor_library_accept, a_whole_table_of_this_abi_is_available)
{ MockExecutorOps ops(AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL);
  ExecutorLibrary library;
  CHECK(!library.available());
  CHECK_EQ(library.sub_apis(), 0u);
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK(library.available());
  CHECK_EQ(library.sub_apis(), AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL);
  CHECK_STATUS(library.accept(&ops.table()), Status::invalid_state); }

TEST(executor_library_accept, no_table_leaves_the_device_without_an_executor)
{ ExecutorLibrary library;
  CHECK_STATUS(library.accept(nullptr), Status::ok);
  CHECK(!library.available());
  CHECK_EQ(library.sub_apis(), 0u);
  CHECK_EQ(library.import_kinds(SubApi::vulkan, some_gpu), 0u);
  /* Nothing to open a domain in, which is not a failure: nothing is shared. */
  CHECK_STATUS(library.open_domain(some_gpu), Status::ok);
  CHECK_EQ(library.domain(), 0u);
  /* A context's worker may still ask for one; it is told there is none. */
  CHECK(library.create(SubApi::vulkan, AvContextOptions{}, nullptr) == nullptr);
  library.close_domain(); }

/* The device and the library are built together: a table of another version,
 * or one missing any operation, is a build to fix, not one to serve. */
TEST(executor_library_accept, another_abi_version_or_any_missing_operation_is_refused)
{ for (std::uint32_t broken = 0u; broken <= 24u; broken++) {
      MockExecutorOps ops(AV_EXECUTOR_SUB_API_VULKAN);
      AvExecutorOps  &table = ops.editable();
      switch (broken) {
      case 0u: table.abi_version = AV_EXECUTOR_ABI_VERSION - 1u; break;
      case 1u: table.sub_apis = nullptr; break;
      case 2u: table.import_kinds = nullptr; break;
      case 3u: table.open_domain = nullptr; break;
      case 4u: table.close_domain = nullptr; break;
      case 5u: table.create = nullptr; break;
      case 6u: table.destroy = nullptr; break;
      case 7u: table.execute = nullptr; break;
      case 8u: table.end_batch = nullptr; break;
      case 9u: table.poll = nullptr; break;
      case 10u: table.gpu_progress = nullptr; break;
      case 11u: table.set_signal = nullptr; break;
      case 12u: table.import_blob = nullptr; break;
      case 13u: table.release_blob = nullptr; break;
      case 14u: table.imported_handle = nullptr; break;
      case 15u: table.share_object = nullptr; break;
      case 16u: table.open_object = nullptr; break;
      case 17u: table.close_object = nullptr; break;
      case 18u: table.release_shared = nullptr; break;
      case 19u: table.read_image = nullptr; break;
      case 20u: table.export_image = nullptr; break;
      case 21u: table.map_frame = nullptr; break;
      case 22u: table.unmap_frame = nullptr; break;
      case 23u: table.export_surface = nullptr; break;
      default: table.sync_surface = nullptr; break;
      }
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&table), Status::invalid_parameter);
      CHECK(!library.available());
  } }

TEST(executor_library_sub_apis, bits_the_protocol_does_not_know_are_not_offered)
{ MockExecutorOps ops(0xFFFFFFFFu);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK_EQ(library.sub_apis(), 15u); }

TEST(executor_library_import_kinds, come_from_the_library_for_one_api_and_one_gpu_and_only_known_kinds)
{ MockExecutorOps ops(AV_EXECUTOR_SUB_API_VULKAN);
  ops.set_import_kinds(AV_BLOB_HOST_POINTER | AV_BLOB_DMA_BUF | 0x40u);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK_EQ(library.import_kinds(SubApi::vulkan, some_gpu), std::uint32_t{AV_BLOB_HOST_POINTER | AV_BLOB_DMA_BUF}); }

/* ------------------------------------------------------------- domain */

TEST(executor_library_domain, is_opened_on_the_device_s_gpu_once_and_closed_once)
{ MockExecutorOps ops(AV_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK_EQ(library.domain(), 0u);
  library.close_domain();              /* none open: nothing to tell the library */
  CHECK_EQ(ops.domains_closed(), 0u);
  CHECK_STATUS(library.open_domain(some_gpu), Status::ok);
  CHECK(library.domain() != 0u);
  CHECK_EQ(ops.last_domain_gpu(), 0x42u);
  CHECK_STATUS(library.open_domain(some_gpu), Status::invalid_state);
  CHECK_EQ(ops.domains_opened(), 1u);
  const std::uint32_t opened = library.domain();
  library.close_domain();
  CHECK_EQ(ops.domains_closed(), 1u);
  CHECK_EQ(ops.last_closed_domain(), opened);
  CHECK_EQ(library.domain(), 0u);
  library.close_domain();
  CHECK_EQ(ops.domains_closed(), 1u); }

TEST(executor_library_domain, a_gpu_the_library_has_not_got_opens_none)
{ MockExecutorOps ops(AV_EXECUTOR_SUB_API_VULKAN);
  ops.refuse_domain_at(1u);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK_STATUS(library.open_domain(some_gpu), Status::not_found);
  CHECK_EQ(library.domain(), 0u);
  CHECK(library.create(SubApi::vulkan, AvContextOptions{}, nullptr) == nullptr);
  CHECK_EQ(ops.option_creates(), 0u); }

/* ---------------------------------------------------------- executors */

/* The domain goes with every create, whatever the device filled in. */
TEST(executor_library_create, makes_an_executor_in_the_device_s_domain_with_its_options_and_share)
{ Opened opened(AV_EXECUTOR_SUB_API_OPENGL);
  AvExecutor *const first = opened.library.create(SubApi::opengl, AvContextOptions{}, nullptr);
  CHECK(first != nullptr);
  const AvContextOptions asked = {4u, 6u, AV_CONTEXT_PROFILE_CORE, AV_CONTEXT_FLAG_DEBUG, 0x77u};
  AvExecutor *const second = opened.library.create(SubApi::opengl, asked, first);
  CHECK(second != nullptr);
  const AvContextOptions seen = opened.ops.last_options();
  CHECK_EQ(seen.major, 4u);
  CHECK_EQ(seen.minor, 6u);
  CHECK_EQ(seen.profile, std::uint32_t{AV_CONTEXT_PROFILE_CORE});
  CHECK_EQ(seen.flags, std::uint32_t{AV_CONTEXT_FLAG_DEBUG});
  CHECK_EQ(seen.domain, opened.library.domain());
  CHECK(opened.ops.last_share() == first);
  CHECK(opened.ops.last_share_live());
  CHECK_EQ(opened.ops.last_sub_api(), std::uint32_t{AV_EXECUTOR_SUB_API_OPENGL});
  opened.library.destroy(second);
  opened.library.destroy(first);
  CHECK_EQ(opened.ops.destroyed(), 2u);
  CHECK_EQ(opened.ops.misuses(), 0u); }

TEST(executor_library_execute, forwards_a_command_and_its_answer)
{ Opened opened(AV_EXECUTOR_SUB_API_VULKAN);
  AvExecutor *const executor = opened.library.create(SubApi::vulkan, AvContextOptions{}, nullptr);
  const std::uint8_t payload[3] = {7u, 8u, 9u};
  std::uint8_t       reply[8] = {};
  std::size_t        produced = 0u;
  CHECK_EQ(opened.library.execute(executor, 0x10001u, payload, sizeof payload, reply, sizeof reply, produced),
           std::uint32_t{AV_EXECUTED});
  CHECK_EQ(produced, sizeof payload);
  CHECK_EQ(reply[2], 9u);
  opened.library.end_batch(executor);
  CHECK(!opened.library.poll(executor));
  std::uint64_t marked = 5u, reached = 5u;
  opened.library.gpu_progress(executor, marked, reached);
  CHECK_EQ(marked, 0u);
  CHECK_EQ(reached, 0u);
  opened.library.destroy(executor);
  CHECK_EQ(opened.ops.misuses(), 0u); }

TEST(executor_library_import_blob, forwards_the_import_its_release_and_the_handle_it_made)
{ std::uint8_t page[4096] = {};
  Opened       opened(AV_EXECUTOR_SUB_API_VULKAN);
  AvExecutor *const executor = opened.library.create(SubApi::vulkan, AvContextOptions{}, nullptr);
  AvBlobHandle handle = {};
  handle.kind = AV_BLOB_HOST_POINTER;
  handle.reserved = 4u;
  handle.pointer = page;
  handle.fd = -1;
  handle.size = sizeof page;
  CHECK_EQ(opened.library.imported_handle(executor, 0x21u), 0u);
  CHECK_EQ(opened.library.import_blob(executor, 0x21u, handle), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(opened.library.imported_handle(executor, 0x21u), 0x21u | MockExecutorOps::handle_bit);
  AvBlobHandle seen = {};
  CHECK(opened.ops.import_of(0x21u, seen));
  CHECK(seen.pointer == page);
  CHECK_EQ(seen.reserved, 4u);
  opened.library.release_blob(executor, 0x21u);
  CHECK_EQ(opened.library.imported_handle(executor, 0x21u), 0u);
  CHECK_EQ(opened.ops.released(), 1u);
  opened.library.destroy(executor);
  CHECK_EQ(opened.ops.misuses(), 0u); }

TEST(executor_library_signal, an_executor_is_given_one_and_the_library_calls_it)
{ Opened opened(AV_EXECUTOR_SUB_API_OPENCL);
  AvExecutor *const executor = opened.library.create(SubApi::opencl, AvContextOptions{}, nullptr);
  static std::uint32_t woken = 0u;
  woken = 0u;
  opened.library.set_signal(executor, [](void *) { ++woken; }, nullptr);
  CHECK_EQ(opened.ops.signals_set(), 1u);
  CHECK(opened.ops.fire_signal());
  CHECK_EQ(woken, 1u);
  opened.library.set_signal(nullptr, [](void *) {}, nullptr);   /* no executor: nothing given */
  CHECK_EQ(opened.ops.signals_set(), 1u);
  opened.library.destroy(executor);
  CHECK_EQ(opened.ops.misuses(), 0u); }

/* ----------------------------------------------------- shared objects */

TEST(executor_library_shared, shares_opens_and_closes_through_the_library)
{ Opened opened(AV_EXECUTOR_SUB_API_VULKAN);
  AvExecutor *const maker = opened.library.create(SubApi::vulkan, AvContextOptions{}, nullptr);
  AvExecutor *const reader = opened.library.create(SubApi::vulkan, AvContextOptions{}, nullptr);
  CHECK_EQ(opened.library.share_object(maker, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x40u), std::uint32_t{AV_SHARED_DONE});
  std::uint64_t made = 0u;
  CHECK_EQ(opened.library.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, made), std::uint32_t{AV_SHARED_DONE});
  CHECK(made != 0u);
  std::uint64_t held = 0u;
  CHECK(opened.ops.opened_as(reader, 0x2Au, held));
  CHECK_EQ(held, made);
  CHECK_EQ(opened.library.open_object(reader, 0x2Bu, AV_SHARED_MEMORY, 4096u, made), std::uint32_t{AV_SHARED_UNKNOWN});
  CHECK_EQ(made, 0u);
  opened.library.close_object(reader, 0x2Au);
  CHECK(!opened.ops.opened_as(reader, 0x2Au, held));
  CHECK_EQ(opened.ops.closed_objects(), 1u);
  opened.library.destroy(reader);
  opened.library.destroy(maker);
  CHECK_EQ(opened.ops.misuses(), 0u); }

/* A library that opened and cannot say what it made gave the guest nothing to
 * name it by: the device refuses the open and closes what was made. */
TEST(executor_library_shared, an_open_that_names_nothing_is_refused_and_closed)
{ Opened opened(AV_EXECUTOR_SUB_API_VULKAN);
  AvExecutor *const maker = opened.library.create(SubApi::vulkan, AvContextOptions{}, nullptr);
  AvExecutor *const reader = opened.library.create(SubApi::vulkan, AvContextOptions{}, nullptr);
  CHECK_EQ(opened.library.share_object(maker, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x40u), std::uint32_t{AV_SHARED_DONE});
  opened.ops.withhold_open_handle_at(1u);
  std::uint64_t made = 7u;
  CHECK_EQ(opened.library.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, made), std::uint32_t{AV_SHARED_REFUSED});
  CHECK_EQ(made, 0u);
  CHECK_EQ(opened.ops.closed_objects(), 1u);
  opened.library.destroy(reader);
  opened.library.destroy(maker); }

TEST(executor_library_release_shared, tells_the_library_in_the_device_s_domain_only_with_one_open)
{ MockExecutorOps ops(AV_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  library.release_shared(0x2Au);       /* no domain yet */
  CHECK_EQ(ops.shared_releases(), 0u);
  CHECK_STATUS(library.open_domain(some_gpu), Status::ok);
  library.release_shared(0x2Au);
  library.release_shared(0u);
  CHECK_EQ(ops.shared_releases(), 2u);
  CHECK_EQ(ops.last_shared_release(), 0u);
  CHECK_EQ(ops.last_domain(), library.domain()); }

/* ------------------------------------------------- images and frames */

TEST(executor_library_images, are_asked_in_the_device_s_domain_and_nothing_without_one)
{ MockExecutorOps ops(AV_EXECUTOR_SUB_API_D3D);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  AvExportedImage image = {};
  std::uint8_t    pixels[16] = {};
  CHECK(!library.export_image(9u, image));
  CHECK(!library.read_image(9u, pixels, 8u, 2u, 2u));
  CHECK_EQ(ops.last_domain(), 0u);
  CHECK_STATUS(library.open_domain(some_gpu), Status::ok);
  CHECK(!library.export_image(9u, image));   /* the mock knows no key */
  CHECK_EQ(ops.last_domain(), library.domain());
  CHECK(!library.read_image(9u, pixels, 8u, 2u, 2u));
  /* A library that has the image answers it, in the device's domain. */
  ops.editable().read_image = [](void *, std::uint32_t domain, std::uint64_t key, void *into, std::uint32_t,
                                 std::uint32_t, std::uint32_t) -> std::uint32_t {
      if (domain == 0u || key != 9u) return AV_UNKNOWN;
      static_cast<std::uint8_t *>(into)[0] = 0x5Au;
      return AV_EXECUTED;
  };
  ops.editable().export_image = [](void *, std::uint32_t domain, std::uint64_t key,
                                   AvExportedImage *exported) -> std::uint32_t {
      if (domain == 0u || key != 9u) return AV_UNKNOWN;
      exported->width = 64u;
      return AV_EXECUTED;
  };
  CHECK(library.read_image(9u, pixels, 8u, 2u, 2u));
  CHECK_EQ(pixels[0], 0x5Au);
  CHECK(library.export_image(9u, image));
  CHECK_EQ(image.width, 64u); }

TEST(executor_library_frames, are_mapped_and_unmapped_in_the_device_s_domain)
{ MockExecutorOps ops(AV_EXECUTOR_SUB_API_D3D);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  std::uint8_t      pixels[16] = {};
  const AvFrameView served = {pixels, 8u, 2u, 2u, 0u};
  ops.serve_frame(0x51u, 3u, served);
  AvFrameView view = {};
  CHECK(!library.map_frame(SurfacePoint(0x51u, 3u), 0u, view));   /* no domain */
  library.unmap_frame(SurfacePoint(0x51u, 3u));
  CHECK_EQ(ops.frame_unmaps(), 0u);
  CHECK_STATUS(library.open_domain(some_gpu), Status::ok);
  CHECK(library.map_frame(SurfacePoint(0x51u, 3u), 0u, view));
  CHECK(view.pixels == pixels);
  CHECK_EQ(ops.last_domain(), library.domain());
  CHECK(!library.map_frame(SurfacePoint(0x51u, 4u), 0u, view));
  library.unmap_frame(SurfacePoint(0x51u, 3u));
  CHECK_EQ(ops.frame_unmaps(), 1u); }

/* What a library hands over is believed only as far as AV_EXECUTED. */
TEST(executor_library_surfaces, are_exported_and_synced_in_the_device_s_domain)
{ MockExecutorOps ops(AV_EXECUTOR_SUB_API_D3D);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  const AvExportedImage served = {5, 0u, 64u, 32u, 256u, 0u, 0x34325241u, 0u, AV_EXPORT_MODIFIER_IMPLICIT};
  ops.serve_surface(0x51u, served, 3u, 6);
  AvExportedImage image = {9, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
  std::uint64_t   writes = 9u;
  int             fd = 9;
  CHECK(!library.export_surface(0x51u, image));   /* no domain */
  CHECK_EQ(image.fd, -1);
  CHECK(!library.sync_surface(0x51u, writes, fd));
  CHECK_EQ(writes, 0u);
  CHECK_EQ(fd, -1);
  CHECK_STATUS(library.open_domain(some_gpu), Status::ok);
  CHECK(library.export_surface(0x51u, image));
  CHECK_EQ(image.fd, 5);
  CHECK_EQ(image.width, 64u);
  CHECK(library.sync_surface(0x51u, writes, fd));
  CHECK_EQ(writes, 3u);
  CHECK_EQ(fd, 6);
  CHECK_EQ(ops.last_domain(), library.domain());
  CHECK(!library.export_surface(0x52u, image));
  CHECK_EQ(image.fd, -1);
  CHECK(!library.sync_surface(0x52u, writes, fd));
  CHECK_EQ(fd, -1); }
