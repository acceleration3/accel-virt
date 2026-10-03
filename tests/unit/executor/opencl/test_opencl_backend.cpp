/* One guest context's OpenCL on the host's own. As with Vulkan, the driver's
 * own answer is not asserted - these pass on a machine with no OpenCL at all,
 * where every command still decodes and the platform count is zero. */
#include "executor/imported_blob.hpp"
#include "device/abi/av_executor.h"
#include "executor/opencl/cl_event_signals.hpp"
#include "executor/opencl/host_svm_ranges.hpp"
#include "executor/opencl/opencl_backend.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/shim/opencl_device.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/cl_blob_backing.hpp"
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

/* The context's blob arena, as the device sees it: whole pages, because the
 * ABI's host-pointer handle is measured in them. */
alignas(4096) std::uint8_t arena_region[2u * 4096u];
alignas(4096) std::uint8_t second_arena_region[4096];

AvBlobHandle host_pointer(std::uint8_t *base, std::uint64_t bytes)
{
    AvBlobHandle handle = {};
    handle.kind = AV_BLOB_HOST_POINTER;
    handle.pointer = base;
    handle.fd = -1;
    handle.size = bytes;
    return handle;
}

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
/* The context's blob arena, adopted so a buffer can be made over it.
 *
 * CL_MEM_USE_HOST_PTR is core OpenCL rather than an extension, so every
 * device takes one - which is what makes this the cheap half of feature 041's
 * promise. A dma-buf is still not adopted: that would be
 * clCreateBufferWithProperties with an fd, which is a per-platform question
 * this host answers differently on each of its OpenCL platforms. */
TEST(opencl_backend, adopts_one_host_pointer_as_the_memory_a_buffer_can_be_made_over)
{ OpenClBackend backend;
  /* Nothing adopted: no offset names anything. */
  CHECK(backend.lent_for(0u, 1u) == nullptr);
  av::ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(arena_region, sizeof arena_region)), av::Status::ok);
  CHECK(backend.adopt(blob));
  CHECK(blob.wire_handle() != 0u);
  CHECK(backend.lent_for(0u, sizeof arena_region) == arena_region);
  CHECK(backend.lent_for(4096u, 64u) == arena_region + 4096u);
  /* The bounds are the point: these are guest numbers. */
  CHECK(backend.lent_for(0u, sizeof arena_region + 1u) == nullptr);
  CHECK(backend.lent_for(sizeof arena_region, 1u) == nullptr);
  CHECK(backend.lent_for(~std::uint64_t{0}, 1u) == nullptr);

  /* One at a time: a second would leave the first unreachable and its
   * buffers pointing into memory nothing owns. */
  av::ImportedBlob second;
  CHECK_STATUS(second.take(2u, host_pointer(second_arena_region, sizeof second_arena_region)),
               av::Status::ok);
  CHECK(!backend.adopt(second));

  backend.abandon(blob);
  CHECK_EQ(blob.object(), 0u);
  CHECK(backend.lent_for(0u, 1u) == nullptr);
  /* And the slot is free again. */
  CHECK(backend.adopt(second));
  backend.abandon(second);
  backend.shut_down(); }

TEST(opencl_backend, a_dma_buf_is_not_memory_a_buffer_can_be_made_over)
{ OpenClBackend backend;
  av::ImportedBlob blob;
  AvBlobHandle    handle = {};
  handle.kind = AV_BLOB_DMA_BUF;
  handle.fd = 7;
  handle.size = 4096u;
  CHECK_STATUS(blob.take(1u, handle), av::Status::ok);
  CHECK(!backend.adopt(blob));
  CHECK(backend.lent_for(0u, 1u) == nullptr);
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
          CHECK_EQ(device_ != nullptr, (host_.sub_apis(i) & AV_EXECUTOR_SUB_API_OPENCL) != 0u);
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

namespace av {

/* The extension-string surgery, reached from outside because the strings that
 * exercise it are ones no host answers: a name that ends where the list's word
 * does not, a word that is only the start of the name, a list that fills its
 * room with no terminator, and one that ends in spaces. */
struct OpenClBackendProbe {
    OpenClBackend &backend;

    std::size_t without(char *text, std::size_t bytes, const char *name) const
    { return backend.without(text, bytes, name); }
    bool names(const char *text, std::size_t length, const char *name) const
    { return backend.names(text, length, name); }
};

}  // namespace av

/* What is struck out, and what is not: only the whole word, wherever it is in
 * the list, and the list stays a list. */
TEST(opencl_backend, striking_a_name_out_of_a_list_takes_the_word_and_nothing_near_it)
{ OpenClBackend      backend;
  av::OpenClBackendProbe probe{backend};
  char first[] = "cl_khr_gl_sharing cl_khr_fp16 cl_khr_icd";
  CHECK_EQ(probe.without(first, sizeof first, "cl_khr_gl_sharing"), std::strlen("cl_khr_fp16 cl_khr_icd") + 1u);
  CHECK(std::strcmp(first, "cl_khr_fp16 cl_khr_icd") == 0);
  char last[] = "cl_khr_fp16 cl_khr_gl_sharing";
  CHECK_EQ(probe.without(last, sizeof last, "cl_khr_gl_sharing"), std::strlen("cl_khr_fp16") + 1u);
  CHECK(std::strcmp(last, "cl_khr_fp16") == 0);
  /* A word the name only starts, and a word that only starts the name: neither
   * is the name. */
  char longer[] = "cl_khr_gl_sharing_more cl_khr_gl";
  CHECK_EQ(probe.without(longer, sizeof longer, "cl_khr_gl_sharing"), sizeof longer);
  CHECK(std::strcmp(longer, "cl_khr_gl_sharing_more cl_khr_gl") == 0);
  /* A list that ends in spaces, and one that fills its room with no
   * terminator at all: both are read as far as they go and no further. */
  char spaced[] = "cl_khr_fp16   ";
  CHECK_EQ(probe.without(spaced, sizeof spaced, "cl_khr_gl_sharing"), std::strlen("cl_khr_fp16") + 1u);
  CHECK(std::strcmp(spaced, "cl_khr_fp16") == 0);
  char unterminated[11] = {'c', 'l', '_', 'k', 'h', 'r', '_', 'f', 'p', '1', '6'};
  CHECK_EQ(probe.without(unterminated, 11u, "cl_khr_gl_sharing"), 12u);
  /* An empty list stays empty. */
  char empty[] = "";
  CHECK_EQ(probe.without(empty, sizeof empty, "cl_khr_gl_sharing"), 1u);
  CHECK_EQ(empty[0], '\0');
  /* And the comparison itself, at the two ends nothing else reaches. */
  CHECK(probe.names("cl_khr_fp16", 11u, "cl_khr_fp16"));
  CHECK(!probe.names("cl_khr_fp16", 5u, "cl_khr_fp16"));
  CHECK(!probe.names("cl_khr_fp16x", 12u, "cl_khr_fp16")); }

/* A display that was never opened is no display: the context binds, shares
 * nothing, and runs everything else. */
TEST(opencl_backend, a_display_that_never_opened_leaves_the_context_sharing_nothing)
{ HostOpenCl host;
  if (host.device() == nullptr) return;
  av::GlDisplay closed;
  OpenClBackend backend;
  CHECK(backend.initialize(host.device(), &closed, nullptr));
  cl_device_id const device = static_cast<cl_device_id>(host.device()->device());
  /* Nothing is opened into it, and the extension it would have been opened
   * through is not shown. */
  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, 0, 4096u, made));
  CHECK_EQ(made, 0u);
  char   answered[4096] = {};
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof answered, answered, nullptr),
           CL_SUCCESS);
  CHECK(std::strstr(answered, "cl_khr_gl_sharing") == nullptr);
  backend.shut_down(); }

