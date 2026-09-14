/* One guest context's OpenCL on the host's own. As with Vulkan, the driver's
 * own answer is not asserted - these pass on a machine with no OpenCL at all,
 * where every command still decodes and the platform count is zero. */
#include "device/executor/imported_blob.hpp"
#include "device/executor/opencl_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::OpenClBackend;
using av::WireArena;
using av::WireReader;
using av::WireWriter;

/* This test calls the real OpenCL ICD loader, which allocates a table of every
 * platform it finds and never gives it back. Named rather than leak detection
 * being turned off, so a leak of this repository's own still fails. */
extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libnvidia\n"
           "leak:libdbus-1\n";
}

namespace {

constexpr std::size_t block_bytes = 65536u;

}  // namespace

TEST(opencl_backend, knows_the_ids_the_profile_carries_and_no_others)
{ OpenClBackend backend;
  CHECK(backend.knows(av::clproto::kCmd_clGetPlatformIDs));
  /* Inside OpenCL's range and not carried. */
  CHECK(!backend.knows(0x2FFFFu));
  /* Another API's range entirely, which the device should never deliver. */
  CHECK(!backend.knows(av::clproto::kCmd_clGetPlatformIDs ^ 0x30000u));
  CHECK(!backend.knows(0u)); }

TEST(opencl_backend, a_well_formed_command_decodes_and_answers)
{ OpenClBackend backend;
  std::uint8_t frame[64];
  WireWriter writer(frame, sizeof frame);
  /* clGetPlatformIDs(num_entries, platforms, num_platforms): ask for the
   * count only, which is the call every OpenCL program starts with. */
  writer.write_pod<std::uint32_t>(0u);
  writer.write_pod<std::uint8_t>(0u);
  writer.write_pod<std::uint8_t>(1u);

  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t reply[64] = {};
  WireReader payload(frame, writer.offset());
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::clproto::kCmd_clGetPlatformIDs, payload, answer, arena));
  CHECK_EQ(backend.ran(), 1u);
  CHECK(!backend.lost());
  backend.shut_down();
  CHECK_EQ(backend.ran(), 0u); }

TEST(opencl_backend, a_truncated_command_is_refused_at_every_length)
{ OpenClBackend backend;
  std::uint8_t frame[64];
  WireWriter writer(frame, sizeof frame);
  writer.write_pod<std::uint32_t>(0u);
  writer.write_pod<std::uint8_t>(0u);
  writer.write_pod<std::uint8_t>(1u);

  /* What the decoder actually reads, rather than what this test happened to
   * write: a frame longer than the command needs is a frame with trailing
   * bytes, which is the device's business and not the decoder's. */
  std::size_t consumed = 0u;
  { std::uint8_t block[block_bytes];
    WireArena arena(block, sizeof block);
    std::uint8_t reply[64] = {};
    WireReader payload(frame, writer.offset());
    WireWriter answer(reply, sizeof reply);
    CHECK(backend.run(av::clproto::kCmd_clGetPlatformIDs, payload, answer, arena));
    consumed = writer.offset() - payload.remaining(); }
  CHECK(consumed > 0u);

  for (std::size_t length = 0u; length < consumed; length++) {
      std::uint8_t block[block_bytes];
      WireArena arena(block, sizeof block);
      std::uint8_t reply[64] = {};
      WireReader payload(frame, length);
      WireWriter answer(reply, sizeof reply);
      CHECK(!backend.run(av::clproto::kCmd_clGetPlatformIDs, payload, answer, arena));
  }
  /* Only the one that decoded ran. */
  CHECK_EQ(backend.ran(), 1u);
  backend.shut_down(); }

/* Importing means clCreateBufferWithProperties with a dma-buf, and whether
 * that works is a per-device question answered where it is used. Until then a
 * blob is copied at the boundary, which is what import_kinds already says. */
TEST(opencl_backend, takes_no_blob_yet_and_says_so)
{ OpenClBackend backend;
  av::ImportedBlob blob;
  CHECK(!backend.adopt(blob));
  backend.abandon(blob);
  CHECK_EQ(blob.object(), 0u); }

