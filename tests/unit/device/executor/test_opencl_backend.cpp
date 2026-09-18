/* One guest context's OpenCL on the host's own. As with Vulkan, the driver's
 * own answer is not asserted - these pass on a machine with no OpenCL at all,
 * where every command still decodes and the platform count is zero. */
#include "device/executor/imported_blob.hpp"
#include "device/abi/rvk_executor.h"
#include "device/executor/opencl_backend.hpp"
#include "device/shim/loader_host_apis.hpp"
#include "device/shim/opencl_device.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <CL/cl_ext.h>
#include <cstdint>
#include <cstring>

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
           "leak:libvulkan\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:iris_dri\n"
           "leak:libglapi\n"
           "leak:libz\n"
           "leak:nvidia_icd\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libnvidia\n"
           "leak:libdbus-1\n";
}

namespace {

constexpr std::size_t block_bytes = 65536u;

/* Stands in for the device's wake. Nothing here completes on another thread,
 * so it is never called; what it proves is that binding one is what lets a
 * registration be attempted at all. */
void nothing_to_wake(void *) {}

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

namespace {

/* The OpenCL device the host kept for the first GPU it says runs OpenCL, and
 * that GPU's UUID. Null on a host with none, where the tests using it judge
 * nothing and say so by returning. */
class HostOpenCl {
public:
    HostOpenCl() : device_(nullptr), uuid_{}
    { host_.open();
      for (std::uint32_t i = 0u; i < host_.gpu_count() && device_ == nullptr; i++) {
          device_ = host_.opencl_device(i);
          CHECK_EQ(device_ != nullptr, (host_.sub_apis(i) & RVK_EXECUTOR_SUB_API_OPENCL) != 0u);
          host_.gpu_uuid(i, uuid_);
      } }
    ~HostOpenCl() { host_.close(); }

    const av::OpenClDevice *device() const { return device_; }
    const std::uint8_t     *uuid() const { return uuid_; }
    av::LoaderHostApis     &host() { return host_; }

private:
    av::LoaderHostApis      host_;
    const av::OpenClDevice *device_;
    std::uint8_t            uuid_[16];
};

cl_platform_id other_platform() { return reinterpret_cast<cl_platform_id>(&host_objects[1]); }

}  // namespace

/* A context the host found no device for is not one: it is refused at setup,
 * and until it is bound it is shown no platform and no device, and makes no
 * context from any type. */
TEST(opencl_backend, unbound_it_offers_no_platform_device_or_context)
{ OpenClBackend backend;
  CHECK(!backend.initialize(nullptr));
  cl_platform_id platforms[2] = {other_platform(), other_platform()};
  cl_uint count = 7u;
  CHECK_EQ(backend.on_clGetPlatformIDs(2u, platforms, &count), CL_PLATFORM_NOT_FOUND_KHR);
  CHECK_EQ(count, 0u);
  CHECK(platforms[0] == other_platform());
  count = 7u;
  CHECK_EQ(backend.on_clGetPlatformIDs(0u, nullptr, &count), CL_PLATFORM_NOT_FOUND_KHR);
  CHECK_EQ(count, 0u);
  cl_device_id devices[2] = {};
  CHECK_EQ(backend.on_clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, 2u, devices, &count), CL_INVALID_PLATFORM);
  cl_int error = CL_SUCCESS;
  CHECK(backend.on_clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PLATFORM);
  CHECK(backend.on_clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr) == nullptr); }

/* The argument combinations OpenCL itself forbids are CL_INVALID_VALUE before
 * anything is looked at: an array with no room, and nowhere at all to answer. */
TEST(opencl_backend, lists_asked_for_nowhere_are_invalid_values)
{ OpenClBackend backend;
  cl_platform_id platforms[1] = {};
  cl_uint count = 0u;
  CHECK_EQ(backend.on_clGetPlatformIDs(0u, platforms, &count), CL_INVALID_VALUE);
  CHECK_EQ(backend.on_clGetPlatformIDs(1u, nullptr, nullptr), CL_INVALID_VALUE);
  cl_device_id devices[1] = {};
  CHECK_EQ(backend.on_clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, 0u, devices, &count), CL_INVALID_VALUE);
  CHECK_EQ(backend.on_clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, 1u, nullptr, nullptr), CL_INVALID_VALUE); }

/* Bound, a context is shown its GPU's platform and device and nothing else,
 * whatever else the host has - this host has three platforms and a CPU runtime
 * beside two GPUs. The device it is shown is the GPU, by its own UUID. */