/* A context the host found no device for is not one: it is refused at setup,
 * and until it is bound it is shown no platform and no device, and makes no
 * context from any type. */
TEST(opencl_backend, unbound_it_offers_no_platform_device_or_context)
{ OpenClBackend backend;
  CHECK(!backend.initialize(nullptr, nullptr, nullptr));
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

/* A context that cannot share tells the guest so, in the one word the guest's
 * ICD reads it from: with no display there is no vehicle, and a host that
 * lists cl_khr_gl_sharing is answered without it. Everything else in the list
 * is still there, in order. */
TEST(opencl_backend, a_context_that_cannot_share_strikes_gl_sharing_out_of_the_device_s_extensions)
{ HostOpenCl host;
  if (host.device() == nullptr) return;
  OpenClBackend backend;
  CHECK(backend.initialize(host.device(), nullptr, nullptr));
  cl_device_id const device = static_cast<cl_device_id>(host.device()->device());

  char   host_list[4096] = {};
  size_t host_bytes = 0u;
  CHECK_EQ(::clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof host_list, host_list, &host_bytes),
           CL_SUCCESS);
  if (std::strstr(host_list, "cl_khr_gl_sharing") == nullptr) return;

  char   answered[4096] = {};
  size_t bytes = 0u;
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof answered, answered, &bytes),
           CL_SUCCESS);
  CHECK(std::strstr(answered, "cl_khr_gl_sharing") == nullptr);
  /* The word is gone and nothing else is: every other name the host listed is
   * still answered, and the size is the string's own. */
  CHECK_EQ(bytes, std::strlen(answered) + 1u);
  CHECK(bytes < host_bytes);
  CHECK(std::strstr(answered, "cl_khr_byte_addressable_store") != nullptr ||
        std::strstr(host_list, "cl_khr_byte_addressable_store") == nullptr);
  /* A word that merely contains the name is not the name. */
  CHECK(std::strstr(answered, "  ") == nullptr);

  /* Every other query is the host's own, and so is a size asked with no room:
   * a caller may be promised more than it needs. */
  size_t room = 0u;
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0u, nullptr, &room), CL_SUCCESS);
  CHECK_EQ(room, host_bytes);
  cl_uint units = 0u;
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof units, &units, nullptr),
           CL_SUCCESS);
  CHECK(units != 0u);
  /* And a query the host refuses is refused, with nothing struck out of
   * anything. */
  CHECK(backend.on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 1u, answered, nullptr) != CL_SUCCESS); }

/* Bound, a context is shown its GPU's platform and device and nothing else,
 * whatever else the host has - this host has three platforms and a CPU runtime
 * beside two GPUs. The device it is shown is the GPU, by its own UUID. */
