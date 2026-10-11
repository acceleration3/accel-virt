/* What the export socket answers of the GPU, decided before a byte is sent:
 * nothing is exported until realize attached the contexts, or where they
 * export nothing; what the executor exports is passed on with its descriptor
 * only when it describes a picture and came with one, and a descriptor with
 * no picture is closed rather than handed to a process outside QEMU. */
#include "device/export/exported_gpu.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/export/export_request.hpp"

#include <cstdint>
#include <sys/stat.h>
#include <unistd.h>

using av::ExportAnswer;
using av::ExportedGpu;
using av::ExportGreeting;
using av::ExportOperation;
using av::ExportRequest;
using av::Status;

namespace {

const std::uint8_t     gpu[ExportGreeting::uuid_bytes] = {0xA1u, 0xB2u};
const AvExportedImage  picture = {-1, 0u, 640u, 480u, 2560u, 0u, 0x34325241u, 0u, 0x00FFFFFFFFFFFFFFull};
constexpr std::uint64_t surface = 0x77u;

ExportRequest asked(ExportOperation operation)
{
    return ExportRequest(operation, operation == ExportOperation::hello ? 0u : surface, 5u);
}

}  // namespace

TEST(exported_gpu, exports_nothing_before_realize_or_where_the_contexts_export_nothing)
{ av::MockHost host;
  av::RecordingContextPlane plane;
  ExportedGpu exported(host);
  int descriptor = 7;
  CHECK_STATUS(exported.greet(asked(ExportOperation::hello)).status(), Status::invalid_state);
  CHECK_STATUS(exported.export_surface(asked(ExportOperation::export_surface), descriptor).status(),
               Status::invalid_state);
  CHECK_EQ(descriptor, -1);
  descriptor = 7;
  CHECK_STATUS(exported.sync(asked(ExportOperation::sync), descriptor).status(), Status::invalid_state);
  CHECK_EQ(descriptor, -1);
  exported.attach(plane, gpu, ExportGreeting(gpu));
  CHECK_STATUS(exported.greet(asked(ExportOperation::hello)).status(), Status::invalid_state);
  CHECK_STATUS(exported.export_surface(asked(ExportOperation::export_surface), descriptor).status(),
               Status::invalid_state);
  CHECK_EQ(plane.exports_asked(), 0u); }

TEST(exported_gpu, hello_names_the_gpu_it_was_attached_with)
{ av::MockHost host;
  av::RecordingContextPlane plane;
  plane.export_as(surface, picture, 0u);
  ExportedGpu exported(host);
  exported.attach(plane, gpu, ExportGreeting(gpu, 0u, 3u, 0u, 0u));
  const ExportAnswer answer = exported.greet(asked(ExportOperation::hello));
  CHECK_STATUS(answer.status(), Status::ok);
  CHECK_EQ(answer.tag(), 5u);
  CHECK_EQ(answer.greeting().uuid()[1], 0xB2u);
  CHECK_EQ(answer.greeting().bus(), 3u);
  CHECK(answer.greeting().valid()); }

/* The executor's own answer, asked by the GPU's UUID, with its descriptor. */
TEST(exported_gpu, an_exported_surface_is_answered_with_its_layout_and_its_dma_buf)
{ av::MockHost host;
  av::RecordingContextPlane plane;
  plane.export_as(surface, picture, 0u);
  ExportedGpu exported(host);
  exported.attach(plane, gpu, ExportGreeting(gpu));
  int descriptor = -1;
  const ExportAnswer answer = exported.export_surface(asked(ExportOperation::export_surface), descriptor);
  CHECK_STATUS(answer.status(), Status::ok);
  CHECK(plane.export_gpu() == gpu);
  CHECK_EQ(answer.surface(), surface);
  CHECK_EQ(answer.layout().width(), 640u);
  CHECK_EQ(answer.layout().stride(), 2560u);
  CHECK_EQ(answer.layout().modifier(), 0x00FFFFFFFFFFFFFFull);
  CHECK_EQ(answer.descriptors(), 1u);
  struct stat file = {};
  CHECK(descriptor >= 0 && ::fstat(descriptor, &file) == 0 && file.st_size == 4096);
  ::close(descriptor);
  const ExportRequest other(ExportOperation::export_surface, surface + 1u, 5u);
  CHECK_STATUS(exported.export_surface(other, descriptor).status(), Status::not_found);
  CHECK_EQ(descriptor, -1);
  CHECK_EQ(host.exports_closed(), 0u); }

/* An executor that answers a picture with no size, or a stride narrower than
 * it, is not believed: its descriptor is closed here. One that answers no
 * descriptor at all has nothing to close. */
TEST(exported_gpu, an_export_with_no_picture_or_no_dma_buf_is_refused_and_its_descriptor_closed)
{ av::MockHost host;
  av::RecordingContextPlane plane;
  AvExportedImage narrow = picture;
  narrow.stride = 639u;
  plane.export_as(surface, narrow, 0u);
  ExportedGpu exported(host);
  exported.attach(plane, gpu, ExportGreeting(gpu));
  int descriptor = 9;
  CHECK_STATUS(exported.export_surface(asked(ExportOperation::export_surface), descriptor).status(), Status::not_found);
  CHECK_EQ(descriptor, -1);
  CHECK_EQ(host.exports_closed(), 1u);
  CHECK(host.logged() >= 1u);
  AvExportedImage empty = picture;
  empty.height = 0u;
  plane.export_as(surface, empty, 0u);
  CHECK_STATUS(exported.export_surface(asked(ExportOperation::export_surface), descriptor).status(), Status::not_found);
  CHECK_EQ(host.exports_closed(), 2u);
  plane.export_as(surface, picture, 0u);
  plane.export_without_descriptor();
  CHECK_STATUS(exported.export_surface(asked(ExportOperation::export_surface), descriptor).status(), Status::not_found);
  CHECK_EQ(descriptor, -1);
  CHECK_EQ(host.exports_closed(), 2u); }

TEST(exported_gpu, a_sync_is_answered_with_the_writes_and_a_sync_file_or_refused_without_one)
{ av::MockHost host;
  av::RecordingContextPlane plane;
  plane.export_as(surface, picture, 12u);
  ExportedGpu exported(host);
  exported.attach(plane, gpu, ExportGreeting(gpu));
  int descriptor = -1;
  const ExportAnswer answer = exported.sync(asked(ExportOperation::sync), descriptor);
  CHECK_STATUS(answer.status(), Status::ok);
  CHECK_EQ(answer.writes(), 12u);
  CHECK(descriptor >= 0);
  ::close(descriptor);
  const ExportRequest other(ExportOperation::sync, surface + 1u, 5u);
  CHECK_STATUS(exported.sync(other, descriptor).status(), Status::not_found);
  CHECK_EQ(descriptor, -1);
  plane.sync_without_descriptor();
  const ExportAnswer bare = exported.sync(asked(ExportOperation::sync), descriptor);
  CHECK_STATUS(bare.status(), Status::not_found);
  CHECK_EQ(bare.writes(), 0u);
  CHECK_EQ(descriptor, -1);
  CHECK_EQ(plane.syncs_asked(), 3u); }