TEST(opencl_backend, bound_it_offers_its_gpu_s_platform_and_device_alone)
{ HostOpenCl host;
  if (host.device() == nullptr) return;
  OpenClBackend backend;
  CHECK(backend.initialize(host.device()));
  const cl_platform_id platform = static_cast<cl_platform_id>(host.device()->platform());
  const cl_device_id   device = static_cast<cl_device_id>(host.device()->device());

  cl_uint count = 0u;
  CHECK_EQ(backend.on_clGetPlatformIDs(0u, nullptr, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  cl_platform_id platforms[3] = {other_platform(), other_platform(), other_platform()};
  CHECK_EQ(backend.on_clGetPlatformIDs(3u, platforms, nullptr), CL_SUCCESS);
  CHECK(platforms[0] == platform);
  CHECK(platforms[1] == other_platform());

  const cl_device_type offered[] = {CL_DEVICE_TYPE_GPU, CL_DEVICE_TYPE_ALL, CL_DEVICE_TYPE_DEFAULT,
                                    CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_CPU};
  for (const cl_device_type type : offered) {
      cl_device_id devices[3] = {};
      count = 7u;
      CHECK_EQ(backend.on_clGetDeviceIDs(platform, type, 3u, devices, &count), CL_SUCCESS);
      CHECK_EQ(count, 1u);
      CHECK(devices[0] == device);
      CHECK(devices[1] == nullptr);
  }
  count = 7u;
  CHECK_EQ(backend.on_clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0u, nullptr, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  cl_device_id devices[1] = {};
  CHECK_EQ(backend.on_clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 1u, devices, nullptr), CL_SUCCESS);
  CHECK(devices[0] == device);
  std::uint8_t uuid[16] = {};
  CHECK_EQ(::clGetDeviceInfo(devices[0], CL_DEVICE_UUID_KHR, sizeof uuid, uuid, nullptr), CL_SUCCESS);
  CHECK(std::memcmp(uuid, host.uuid(), sizeof uuid) == 0);

  const cl_device_type absent[] = {CL_DEVICE_TYPE_CPU, CL_DEVICE_TYPE_ACCELERATOR, CL_DEVICE_TYPE_CUSTOM};
  for (const cl_device_type type : absent) {
      devices[0] = nullptr;
      count = 7u;
      CHECK_EQ(backend.on_clGetDeviceIDs(platform, type, 1u, devices, &count), CL_DEVICE_NOT_FOUND);
      CHECK_EQ(count, 0u);
      CHECK(devices[0] == nullptr);
  }
  CHECK_EQ(backend.on_clGetDeviceIDs(platform, 0u, 1u, devices, &count), CL_INVALID_DEVICE_TYPE);
  CHECK_EQ(backend.on_clGetDeviceIDs(platform, cl_device_type{1} << 40u, 1u, devices, &count),
           CL_INVALID_DEVICE_TYPE);
  CHECK_EQ(backend.on_clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1u, devices, nullptr), CL_SUCCESS);
  CHECK_EQ(backend.on_clGetDeviceIDs(other_platform(), CL_DEVICE_TYPE_GPU, 1u, devices, &count),
           CL_INVALID_PLATFORM);

  backend.shut_down();
  CHECK_EQ(backend.on_clGetPlatformIDs(0u, nullptr, &count), CL_PLATFORM_NOT_FOUND_KHR); }

/* A context from a type is made on the bound device alone, even where the
 * platform has other devices of that type, and a type the device is not finds
 * nothing. */
TEST(opencl_backend, a_context_from_a_type_holds_the_bound_device_alone)
{ HostOpenCl host;
  if (host.device() == nullptr) return;
  OpenClBackend backend;
  CHECK(backend.initialize(host.device()));
  cl_int error = CL_INVALID_VALUE;
  const cl_context context = backend.on_clCreateContextFromType(nullptr, CL_DEVICE_TYPE_ALL, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(context != nullptr);
  if (context != nullptr) {
      cl_uint devices = 0u;
      CHECK_EQ(::clGetContextInfo(context, CL_CONTEXT_NUM_DEVICES, sizeof devices, &devices, nullptr), CL_SUCCESS);
      CHECK_EQ(devices, 1u);
      cl_device_id device = nullptr;
      CHECK_EQ(::clGetContextInfo(context, CL_CONTEXT_DEVICES, sizeof device, &device, nullptr), CL_SUCCESS);
      CHECK(device == static_cast<cl_device_id>(host.device()->device()));
      CHECK_EQ(::clReleaseContext(context), CL_SUCCESS);
  }
  error = CL_SUCCESS;
  CHECK(backend.on_clCreateContextFromType(nullptr, CL_DEVICE_TYPE_CPU, &error) == nullptr);
  CHECK_EQ(error, CL_DEVICE_NOT_FOUND);
  backend.shut_down(); }

/* Through the wire, the platform a guest is answered is the one the backend
 * then accepts back; a platform the host has and the guest was never shown is
 * refused when named. */
TEST(opencl_backend, the_guest_can_name_only_the_platform_it_was_shown)
{ HostOpenCl host;
  if (host.device() == nullptr) return;
  OpenClBackend backend;
  CHECK(backend.initialize(host.device()));
  std::uint8_t frame[16];
  WireWriter writer(frame, sizeof frame);
  writer.write_varint<std::uint32_t>(4u);
  writer.write_pod<std::uint8_t>(1u);
  writer.write_pod<std::uint8_t>(1u);
  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t reply[64] = {};
  WireReader payload(frame, writer.offset());
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::clproto::kCmd_clGetPlatformIDs, payload, answer, arena));
  const void *const platform = host.device()->platform();
  CHECK(backend.handle_for(av::clproto::HandleKind::platform, wire_of(platform)) == platform);

  cl_platform_id every[16] = {};
  cl_uint listed = 0u;
  if (::clGetPlatformIDs(16u, every, &listed) == CL_SUCCESS) {
      for (cl_uint i = 0u; i < listed && i < 16u; i++) {
          if (every[i] == platform) continue;
          CHECK(backend.handle_for(av::clproto::HandleKind::platform, wire_of(every[i])) == nullptr);
      }
  }
  backend.shut_down(); }