TEST(opencl_backend, bound_it_offers_its_gpu_s_platform_and_device_alone)
{ HostOpenCl host;
  if (host.device() == nullptr) return;
  OpenClBackend backend;
  CHECK(backend.initialize(host.device(), nullptr, nullptr));
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
  CHECK(backend.initialize(host.device(), nullptr, nullptr));
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
  CHECK(backend.initialize(host.device(), nullptr, nullptr));
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

/* ---- the ten commands of contracts/wire_additions.md --------------------
 *
 * Two shapes the wire had never carried before feature 052: a command that
 * returns an address rather than a handle or a result, and a parameter that
 * is an address inside memory this session allocated. Both are judged here
 * as the device judges them - before the host's OpenCL is handed anything -
 * so the cases below need no GPU except where they say so.
 */

namespace {

constexpr std::size_t frame_bytes = 256u;
constexpr std::size_t answer_bytes = 128u;

/* One command as a guest sends it: the frame, how much of it the decoder
 * read, and what it wrote back. */
class ClCommand {
public:
    explicit ClCommand(std::uint32_t id)
        : id_(id), bytes_{}, frame_(bytes_, sizeof bytes_), answer_{}, answered_(0u), consumed_(0u)
    {}

    ClCommand(const ClCommand &) = delete;
    ClCommand &operator=(const ClCommand &) = delete;

    WireWriter &frame() { return frame_; }

    bool decodes(OpenClBackend &backend)
    {
        std::uint8_t block[block_bytes];
        WireArena    arena(block, sizeof block);
        WireReader   payload(bytes_, frame_.offset());
        WireWriter   answer(answer_, sizeof answer_);
        const bool   ran = backend.run(id_, payload, answer, arena);
        consumed_ = frame_.offset() - payload.remaining();
        answered_ = answer.offset();
        return ran;
    }

    WireReader answer() const { return WireReader(answer_, answered_); }

    /* The cl_int a result-returning command wrote. */
    cl_int result() const
    { WireReader reply = answer();
      std::uint64_t value = 0u;
      CHECK(reply.read_varint(value));
      return static_cast<cl_int>(value); }

    /* The address an address-returning command wrote. */
    std::uint64_t address() const
    { WireReader reply = answer();
      std::uint64_t value = 0u;
      CHECK(reply.read_pod(value));
      return value; }

    /* Every prefix of what the decoder read is refused, and none of them
     * runs: a host that cut a frame short must not have half of it acted on. */
    void is_refused_at_every_shorter_length(OpenClBackend &backend) const
    { CHECK(consumed_ > 0u);
      const std::uint64_t before = backend.ran();
      for (std::size_t length = 0u; length < consumed_; length++) {
          std::uint8_t block[block_bytes];
          WireArena    arena(block, sizeof block);
          std::uint8_t reply[answer_bytes] = {};
          WireReader   payload(bytes_, length);
          WireWriter   answer(reply, sizeof reply);
          CHECK(!backend.run(id_, payload, answer, arena));
      }
      CHECK_EQ(backend.ran(), before); }

private:
    std::uint32_t id_;
    std::uint8_t  bytes_[frame_bytes];
    WireWriter    frame_;
    std::uint8_t  answer_[answer_bytes];
    std::size_t   answered_;
    std::size_t   consumed_;
};

/* An address no clSVMAlloc ever answered. Page-aligned and plausible, which
 * is the point: nothing about it looks wrong, and it is still refused. */
constexpr std::uint64_t invented_address = 0x7F0000004000ull;

void svm_alloc_frame(ClCommand &command, std::uint64_t context, std::uint64_t size)
{
    command.frame().write_pod<std::uint64_t>(context);
    command.frame().write_varint<std::uint64_t>(CL_MEM_READ_WRITE);
    command.frame().write_varint(size);
    command.frame().write_varint<std::uint64_t>(0u);
}

void svm_map_frame(ClCommand &command, std::uint64_t address, std::uint64_t size)
{
    command.frame().write_pod<std::uint64_t>(0u);       /* the queue */
    command.frame().write_varint<std::uint64_t>(CL_TRUE);
    command.frame().write_varint<std::uint64_t>(CL_MAP_READ);
    command.frame().write_pod<std::uint64_t>(address);
    command.frame().write_varint(size);
    command.frame().write_varint<std::uint64_t>(0u);    /* no events waited on */
    command.frame().write_pod<std::uint8_t>(0u);
    command.frame().write_pod<std::uint8_t>(0u);        /* no event wanted */
}

void svm_memcpy_frame(ClCommand &command, std::uint64_t destination, std::uint64_t source,
                      std::uint64_t size)
{
    command.frame().write_pod<std::uint64_t>(0u);
    command.frame().write_varint<std::uint64_t>(CL_FALSE);
    command.frame().write_pod<std::uint64_t>(destination);
    command.frame().write_pod<std::uint64_t>(source);
    command.frame().write_varint(size);
    command.frame().write_varint<std::uint64_t>(0u);
    command.frame().write_pod<std::uint8_t>(0u);
    command.frame().write_pod<std::uint8_t>(0u);
}

void kernel_arg_svm_frame(ClCommand &command, std::uint64_t address)
{
    command.frame().write_pod<std::uint64_t>(0u);       /* the kernel */
    command.frame().write_varint<std::uint64_t>(0u);    /* argument 0 */
    command.frame().write_pod<std::uint64_t>(address);
}

}  // namespace

/* 3. clSVMAlloc - the first of the two address returns. A failed allocation
 * is a null address; before feature 052's third classification the emitter
 * wrote CL_OUT_OF_RESOURCES out of a void *-returning command and the guest
 * read a failure as an address. */
TEST(opencl_backend, clSVMAlloc_the_host_refuses_answers_a_null_address_and_remembers_nothing)
{ OpenClBackend backend;
  ClCommand command(av::clproto::kCmd_clSVMAlloc);
  /* No context: the guest named none, so the host's allocator is asked with
   * none and answers nothing. */
  svm_alloc_frame(command, 0u, 256u);
  CHECK(command.decodes(backend));
  CHECK_EQ(backend.ran(), 1u);
  CHECK_EQ(command.address(), 0u);
  /* Nothing was remembered, so no address this context could name resolves. */
  CHECK(backend.svm_for(invented_address, 1u) == nullptr);
  CHECK(!backend.lost());
  command.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* 4. clSVMFree - an address. */
TEST(opencl_backend, clSVMFree_of_an_address_no_allocation_answered_is_refused_before_the_host)
{ OpenClBackend backend;
  ClCommand invented(av::clproto::kCmd_clSVMFree);
  invented.frame().write_pod<std::uint64_t>(0u);
  invented.frame().write_pod<std::uint64_t>(invented_address);
  CHECK(!invented.decodes(backend));
  CHECK_EQ(backend.ran(), 0u);
  invented.is_refused_at_every_shorter_length(backend);

  /* A null address is the API's own error rather than a refusal: the guest
   * asked to free nothing, which is a call the host answers. */
  ClCommand none(av::clproto::kCmd_clSVMFree);
  none.frame().write_pod<std::uint64_t>(0u);
  none.frame().write_pod<std::uint64_t>(0u);
  CHECK(none.decodes(backend));
  CHECK_EQ(backend.ran(), 1u);
  none.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* The other host address the guest is ever told: the one a map answers.
 *
 * clEnqueueUnmapMemObject names that address back, so the decoder judges it
 * through the same seam it judges an SVM address through - and a backend that
 * remembered only what clSVMAlloc answered refused every real unmap, which is
 * what T040 found. The two are kept in separate tables on purpose: one table
 * would let clSVMFree be handed a mapping's pointer and free it. */
TEST(opencl_backend, an_unmap_of_an_address_no_map_answered_is_refused_before_the_host)
{ OpenClBackend backend;
  ClCommand invented(av::clproto::kCmd_clEnqueueUnmapMemObject);
  invented.frame().write_pod<std::uint64_t>(0u);     /* the queue */
  invented.frame().write_pod<std::uint64_t>(0u);     /* the buffer */
  invented.frame().write_pod<std::uint64_t>(invented_address);
  invented.frame().write_varint<std::uint64_t>(0u);  /* no events waited on */
  invented.frame().write_pod<std::uint8_t>(0u);
  invented.frame().write_pod<std::uint8_t>(0u);      /* no event wanted */
  /* Refused at the decoder, so the host's clEnqueueUnmapMemObject is never
   * reached with an address of the guest's invention. */
  CHECK(!invented.decodes(backend));
  CHECK_EQ(backend.ran(), 0u);
  invented.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

TEST(opencl_backend, a_map_the_host_refuses_answers_a_null_address_and_remembers_no_mapping)
{ OpenClBackend backend;
  ClCommand command(av::clproto::kCmd_clEnqueueMapBuffer);
  command.frame().write_pod<std::uint64_t>(0u);        /* no queue, so the host refuses */
  command.frame().write_pod<std::uint64_t>(0u);        /* no buffer */
  command.frame().write_varint<std::uint64_t>(CL_TRUE);
  command.frame().write_varint<std::uint64_t>(CL_MAP_WRITE);
  command.frame().write_varint<std::uint64_t>(0u);     /* offset */
  command.frame().write_varint<std::uint64_t>(256u);   /* size */
  command.frame().write_varint<std::uint64_t>(0u);     /* no events waited on */
  command.frame().write_pod<std::uint8_t>(0u);
  command.frame().write_pod<std::uint8_t>(0u);         /* no event wanted */
  command.frame().write_pod<std::uint8_t>(1u);         /* the error is */
  CHECK(command.decodes(backend));
  CHECK_EQ(backend.ran(), 1u);
  CHECK_EQ(command.address(), 0u);
  /* A map that answered nothing left nothing to name, so the unmap of any
   * address is still refused. */
  CHECK(backend.svm_for(invented_address, 1u) == nullptr);
  CHECK(!backend.lost());
  command.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

namespace {

/* A clCreateBufferWithProperties frame: the property list, the flags, the
 * size, no host_ptr, and an error wanted. `named` says whether the list
 * carries this wire's blob-backing property, and `offset` where it points. */
void buffer_with_properties_frame(ClCommand &command, bool named, std::uint64_t offset,
                                  std::uint64_t size, bool error_wanted = true)
{
    command.frame().write_pod<std::uint64_t>(0u);        /* the context */
    command.frame().write_pod<std::uint8_t>(named ? 1u : 0u);
    if (named) {
        command.frame().write_varint<std::uint64_t>(3u); /* key, value, terminator */
        command.frame().write_pod<std::uint64_t>(av::ClBlobBacking::property);
        command.frame().write_pod<std::uint64_t>(offset);
        command.frame().write_pod<std::uint64_t>(0u);
    }
    command.frame().write_varint<std::uint64_t>(CL_MEM_READ_WRITE);
    command.frame().write_varint(size);
    command.frame().write_pod<std::uint8_t>(0u);         /* no host_ptr */
    command.frame().write_pod<std::uint8_t>(error_wanted ? 1u : 0u);
}

}  // namespace

/* A buffer made over the context's blob arena (feature 052, T033). The offset
 * is a number the guest wrote, so it is judged against the adopted arena
 * before it is ever a pointer: a buffer over memory that is not the arena's
 * would let the guest choose what the host writes into. */
TEST(opencl_backend, a_buffer_over_an_offset_outside_the_arena_is_refused)
{ OpenClBackend backend;
  av::ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(arena_region, sizeof arena_region)), av::Status::ok);
  CHECK(backend.adopt(blob));

  /* Past the end of the arena, and a length that runs past it from a base
   * that is inside. Both are addresses the guest invented. */
  ClCommand beyond(av::clproto::kCmd_clCreateBufferWithProperties);
  buffer_with_properties_frame(beyond, true, sizeof arena_region, 16u);
  CHECK(beyond.decodes(backend));
  CHECK_EQ(beyond.address(), 0u);
  beyond.is_refused_at_every_shorter_length(backend);

  ClCommand overrunning(av::clproto::kCmd_clCreateBufferWithProperties);
  buffer_with_properties_frame(overrunning, true, sizeof arena_region - 8u, 64u);
  CHECK(overrunning.decodes(backend));
  CHECK_EQ(overrunning.address(), 0u);

  backend.abandon(blob);
  /* And with no arena adopted at all, every offset is outside it. */
  ClCommand unbacked(av::clproto::kCmd_clCreateBufferWithProperties);
  buffer_with_properties_frame(unbacked, true, 0u, 16u);
  CHECK(unbacked.decodes(backend));
  CHECK_EQ(unbacked.address(), 0u);
  CHECK(!backend.lost());
  backend.shut_down(); }

TEST(opencl_backend, a_buffer_naming_no_backing_is_the_ordinary_call)
{ OpenClBackend backend;
  /* No property list, and a list naming something else: neither is this
   * wire's backing, so the host is asked for a plain buffer and answers for
   * itself - which with no context is nothing. */
  ClCommand plain(av::clproto::kCmd_clCreateBufferWithProperties);
  buffer_with_properties_frame(plain, false, 0u, 16u);
  CHECK(plain.decodes(backend));
  CHECK_EQ(plain.address(), 0u);
  CHECK_EQ(backend.ran(), 1u);
  plain.is_refused_at_every_shorter_length(backend);
  CHECK(!backend.lost());
  backend.shut_down(); }

/* 5. clEnqueueSVMMap - an address with an extent. */
TEST(opencl_backend, clEnqueueSVMMap_of_an_address_outside_every_range_is_refused)
{ OpenClBackend backend;
  ClCommand invented(av::clproto::kCmd_clEnqueueSVMMap);
  svm_map_frame(invented, invented_address, 64u);
  CHECK(!invented.decodes(backend));
  CHECK_EQ(backend.ran(), 0u);
  invented.is_refused_at_every_shorter_length(backend);

  ClCommand none(av::clproto::kCmd_clEnqueueSVMMap);
  svm_map_frame(none, 0u, 64u);
  CHECK(none.decodes(backend));
  /* Reached the host, which refuses the queue the guest did not name. */
  CHECK_EQ(none.result(), CL_INVALID_COMMAND_QUEUE);
  none.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* 6. clEnqueueSVMUnmap - an address with no extent. */
TEST(opencl_backend, clEnqueueSVMUnmap_of_an_address_outside_every_range_is_refused)
{ OpenClBackend backend;
  ClCommand invented(av::clproto::kCmd_clEnqueueSVMUnmap);
  invented.frame().write_pod<std::uint64_t>(0u);
  invented.frame().write_pod<std::uint64_t>(invented_address);
  invented.frame().write_varint<std::uint64_t>(0u);
  invented.frame().write_pod<std::uint8_t>(0u);
  invented.frame().write_pod<std::uint8_t>(0u);
  CHECK(!invented.decodes(backend));
  CHECK_EQ(backend.ran(), 0u);
  invented.is_refused_at_every_shorter_length(backend);

  ClCommand none(av::clproto::kCmd_clEnqueueSVMUnmap);
  none.frame().write_pod<std::uint64_t>(0u);
  none.frame().write_pod<std::uint64_t>(0u);
  none.frame().write_varint<std::uint64_t>(0u);
  none.frame().write_pod<std::uint8_t>(0u);
  none.frame().write_pod<std::uint8_t>(0u);
  CHECK(none.decodes(backend));
  CHECK_EQ(none.result(), CL_INVALID_COMMAND_QUEUE);
  none.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* 7. clEnqueueSVMMemcpy - two addresses judged against the one extent. Each
 * is judged: a copy whose destination is real and whose source is invented
 * would read host memory the guest never allocated. */
TEST(opencl_backend, clEnqueueSVMMemcpy_judges_both_of_its_addresses)
{ OpenClBackend backend;
  ClCommand invented_source(av::clproto::kCmd_clEnqueueSVMMemcpy);
  svm_memcpy_frame(invented_source, 0u, invented_address, 16u);
  CHECK(!invented_source.decodes(backend));
  invented_source.is_refused_at_every_shorter_length(backend);

  ClCommand invented_destination(av::clproto::kCmd_clEnqueueSVMMemcpy);
  svm_memcpy_frame(invented_destination, invented_address, 0u, 16u);
  CHECK(!invented_destination.decodes(backend));
  CHECK_EQ(backend.ran(), 0u);

  ClCommand none(av::clproto::kCmd_clEnqueueSVMMemcpy);
  svm_memcpy_frame(none, 0u, 0u, 16u);
  CHECK(none.decodes(backend));
  CHECK_EQ(none.result(), CL_INVALID_COMMAND_QUEUE);
  none.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* 8. clEnqueueSVMMemFill - an address with an extent, and a pattern that
 * crossed as bytes. A pattern_size larger than the bytes that arrived would
 * have the host read past what the guest sent. */
TEST(opencl_backend, clEnqueueSVMMemFill_refuses_a_pattern_shorter_than_the_size_it_claims)
{ OpenClBackend backend;
  ClCommand claimed(av::clproto::kCmd_clEnqueueSVMMemFill);
  claimed.frame().write_pod<std::uint64_t>(0u);
  claimed.frame().write_pod<std::uint64_t>(0u);
  claimed.frame().write_pod<std::uint8_t>(1u);           /* a pattern is present */
  claimed.frame().write_varint<std::uint64_t>(4u);       /* of four bytes */
  for (std::uint8_t byte = 0u; byte < 4u; byte++) claimed.frame().write_pod<std::uint8_t>(byte);
  claimed.frame().write_varint<std::uint64_t>(8u);       /* claiming eight */
  claimed.frame().write_varint<std::uint64_t>(64u);
  claimed.frame().write_varint<std::uint64_t>(0u);
  claimed.frame().write_pod<std::uint8_t>(0u);
  claimed.frame().write_pod<std::uint8_t>(0u);
  CHECK(!claimed.decodes(backend));
  CHECK_EQ(backend.ran(), 0u);
  claimed.is_refused_at_every_shorter_length(backend);

  ClCommand honest(av::clproto::kCmd_clEnqueueSVMMemFill);
  honest.frame().write_pod<std::uint64_t>(0u);
  honest.frame().write_pod<std::uint64_t>(0u);
  honest.frame().write_pod<std::uint8_t>(1u);
  honest.frame().write_varint<std::uint64_t>(4u);
  for (std::uint8_t byte = 0u; byte < 4u; byte++) honest.frame().write_pod<std::uint8_t>(byte);
  honest.frame().write_varint<std::uint64_t>(4u);
  honest.frame().write_varint<std::uint64_t>(64u);
  honest.frame().write_varint<std::uint64_t>(0u);
  honest.frame().write_pod<std::uint8_t>(0u);
  honest.frame().write_pod<std::uint8_t>(0u);
  CHECK(honest.decodes(backend));
  CHECK_EQ(honest.result(), CL_INVALID_COMMAND_QUEUE);
  honest.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* 9. clSetKernelArgSVMPointer - an address with no extent at all, which is
 * the one the guest is most likely to have invented: nothing else in the
 * command says how big it is. */
TEST(opencl_backend, clSetKernelArgSVMPointer_refuses_an_address_this_context_never_allocated)
{ OpenClBackend backend;
  ClCommand invented(av::clproto::kCmd_clSetKernelArgSVMPointer);
  kernel_arg_svm_frame(invented, invented_address);
  CHECK(!invented.decodes(backend));
  CHECK_EQ(backend.ran(), 0u);
  invented.is_refused_at_every_shorter_length(backend);

  /* Null is the API's own "no argument", and the host answers it. */
  ClCommand none(av::clproto::kCmd_clSetKernelArgSVMPointer);
  kernel_arg_svm_frame(none, 0u);
  CHECK(none.decodes(backend));
  CHECK_EQ(none.result(), CL_INVALID_KERNEL);
  none.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* 1. clEnqueueMapBuffer - the other address return, and the only command
 * whose event depends on it. A map that failed answers no address, and the
 * generated reply must then answer no event either: an event handle written
 * for a failed map is one the guest would release and the host never made. */
TEST(opencl_backend, clEnqueueMapBuffer_that_failed_answers_no_address_and_no_event)
{ OpenClBackend backend;
  ClCommand command(av::clproto::kCmd_clEnqueueMapBuffer);
  command.frame().write_pod<std::uint64_t>(0u);          /* the queue */
  command.frame().write_pod<std::uint64_t>(0u);          /* the buffer */
  command.frame().write_varint<std::uint64_t>(CL_TRUE);
  command.frame().write_varint<std::uint64_t>(CL_MAP_READ);
  command.frame().write_varint<std::uint64_t>(0u);       /* offset */
  command.frame().write_varint<std::uint64_t>(64u);      /* size */
  command.frame().write_varint<std::uint64_t>(0u);
  command.frame().write_pod<std::uint8_t>(0u);
  command.frame().write_pod<std::uint8_t>(1u);           /* an event is wanted */
  command.frame().write_pod<std::uint8_t>(1u);           /* and so is the error */
  CHECK(command.decodes(backend));
  WireReader    reply = command.answer();
  std::uint64_t mapped = 1u;
  std::uint64_t event = 1u;
  std::uint64_t error = 0u;
  CHECK(reply.read_pod(mapped));
  CHECK(reply.read_pod(event));
  CHECK(reply.read_varint(error));
  CHECK_EQ(mapped, 0u);
  CHECK_EQ(event, 0u);
  CHECK_EQ(static_cast<cl_int>(error), CL_INVALID_COMMAND_QUEUE);
  /* And no event was put in the registry for the guest to release. */
  CHECK(backend.handle_for(av::clproto::HandleKind::event, 1u) == nullptr);
  command.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* 2. clEnqueueUnmapMemObject - the address clEnqueueMapBuffer answered,
 * handed back. It is judged the same way an SVM address is, because it is
 * one: the guest may not name host memory it was never given. */
TEST(opencl_backend, clEnqueueUnmapMemObject_refuses_a_mapped_address_it_never_answered)
{ OpenClBackend backend;
  ClCommand invented(av::clproto::kCmd_clEnqueueUnmapMemObject);
  invented.frame().write_pod<std::uint64_t>(0u);
  invented.frame().write_pod<std::uint64_t>(0u);
  invented.frame().write_pod<std::uint64_t>(invented_address);
  invented.frame().write_varint<std::uint64_t>(0u);
  invented.frame().write_pod<std::uint8_t>(0u);
  invented.frame().write_pod<std::uint8_t>(0u);
  CHECK(!invented.decodes(backend));
  CHECK_EQ(backend.ran(), 0u);
  invented.is_refused_at_every_shorter_length(backend);

  ClCommand none(av::clproto::kCmd_clEnqueueUnmapMemObject);
  none.frame().write_pod<std::uint64_t>(0u);
  none.frame().write_pod<std::uint64_t>(0u);
  none.frame().write_pod<std::uint64_t>(0u);
  none.frame().write_varint<std::uint64_t>(0u);
  none.frame().write_pod<std::uint8_t>(0u);
  none.frame().write_pod<std::uint8_t>(0u);
  CHECK(none.decodes(backend));
  CHECK_EQ(none.result(), CL_INVALID_COMMAND_QUEUE);
  none.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* 10. clSetEventCallback - the command whose callback the lowering drops.
 * The generated server reaches this backend's own hook rather than calling
 * the host with a null pfn_notify, which the API refuses outright; which of
 * the two refusals comes back is how a test can tell the difference. */
TEST(opencl_backend, clSetEventCallback_over_the_wire_reaches_the_backends_own_registration)
{ OpenClBackend backend;
  ClCommand unheard(av::clproto::kCmd_clSetEventCallback);
  unheard.frame().write_pod<std::uint64_t>(0u);
  unheard.frame().write_varint<std::uint64_t>(static_cast<std::uint64_t>(CL_COMPLETE));
  CHECK(unheard.decodes(backend));
  /* No device callback bound: the backend refuses rather than register one
   * whose completion could never reach the guest. */
  CHECK_EQ(unheard.result(), CL_INVALID_OPERATION);
  unheard.is_refused_at_every_shorter_length(backend);

  backend.bind_signal(&nothing_to_wake, nullptr);
  ClCommand bound(av::clproto::kCmd_clSetEventCallback);
  bound.frame().write_pod<std::uint64_t>(0u);
  bound.frame().write_varint<std::uint64_t>(static_cast<std::uint64_t>(CL_COMPLETE));
  CHECK(bound.decodes(backend));
  /* Now it is the host that refuses the event the guest did not name, which
   * is what proves the registration was attempted at all. */
  CHECK_EQ(bound.result(), CL_INVALID_EVENT);
  bound.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* What the ten are for, on a host that has shared virtual memory: an address
 * the device answered is the only one it will take back, for exactly the
 * bytes it allocated, until it is freed. */
TEST(opencl_backend, an_allocated_address_is_the_only_one_the_ten_commands_accept)
{ HostOpenCl host;
  if (host.device() == nullptr) return;
  cl_int      error = CL_SUCCESS;
  cl_device_id device = static_cast<cl_device_id>(host.device()->device());
  const cl_context context = ::clCreateContext(nullptr, 1u, &device, nullptr, nullptr, &error);
  if (context == nullptr) return;

  OpenClBackend backend;
  CHECK(backend.initialize(host.device(), nullptr, nullptr));
  /* One reference for this test to hold past the shutdown, one for the ICD. */
  CHECK_EQ(::clRetainContext(context), CL_SUCCESS);
  backend.made(av::clproto::HandleKind::context, context);

  constexpr std::uint64_t allocated = 256u;
  ClCommand allocation(av::clproto::kCmd_clSVMAlloc);
  svm_alloc_frame(allocation, wire_of(context), allocated);
  CHECK(allocation.decodes(backend));
  const std::uint64_t base = allocation.address();
  if (base == 0u) {
      /* A device with no shared virtual memory; the refusals above are what
       * this backend owes such a host, and they are asserted there. */
      backend.shut_down();
      CHECK_EQ(::clReleaseContext(context), CL_SUCCESS);
      return;
  }

  /* Exactly what was allocated, and not a byte more. */
  CHECK(backend.svm_for(base, allocated) == reinterpret_cast<void *>(base));
  CHECK(backend.svm_for(base, allocated + 1u) == nullptr);
  CHECK(backend.svm_for(base + allocated, 0u) == nullptr);
  CHECK(backend.svm_for(base - 1u, 1u) == nullptr);

  /* A map whose extent leaves the allocation is refused; the same map inside
   * it reaches the host. */
  ClCommand past_the_end(av::clproto::kCmd_clEnqueueSVMMap);
  svm_map_frame(past_the_end, base, allocated + 1u);
  CHECK(!past_the_end.decodes(backend));
  ClCommand inside(av::clproto::kCmd_clEnqueueSVMMap);
  svm_map_frame(inside, base, allocated);
  CHECK(inside.decodes(backend));
  CHECK_EQ(inside.result(), CL_INVALID_COMMAND_QUEUE);

  /* A kernel argument points into the middle of an allocation, which is the
   * one place an address that is not a base is legitimate. */
  ClCommand middle(av::clproto::kCmd_clSetKernelArgSVMPointer);
  kernel_arg_svm_frame(middle, base + 64u);
  CHECK(middle.decodes(backend));
  CHECK_EQ(middle.result(), CL_INVALID_KERNEL);

  /* A copy out of the allocation into somewhere it never was. */
  ClCommand stray(av::clproto::kCmd_clEnqueueSVMMemcpy);
  svm_memcpy_frame(stray, invented_address, base, allocated);
  CHECK(!stray.decodes(backend));

  /* Freeing takes the base the host's allocator answered, and afterwards no
   * address of that allocation resolves. */
  ClCommand freeing(av::clproto::kCmd_clSVMFree);
  freeing.frame().write_pod<std::uint64_t>(wire_of(context));
  freeing.frame().write_pod<std::uint64_t>(base);
  CHECK(freeing.decodes(backend));
  CHECK(backend.svm_for(base, 1u) == nullptr);
  CHECK(backend.svm_for(base + 64u, 1u) == nullptr);

  CHECK(!backend.lost());
  backend.shut_down();
  CHECK_EQ(::clReleaseContext(context), CL_SUCCESS); }

/* A guest that asked for no error still gets no buffer: there is nowhere to
 * say why, and writing through a pointer it did not pass would be worse than
 * saying nothing. */
TEST(opencl_backend, a_buffer_over_a_bad_offset_with_nowhere_to_report_it_is_still_refused)
{ OpenClBackend backend;
  av::ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(arena_region, sizeof arena_region)), av::Status::ok);
  CHECK(backend.adopt(blob));
  ClCommand beyond(av::clproto::kCmd_clCreateBufferWithProperties);
  buffer_with_properties_frame(beyond, true, sizeof arena_region, 16u, false);
  CHECK(beyond.decodes(backend));
  CHECK_EQ(beyond.address(), 0u);
  beyond.is_refused_at_every_shorter_length(backend);
  backend.abandon(blob);
  backend.shut_down(); }

/* A registration the host refuses gives its slot straight back.
 *
 * The slot is taken before the host is asked, because the host is handed a
 * pointer to it - so a refusal that kept the slot would lose one per failed
 * registration, and a guest that asked often enough would run the session
 * out of them without ever registering anything. */
TEST(opencl_backend, a_callback_the_host_refuses_gives_its_registration_back)
{ OpenClBackend backend;
  int            device = 0;
  backend.bind_signal(&nothing_to_wake, &device);

  /* More asks than there are slots, every one of them refused by the host
   * because the event is nothing. If a refusal leaked a slot, the last of
   * these would answer CL_OUT_OF_RESOURCES instead. */
  for (std::uint32_t index = 0u; index < 200u; ++index) {
      ClCommand command(av::clproto::kCmd_clSetEventCallback);
      command.frame().write_pod<std::uint64_t>(0u);        /* the event */
      command.frame().write_varint<std::uint64_t>(0u);     /* CL_COMPLETE */
      CHECK(command.decodes(backend));
      CHECK(command.result() != CL_OUT_OF_RESOURCES);
  }
  backend.shut_down(); }

/* A device that gave this library nothing to signal with cannot wake the
 * guest, and a callback nobody hears is worse than a refusal - the guest
 * would wait on it forever - so the registration is refused outright. */
TEST(opencl_backend, a_callback_on_a_session_with_no_device_to_wake_is_refused)
{ OpenClBackend backend;
  ClCommand command(av::clproto::kCmd_clSetEventCallback);
  command.frame().write_pod<std::uint64_t>(0u);
  command.frame().write_varint<std::uint64_t>(0u);
  CHECK(command.decodes(backend));
  CHECK_EQ(command.result(), CL_INVALID_OPERATION);
  command.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

/* ---- the three tables a session cannot grow --------------------------------
 *
 * Shared allocations, mappings and callback registrations all live in
 * fixed-size tables, because nothing on the submission path allocates. What
 * happens at the end of each is not a detail: a guest that asked often enough
 * would reach it, and an address or a mapping the session could not judge
 * afterwards is worse than a refusal it can see. These need a host with
 * OpenCL, and say nothing on one without.
 */
namespace {

/* One real context on the host's OpenCL device, with a queue and a buffer for
 * the tests that need to map something. Null throughout where the host has no
 * OpenCL at all. */
class HostSession {
public:
    explicit HostSession(HostOpenCl &host)
        : context_(nullptr), queue_(nullptr), buffer_(nullptr)
    { if (host.device() == nullptr) return;
      cl_device_id device = static_cast<cl_device_id>(host.device()->device());
      cl_int       error = CL_SUCCESS;
      context_ = ::clCreateContext(nullptr, 1u, &device, nullptr, nullptr, &error);
      if (context_ == nullptr) return;
      queue_ = ::clCreateCommandQueueWithProperties(context_, device, nullptr, &error);
      buffer_ = ::clCreateBuffer(context_, CL_MEM_READ_WRITE, buffer_bytes, nullptr, &error); }
    ~HostSession()
    { if (buffer_ != nullptr) CHECK_EQ(::clReleaseMemObject(buffer_), CL_SUCCESS);
      if (queue_ != nullptr) CHECK_EQ(::clReleaseCommandQueue(queue_), CL_SUCCESS);
      if (context_ != nullptr) CHECK_EQ(::clReleaseContext(context_), CL_SUCCESS); }
    HostSession(const HostSession &) = delete;
    HostSession &operator=(const HostSession &) = delete;

    /* Room for one mapping per table slot, each at its own offset so each
     * answers its own address. */
    static constexpr std::size_t mapping_bytes = 64u;
    static constexpr std::size_t buffer_bytes =
        (av::HostSvmRanges::max_ranges + 1u) * mapping_bytes;

    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }
    cl_mem           buffer() const { return buffer_; }

private:
    cl_context       context_;
    cl_command_queue queue_;
    cl_mem           buffer_;
};

}  // namespace

TEST(opencl_backend, more_shared_allocations_than_the_table_holds_are_given_straight_back)
{ HostOpenCl  host;
  HostSession session(host);
  if (session.context() == nullptr) return;
  OpenClBackend backend;
  void         *held[av::HostSvmRanges::max_ranges] = {};
  for (std::uint32_t index = 0u; index < av::HostSvmRanges::max_ranges; ++index) {
      held[index] = backend.on_clSVMAlloc(session.context(), CL_MEM_READ_WRITE, 64u, 0u);
      if (held[index] == nullptr) return;   /* a device without shared memory */
  }
  /* One more than the table holds: the host allocates it and it is freed
   * again, because an address this session could never resolve is worse than
   * a refusal the guest can see at the allocation. */
  CHECK(backend.on_clSVMAlloc(session.context(), CL_MEM_READ_WRITE, 64u, 0u) == nullptr);
  for (void *const address : held) backend.on_clSVMFree(session.context(), address);
  /* And with the table drained, the next one is answered again. */
  void *const after = backend.on_clSVMAlloc(session.context(), CL_MEM_READ_WRITE, 64u, 0u);
  CHECK(after != nullptr);
  backend.on_clSVMFree(session.context(), after);
  CHECK(!backend.lost());
  backend.shut_down(); }

TEST(opencl_backend, more_mappings_than_the_table_holds_are_unmapped_again)
{ HostOpenCl  host;
  HostSession session(host);
  if (session.queue() == nullptr || session.buffer() == nullptr) return;
  OpenClBackend backend;
  void         *held[av::HostSvmRanges::max_ranges] = {};
  for (std::uint32_t index = 0u; index < av::HostSvmRanges::max_ranges; ++index) {
      cl_int error = CL_SUCCESS;
      held[index] = backend.on_clEnqueueMapBuffer(session.queue(), session.buffer(), CL_TRUE,
                                                  CL_MAP_WRITE, index * HostSession::mapping_bytes,
                                                  HostSession::mapping_bytes, 0u, nullptr, nullptr,
                                                  &error);
      if (held[index] == nullptr) return;   /* a device that maps no buffer */
      CHECK_EQ(error, CL_SUCCESS);
  }
  /* The one past the end, both ways the guest can ask: with somewhere to
   * report the refusal and with nowhere. Either way the mapping the host made
   * is given back, because a buffer the guest could never unmap would be held
   * for the life of the context. */
  cl_int refused = CL_SUCCESS;
  CHECK(backend.on_clEnqueueMapBuffer(session.queue(), session.buffer(), CL_TRUE, CL_MAP_WRITE,
                                      av::HostSvmRanges::max_ranges * HostSession::mapping_bytes,
                                      HostSession::mapping_bytes, 0u, nullptr, nullptr,
                                      &refused) == nullptr);
  CHECK_EQ(refused, CL_OUT_OF_RESOURCES);
  CHECK(backend.on_clEnqueueMapBuffer(session.queue(), session.buffer(), CL_TRUE, CL_MAP_WRITE,
                                      av::HostSvmRanges::max_ranges * HostSession::mapping_bytes,
                                      HostSession::mapping_bytes, 0u, nullptr, nullptr,
                                      nullptr) == nullptr);
  for (void *const address : held)
      CHECK_EQ(backend.on_clEnqueueUnmapMemObject(session.queue(), session.buffer(), address, 0u,
                                                  nullptr, nullptr),
               CL_SUCCESS);
  CHECK_EQ(::clFinish(session.queue()), CL_SUCCESS);
  CHECK(!backend.lost());
  backend.shut_down(); }

TEST(opencl_backend, more_callback_registrations_than_the_table_holds_are_refused)
{ HostOpenCl  host;
  HostSession session(host);
  if (session.context() == nullptr) return;
  OpenClBackend backend;
  int           device = 0;
  backend.bind_signal(&nothing_to_wake, &device);
  cl_event events[av::ClEventSignals::max_registrations + 1u] = {};
  for (cl_event &event : events) {
      cl_int error = CL_SUCCESS;
      event = ::clCreateUserEvent(session.context(), &error);
      if (event == nullptr) return;   /* a device with no user events */
  }
  for (std::uint32_t index = 0u; index < av::ClEventSignals::max_registrations; ++index) {
      const cl_int registered = backend.on_clSetEventCallback(events[index], CL_COMPLETE);
      if (registered != CL_SUCCESS) {
          for (cl_event event : events) CHECK_EQ(::clReleaseEvent(event), CL_SUCCESS);
          backend.shut_down();
          return;   /* a host that registers no callback at all */
      }
  }
  /* Every slot taken: the guest is refused rather than handed a registration
   * whose completion nothing could deliver. */
  CHECK_EQ(backend.on_clSetEventCallback(events[av::ClEventSignals::max_registrations], CL_COMPLETE),
           CL_OUT_OF_RESOURCES);
  /* Completed so the host's own callbacks run while the slots are still this
   * session's, rather than after it is torn down. */
  for (cl_event event : events) CHECK_EQ(::clSetUserEventStatus(event, CL_COMPLETE), CL_SUCCESS);
  for (cl_event event : events) CHECK_EQ(::clReleaseEvent(event), CL_SUCCESS);
  backend.shut_down(); }