namespace {

unsigned char host_objects[av::ClHandleRegistry::max_handles + 1u];

std::uint64_t wire_of(const void *object)
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(object));
}

}  // namespace

/* What the generated decoder asks before handing the ICD a handle: only what
 * a call made, as the kind it made, until its last release. */
TEST(opencl_backend, answers_the_decoder_only_for_objects_its_calls_made)
{ OpenClBackend backend;
  void *const queue = &host_objects[0];
  CHECK(backend.handle_for(av::clproto::HandleKind::command_queue, wire_of(queue)) == nullptr);
  backend.made(av::clproto::HandleKind::command_queue, queue);
  CHECK(backend.handle_for(av::clproto::HandleKind::command_queue, wire_of(queue)) == queue);
  CHECK(backend.handle_for(av::clproto::HandleKind::context, wire_of(queue)) == nullptr);
  backend.retained(av::clproto::HandleKind::command_queue, queue);
  backend.released(av::clproto::HandleKind::command_queue, queue);
  CHECK(backend.handle_for(av::clproto::HandleKind::command_queue, wire_of(queue)) == queue);
  backend.released(av::clproto::HandleKind::command_queue, queue);
  CHECK(backend.handle_for(av::clproto::HandleKind::command_queue, wire_of(queue)) == nullptr);
  CHECK(!backend.lost()); }

/* A registry with no room left would hand the guest a number the host then
 * refuses; the backend loses its device instead, which ends the context. */
TEST(opencl_backend, an_object_it_has_no_room_to_hold_loses_the_device)
{ OpenClBackend backend;
  for (std::size_t i = 0u; i < av::ClHandleRegistry::max_handles; i++)
      backend.made(av::clproto::HandleKind::event, &host_objects[i]);
  CHECK(!backend.lost());
  backend.made(av::clproto::HandleKind::event, &host_objects[av::ClHandleRegistry::max_handles]);
  CHECK(backend.lost());
  /* Given back first, because shutting down releases what is still held and
   * these are not objects any ICD made. */
  for (std::size_t i = 0u; i < av::ClHandleRegistry::max_handles; i++)
      backend.released(av::clproto::HandleKind::event, &host_objects[i]);
  backend.shut_down();
  CHECK(!backend.lost()); }

/* A context that dies holding an OpenCL context gives the ICD back every
 * reference the guest held on it, and nothing more: the ICD's own count is
 * what says so. The test holds a reference of its own so the context outlives
 * the shutdown and can be asked. A host with no OpenCL device has no context
 * to make, and this says nothing there. */
TEST(opencl_backend, shutting_down_releases_the_references_the_guest_still_held)
{ cl_platform_id platform = nullptr;
  cl_uint platforms = 0u;
  if (::clGetPlatformIDs(1u, &platform, &platforms) != CL_SUCCESS || platforms == 0u) return;
  cl_device_id device = nullptr;
  cl_uint devices = 0u;
  if (::clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1u, &device, &devices) != CL_SUCCESS ||
      devices == 0u)
      return;
  cl_int error = CL_SUCCESS;
  const cl_context context = ::clCreateContext(nullptr, 1u, &device, nullptr, nullptr, &error);
  if (context == nullptr) return;

  OpenClBackend backend;
  backend.made(av::clproto::HandleKind::context, context);
  /* The guest retained it once more, and the ICD took that reference. */
  CHECK_EQ(::clRetainContext(context), CL_SUCCESS);
  backend.retained(av::clproto::HandleKind::context, context);
  CHECK_EQ(::clRetainContext(context), CL_SUCCESS);

  backend.shut_down();
  cl_uint references = 0u;
  CHECK_EQ(::clGetContextInfo(context, CL_CONTEXT_REFERENCE_COUNT, sizeof references, &references,
                              nullptr),
           CL_SUCCESS);
  CHECK_EQ(references, 1u);
  CHECK(backend.handle_for(av::clproto::HandleKind::context, wire_of(context)) == nullptr);
  CHECK_EQ(::clReleaseContext(context), CL_SUCCESS); }