/* ---- shared virtual memory, and the events the host completes on its own --
 *
 * What a backend with no host OpenCL behind it still has to be right about:
 * an address the guest names before it allocated anything, and a callback
 * asked for by a guest whose device cannot wake it.
 */

TEST(opencl_backend, svm_for_answers_nothing_before_the_guest_allocates_anything)
{ OpenClBackend backend;
  /* Every address is a guest's invention until clSVMAlloc answers one. */
  CHECK(backend.svm_for(0x1000u, 1u) == nullptr);
  CHECK(backend.svm_for(0u, 0u) == nullptr);
  CHECK(backend.svm_for(~std::uint64_t{0}, 1u) == nullptr); }

TEST(opencl_backend, on_clSVMFree_frees_nothing_it_did_not_allocate)
{ OpenClBackend backend;
  /* The decoder resolves an address inside a live allocation as well as one
   * at its base, so the free has to judge it again: an address the host's
   * allocator never answered must not reach clSVMFree. Nothing was allocated
   * here at all, so nothing may be freed, and a backend with no host behind
   * it must not call through to one. */
  backend.on_clSVMFree(nullptr, nullptr);
  backend.on_clSVMFree(nullptr, reinterpret_cast<void *>(std::uintptr_t{0x1000}));
  CHECK(!backend.lost()); }

TEST(opencl_backend, on_clSetEventCallback_is_refused_when_the_device_cannot_wake_the_guest)
{ OpenClBackend backend;
  /* A version 4 device binds nothing. A callback nobody could deliver is
   * worse than a refusal the application can see: the guest would wait on it
   * for ever. */
  CHECK_EQ(backend.on_clSetEventCallback(nullptr, CL_COMPLETE), CL_INVALID_OPERATION); }

TEST(opencl_backend, bind_signal_is_what_makes_an_event_callback_possible_at_all)
{ OpenClBackend backend;
  CHECK_EQ(backend.on_clSetEventCallback(nullptr, CL_COMPLETE), CL_INVALID_OPERATION);
  backend.bind_signal(&nothing_to_wake, nullptr);
  /* Bound, the refusal is no longer this one: with no host OpenCL the
   * registration itself fails, which is the host's answer and not ours. */
  CHECK(backend.on_clSetEventCallback(nullptr, CL_COMPLETE) != CL_INVALID_OPERATION);
  /* And unbinding puts it back. */
  backend.bind_signal(nullptr, nullptr);
  CHECK_EQ(backend.on_clSetEventCallback(nullptr, CL_COMPLETE), CL_INVALID_OPERATION); }
