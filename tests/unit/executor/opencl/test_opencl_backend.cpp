#include "executor/exported_memory.hpp"
#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"
#include "device/abi/av_executor.h"
#include "executor/opencl/cl_event_signals.hpp"
#include "executor/opencl/host_svm_ranges.hpp"
#include "executor/opencl/opencl_backend.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/shim/opencl_device.hpp"
#include "executor/sharing/shared_image_shape.hpp"
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
std::uint64_t detail = 0u;

constexpr std::size_t block_bytes = 65536u;

alignas(4096) std::uint8_t arena_region[2u * 4096u];
alignas(4096) std::uint8_t second_arena_region[4096];

av::SourceMemory &memory_over(std::uint8_t *base, std::uint64_t bytes)
{
    static av::SourceMemory whole(1u, 1u, 0u, AV_MEMORY_WRITE_BACK,
                                  av::ExportedMemory(1u, -1, arena_region, sizeof arena_region));
    static av::SourceMemory other(1u, 2u, 0u, AV_MEMORY_WRITE_BACK,
                                  av::ExportedMemory(2u, -1, second_arena_region, sizeof second_arena_region));
    return base == arena_region && bytes == sizeof arena_region ? whole : other;
}

void nothing_to_wake(void *) {}

}

TEST(opencl_backend, knows_the_ids_the_profile_carries_and_no_others)
{ OpenClBackend backend;
  CHECK(backend.knows(av::clproto::kCmd_clGetPlatformIDs));
  CHECK(!backend.knows(0x2FFFFu));
  CHECK(!backend.knows(av::clproto::kCmd_clGetPlatformIDs ^ 0x30000u));
  CHECK(!backend.knows(0u)); }

TEST(opencl_backend, a_well_formed_command_decodes_and_answers)
{ OpenClBackend backend;
  std::uint8_t frame[64];
  WireWriter writer(frame, sizeof frame);
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
  CHECK_EQ(backend.ran(), 1u);
  backend.shut_down(); }

TEST(opencl_backend, adopts_one_host_pointer_as_the_memory_a_buffer_can_be_made_over)
{ OpenClBackend backend;
  CHECK(backend.lent_for(0u, 1u) == nullptr);
  av::ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, memory_over(arena_region, sizeof arena_region)), av::Status::ok);
  CHECK(backend.adopt(blob));
  CHECK(blob.wire_handle() != 0u);
  CHECK(backend.lent_for(0u, sizeof arena_region) == arena_region);
  CHECK(backend.lent_for(4096u, 64u) == arena_region + 4096u);
  CHECK(backend.lent_for(0u, sizeof arena_region + 1u) == nullptr);
  CHECK(backend.lent_for(sizeof arena_region, 1u) == nullptr);
  CHECK(backend.lent_for(~std::uint64_t{0}, 1u) == nullptr);

  av::ImportedBlob second;
  CHECK_STATUS(second.take(2u, memory_over(second_arena_region, sizeof second_arena_region)),
               av::Status::ok);
  CHECK(!backend.adopt(second));

  backend.abandon(blob);
  CHECK_EQ(blob.object(), 0u);
  CHECK(backend.lent_for(0u, 1u) == nullptr);
  CHECK(backend.adopt(second));
  backend.abandon(second);
  backend.shut_down(); }

TEST(opencl_backend, memory_with_no_mapping_is_not_memory_a_buffer_can_be_made_over)
{ OpenClBackend backend;
  av::SourceMemory unmapped(1u, 3u, 0u, AV_MEMORY_WRITE_BACK, av::ExportedMemory(3u, -1, nullptr, 4096u));
  av::ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, unmapped), av::Status::ok);
  CHECK(!backend.adopt(blob));
  CHECK(backend.lent_for(0u, 1u) == nullptr);
  backend.abandon(blob);
  CHECK_EQ(blob.object(), 0u); }

namespace {

unsigned char host_objects[3001u];

std::uint64_t wire_of(const void *object)
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(object));
}

}

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

TEST(opencl_backend, holds_thousands_of_objects_with_room_made_first)
{ av::MockPlatform platform;
  OpenClBackend    backend(platform);
  constexpr std::size_t many = 3000u;
  CHECK(backend.room_for(many));
  for (std::size_t i = 0u; i < many; i++) backend.made(av::clproto::HandleKind::mem, &host_objects[i]);
  bool answered = true;
  for (std::size_t i = 0u; i < many; i++)
      answered = answered && backend.handle_for(av::clproto::HandleKind::mem, wire_of(&host_objects[i])) == &host_objects[i];
  CHECK(answered);
  CHECK(!backend.lost());
  for (std::size_t i = 0u; i < many; i++) backend.released(av::clproto::HandleKind::mem, &host_objects[i]);
  backend.shut_down(); }

TEST(opencl_backend, no_room_refuses_before_the_icd_and_loses_nothing)
{ av::MockPlatform platform;
  OpenClBackend    backend(platform);
  CHECK(!backend.room_for(~std::uint64_t{0}));
  platform.fail_allocation_at(1u);
  CHECK(!backend.room_for(1u));
  platform.fail_allocation_at(1u);
  backend.made(av::clproto::HandleKind::event, &host_objects[0]);
  CHECK(backend.handle_for(av::clproto::HandleKind::event, wire_of(&host_objects[0])) == nullptr);
  CHECK(!backend.lost());
  backend.shut_down(); }

TEST(opencl_backend, an_address_held_as_another_kind_loses_the_device)
{ av::MockPlatform platform;
  OpenClBackend    backend(platform);
  backend.made(av::clproto::HandleKind::event, &host_objects[0]);
  CHECK(!backend.lost());
  backend.made(av::clproto::HandleKind::mem, &host_objects[0]);
  CHECK(backend.lost());
  backend.released(av::clproto::HandleKind::event, &host_objects[0]);
  backend.shut_down();
  CHECK(!backend.lost()); }

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

}

TEST(opencl_backend, a_display_that_never_opened_leaves_the_context_sharing_nothing)
{ HostOpenCl host;
  if (host.device() == nullptr) return;
  av::GlDisplay closed;
  OpenClBackend backend;
  CHECK(backend.initialize(host.device(), &closed, nullptr));
  cl_device_id const device = static_cast<cl_device_id>(host.device()->device());
  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, 0, 4096u, av::SharedImageShape(), made, detail));
  CHECK_EQ(made, 0u);
  char   answered[4096] = {};
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof answered, answered, nullptr),
           CL_SUCCESS);
  CHECK(std::strstr(answered, "cl_khr_gl_sharing") == nullptr);
  CHECK(std::strstr(answered, "cl_khr_external_memory ") == nullptr);
  backend.shut_down(); }

TEST(opencl_backend, a_context_with_no_gl_names_no_gl_object)
{ HostOpenCl host;
  if (host.device() == nullptr) return;
  OpenClBackend backend;
  CHECK(backend.initialize(host.device(), nullptr, nullptr));
  int    nothing = 0;
  auto  *context = reinterpret_cast<cl_context>(&nothing);
  auto  *mem = reinterpret_cast<cl_mem>(&nothing);
  cl_int error = CL_SUCCESS;
  CHECK(backend.on_clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(backend.on_clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, 1u, nullptr) == nullptr);
  error = CL_SUCCESS;
  CHECK(backend.on_clCreateFromGLTexture(context, CL_MEM_READ_WRITE, 0x0DE1u, 0, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(backend.on_clCreateFromGLTexture(context, CL_MEM_READ_WRITE, 0x0DE1u, 0, 1u, nullptr) == nullptr);
  error = CL_SUCCESS;
  CHECK(backend.on_clCreateFromGLTexture2D(context, CL_MEM_READ_WRITE, 0x0DE1u, 0, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = CL_SUCCESS;
  CHECK(backend.on_clCreateFromGLTexture3D(context, CL_MEM_READ_WRITE, 0x806Fu, 0, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = CL_SUCCESS;
  CHECK(backend.on_clCreateFromGLRenderbuffer(context, CL_MEM_READ_WRITE, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(backend.on_clCreateFromGLRenderbuffer(context, CL_MEM_READ_WRITE, 1u, nullptr) == nullptr);
  cl_gl_object_type type = 0u;
  cl_GLuint         name = 0u;
  CHECK_EQ(backend.on_clGetGLObjectInfo(mem, &type, &name), CL_INVALID_GL_OBJECT);
  cl_GLenum target = 0u;
  CHECK_EQ(backend.on_clGetGLTextureInfo(mem, CL_GL_TEXTURE_TARGET, sizeof target, &target, nullptr),
           CL_INVALID_GL_OBJECT);
  auto *queue = reinterpret_cast<cl_command_queue>(&nothing);
  CHECK_EQ(backend.on_clEnqueueAcquireGLObjects(queue, 1u, &mem, 0u, nullptr, nullptr), CL_INVALID_CONTEXT);
  CHECK_EQ(backend.on_clEnqueueReleaseGLObjects(queue, 1u, &mem, 0u, nullptr, nullptr), CL_INVALID_CONTEXT);
  backend.shut_down(); }

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

TEST(opencl_backend, lists_asked_for_nowhere_are_invalid_values)
{ OpenClBackend backend;
  cl_platform_id platforms[1] = {};
  cl_uint count = 0u;
  CHECK_EQ(backend.on_clGetPlatformIDs(0u, platforms, &count), CL_INVALID_VALUE);
  CHECK_EQ(backend.on_clGetPlatformIDs(1u, nullptr, nullptr), CL_INVALID_VALUE);
  cl_device_id devices[1] = {};
  CHECK_EQ(backend.on_clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, 0u, devices, &count), CL_INVALID_VALUE);
  CHECK_EQ(backend.on_clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, 1u, nullptr, nullptr), CL_INVALID_VALUE); }

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
  CHECK_EQ(bytes, std::strlen(answered) + 1u);
  CHECK(bytes < host_bytes);
  CHECK(std::strstr(answered, "cl_khr_byte_addressable_store") != nullptr ||
        std::strstr(host_list, "cl_khr_byte_addressable_store") == nullptr);
  CHECK(std::strstr(answered, "  ") == nullptr);

  size_t room = 0u;
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0u, nullptr, &room), CL_SUCCESS);
  CHECK_EQ(room, bytes);
  cl_uint units = 0u;
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof units, &units, nullptr),
           CL_SUCCESS);
  CHECK(units != 0u);
  std::memset(answered, 'x', sizeof answered);
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 1u, answered, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(answered[0], 'x');
  CHECK(backend.on_clGetDeviceInfo(nullptr, CL_DEVICE_EXTENSIONS, sizeof answered, answered, nullptr) != CL_SUCCESS); }

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

TEST(opencl_backend, svm_for_answers_nothing_before_the_guest_allocates_anything)
{ OpenClBackend backend;
  CHECK(backend.svm_for(0x1000u, 1u) == nullptr);
  CHECK(backend.svm_for(0u, 0u) == nullptr);
  CHECK(backend.svm_for(~std::uint64_t{0}, 1u) == nullptr); }

TEST(opencl_backend, on_clSVMFree_frees_nothing_it_did_not_allocate)
{ OpenClBackend backend;
  backend.on_clSVMFree(nullptr, nullptr);
  backend.on_clSVMFree(nullptr, reinterpret_cast<void *>(std::uintptr_t{0x1000}));
  CHECK(!backend.lost()); }

TEST(opencl_backend, on_clSetEventCallback_is_refused_when_the_device_cannot_wake_the_guest)
{ OpenClBackend backend;
  CHECK_EQ(backend.on_clSetEventCallback(nullptr, CL_COMPLETE), CL_INVALID_OPERATION); }

TEST(opencl_backend, bind_signal_is_what_makes_an_event_callback_possible_at_all)
{ OpenClBackend backend;
  CHECK_EQ(backend.on_clSetEventCallback(nullptr, CL_COMPLETE), CL_INVALID_OPERATION);
  backend.bind_signal(&nothing_to_wake, nullptr);
  CHECK(backend.on_clSetEventCallback(nullptr, CL_COMPLETE) != CL_INVALID_OPERATION);
  backend.bind_signal(nullptr, nullptr);
  CHECK_EQ(backend.on_clSetEventCallback(nullptr, CL_COMPLETE), CL_INVALID_OPERATION); }

namespace {

constexpr std::size_t frame_bytes = 256u;
constexpr std::size_t answer_bytes = 128u;

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

    cl_int result() const
    { WireReader reply = answer();
      std::uint64_t value = 0u;
      CHECK(reply.read_varint(value));
      return static_cast<cl_int>(value); }

    std::uint64_t address() const
    { WireReader reply = answer();
      std::uint64_t value = 0u;
      CHECK(reply.read_pod(value));
      return value; }

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
    command.frame().write_pod<std::uint64_t>(0u);
    command.frame().write_varint<std::uint64_t>(CL_TRUE);
    command.frame().write_varint<std::uint64_t>(CL_MAP_READ);
    command.frame().write_pod<std::uint64_t>(address);
    command.frame().write_varint(size);
    command.frame().write_varint<std::uint64_t>(0u);
    command.frame().write_pod<std::uint8_t>(0u);
    command.frame().write_pod<std::uint8_t>(0u);
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
    command.frame().write_pod<std::uint64_t>(0u);
    command.frame().write_varint<std::uint64_t>(0u);
    command.frame().write_pod<std::uint64_t>(address);
}

}

TEST(opencl_backend, clSVMAlloc_the_host_refuses_answers_a_null_address_and_remembers_nothing)
{ OpenClBackend backend;
  ClCommand command(av::clproto::kCmd_clSVMAlloc);
  svm_alloc_frame(command, 0u, 256u);
  CHECK(command.decodes(backend));
  CHECK_EQ(backend.ran(), 1u);
  CHECK_EQ(command.address(), 0u);
  CHECK(backend.svm_for(invented_address, 1u) == nullptr);
  CHECK(!backend.lost());
  command.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

TEST(opencl_backend, clSVMFree_of_an_address_no_allocation_answered_is_refused_before_the_host)
{ OpenClBackend backend;
  ClCommand invented(av::clproto::kCmd_clSVMFree);
  invented.frame().write_pod<std::uint64_t>(0u);
  invented.frame().write_pod<std::uint64_t>(invented_address);
  CHECK(!invented.decodes(backend));
  CHECK_EQ(backend.ran(), 0u);
  invented.is_refused_at_every_shorter_length(backend);

  ClCommand none(av::clproto::kCmd_clSVMFree);
  none.frame().write_pod<std::uint64_t>(0u);
  none.frame().write_pod<std::uint64_t>(0u);
  CHECK(none.decodes(backend));
  CHECK_EQ(backend.ran(), 1u);
  none.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

TEST(opencl_backend, an_unmap_of_an_address_no_map_answered_is_refused_before_the_host)
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
  backend.shut_down(); }

TEST(opencl_backend, a_map_the_host_refuses_answers_a_null_address_and_remembers_no_mapping)
{ OpenClBackend backend;
  ClCommand command(av::clproto::kCmd_clEnqueueMapBuffer);
  command.frame().write_pod<std::uint64_t>(0u);
  command.frame().write_pod<std::uint64_t>(0u);
  command.frame().write_varint<std::uint64_t>(CL_TRUE);
  command.frame().write_varint<std::uint64_t>(CL_MAP_WRITE);
  command.frame().write_varint<std::uint64_t>(0u);
  command.frame().write_varint<std::uint64_t>(256u);
  command.frame().write_varint<std::uint64_t>(0u);
  command.frame().write_pod<std::uint8_t>(0u);
  command.frame().write_pod<std::uint8_t>(0u);
  command.frame().write_pod<std::uint8_t>(1u);
  CHECK(command.decodes(backend));
  CHECK_EQ(backend.ran(), 1u);
  CHECK_EQ(command.address(), 0u);
  CHECK(backend.svm_for(invented_address, 1u) == nullptr);
  CHECK(!backend.lost());
  command.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

namespace {
void buffer_with_properties_frame(ClCommand &command, bool named, std::uint64_t offset,
                                  std::uint64_t size, bool error_wanted = true)
{
    command.frame().write_pod<std::uint64_t>(0u);
    command.frame().write_pod<std::uint8_t>(named ? 1u : 0u);
    if (named) {
        command.frame().write_varint<std::uint64_t>(3u);
        command.frame().write_pod<std::uint64_t>(av::ClBlobBacking::property);
        command.frame().write_pod<std::uint64_t>(offset);
        command.frame().write_pod<std::uint64_t>(0u);
    }
    command.frame().write_varint<std::uint64_t>(CL_MEM_READ_WRITE);
    command.frame().write_varint(size);
    command.frame().write_pod<std::uint8_t>(0u);
    command.frame().write_pod<std::uint8_t>(error_wanted ? 1u : 0u);
}

}

TEST(opencl_backend, a_buffer_over_an_offset_outside_the_arena_is_refused)
{ OpenClBackend backend;
  av::ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, memory_over(arena_region, sizeof arena_region)), av::Status::ok);
  CHECK(backend.adopt(blob));

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
  ClCommand unbacked(av::clproto::kCmd_clCreateBufferWithProperties);
  buffer_with_properties_frame(unbacked, true, 0u, 16u);
  CHECK(unbacked.decodes(backend));
  CHECK_EQ(unbacked.address(), 0u);
  CHECK(!backend.lost());
  backend.shut_down(); }

TEST(opencl_backend, a_buffer_naming_no_backing_is_the_ordinary_call)
{ OpenClBackend backend;
  ClCommand plain(av::clproto::kCmd_clCreateBufferWithProperties);
  buffer_with_properties_frame(plain, false, 0u, 16u);
  CHECK(plain.decodes(backend));
  CHECK_EQ(plain.address(), 0u);
  CHECK_EQ(backend.ran(), 1u);
  plain.is_refused_at_every_shorter_length(backend);
  CHECK(!backend.lost());
  backend.shut_down(); }

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
  CHECK_EQ(none.result(), CL_INVALID_COMMAND_QUEUE);
  none.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

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

TEST(opencl_backend, clEnqueueSVMMemFill_refuses_a_pattern_shorter_than_the_size_it_claims)
{ OpenClBackend backend;
  ClCommand claimed(av::clproto::kCmd_clEnqueueSVMMemFill);
  claimed.frame().write_pod<std::uint64_t>(0u);
  claimed.frame().write_pod<std::uint64_t>(0u);
  claimed.frame().write_pod<std::uint8_t>(1u);
  claimed.frame().write_varint<std::uint64_t>(4u);
  for (std::uint8_t byte = 0u; byte < 4u; byte++) claimed.frame().write_pod<std::uint8_t>(byte);
  claimed.frame().write_varint<std::uint64_t>(8u);
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

TEST(opencl_backend, clSetKernelArgSVMPointer_refuses_an_address_this_context_never_allocated)
{ OpenClBackend backend;
  ClCommand invented(av::clproto::kCmd_clSetKernelArgSVMPointer);
  kernel_arg_svm_frame(invented, invented_address);
  CHECK(!invented.decodes(backend));
  CHECK_EQ(backend.ran(), 0u);
  invented.is_refused_at_every_shorter_length(backend);

  ClCommand none(av::clproto::kCmd_clSetKernelArgSVMPointer);
  kernel_arg_svm_frame(none, 0u);
  CHECK(none.decodes(backend));
  CHECK_EQ(none.result(), CL_INVALID_KERNEL);
  none.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

TEST(opencl_backend, clEnqueueMapBuffer_that_failed_answers_no_address_and_no_event)
{ OpenClBackend backend;
  ClCommand command(av::clproto::kCmd_clEnqueueMapBuffer);
  command.frame().write_pod<std::uint64_t>(0u);
  command.frame().write_pod<std::uint64_t>(0u);
  command.frame().write_varint<std::uint64_t>(CL_TRUE);
  command.frame().write_varint<std::uint64_t>(CL_MAP_READ);
  command.frame().write_varint<std::uint64_t>(0u);
  command.frame().write_varint<std::uint64_t>(64u);
  command.frame().write_varint<std::uint64_t>(0u);
  command.frame().write_pod<std::uint8_t>(0u);
  command.frame().write_pod<std::uint8_t>(1u);
  command.frame().write_pod<std::uint8_t>(1u);
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
  CHECK(backend.handle_for(av::clproto::HandleKind::event, 1u) == nullptr);
  command.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

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

TEST(opencl_backend, clSetEventCallback_over_the_wire_reaches_the_backends_own_registration)
{ OpenClBackend backend;
  ClCommand unheard(av::clproto::kCmd_clSetEventCallback);
  unheard.frame().write_pod<std::uint64_t>(0u);
  unheard.frame().write_varint<std::uint64_t>(static_cast<std::uint64_t>(CL_COMPLETE));
  CHECK(unheard.decodes(backend));
  CHECK_EQ(unheard.result(), CL_INVALID_OPERATION);
  unheard.is_refused_at_every_shorter_length(backend);

  backend.bind_signal(&nothing_to_wake, nullptr);
  ClCommand bound(av::clproto::kCmd_clSetEventCallback);
  bound.frame().write_pod<std::uint64_t>(0u);
  bound.frame().write_varint<std::uint64_t>(static_cast<std::uint64_t>(CL_COMPLETE));
  CHECK(bound.decodes(backend));
  CHECK_EQ(bound.result(), CL_INVALID_EVENT);
  bound.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

TEST(opencl_backend, an_allocated_address_is_the_only_one_the_ten_commands_accept)
{ HostOpenCl host;
  if (host.device() == nullptr) return;
  cl_int      error = CL_SUCCESS;
  cl_device_id device = static_cast<cl_device_id>(host.device()->device());
  const cl_context context = ::clCreateContext(nullptr, 1u, &device, nullptr, nullptr, &error);
  if (context == nullptr) return;

  OpenClBackend backend;
  CHECK(backend.initialize(host.device(), nullptr, nullptr));
  CHECK_EQ(::clRetainContext(context), CL_SUCCESS);
  backend.made(av::clproto::HandleKind::context, context);

  constexpr std::uint64_t allocated = 256u;
  ClCommand allocation(av::clproto::kCmd_clSVMAlloc);
  svm_alloc_frame(allocation, wire_of(context), allocated);
  CHECK(allocation.decodes(backend));
  const std::uint64_t base = allocation.address();
  if (base == 0u) {
      backend.shut_down();
      CHECK_EQ(::clReleaseContext(context), CL_SUCCESS);
      return;
  }

  CHECK(backend.svm_for(base, allocated) == reinterpret_cast<void *>(base));
  CHECK(backend.svm_for(base, allocated + 1u) == nullptr);
  CHECK(backend.svm_for(base + allocated, 0u) == nullptr);
  CHECK(backend.svm_for(base - 1u, 1u) == nullptr);

  ClCommand past_the_end(av::clproto::kCmd_clEnqueueSVMMap);
  svm_map_frame(past_the_end, base, allocated + 1u);
  CHECK(!past_the_end.decodes(backend));
  ClCommand inside(av::clproto::kCmd_clEnqueueSVMMap);
  svm_map_frame(inside, base, allocated);
  CHECK(inside.decodes(backend));
  CHECK_EQ(inside.result(), CL_INVALID_COMMAND_QUEUE);

  ClCommand middle(av::clproto::kCmd_clSetKernelArgSVMPointer);
  kernel_arg_svm_frame(middle, base + 64u);
  CHECK(middle.decodes(backend));
  CHECK_EQ(middle.result(), CL_INVALID_KERNEL);

  ClCommand stray(av::clproto::kCmd_clEnqueueSVMMemcpy);
  svm_memcpy_frame(stray, invented_address, base, allocated);
  CHECK(!stray.decodes(backend));

  ClCommand freeing(av::clproto::kCmd_clSVMFree);
  freeing.frame().write_pod<std::uint64_t>(wire_of(context));
  freeing.frame().write_pod<std::uint64_t>(base);
  CHECK(freeing.decodes(backend));
  CHECK(backend.svm_for(base, 1u) == nullptr);
  CHECK(backend.svm_for(base + 64u, 1u) == nullptr);

  CHECK(!backend.lost());
  backend.shut_down();
  CHECK_EQ(::clReleaseContext(context), CL_SUCCESS); }

TEST(opencl_backend, a_buffer_over_a_bad_offset_with_nowhere_to_report_it_is_still_refused)
{ OpenClBackend backend;
  av::ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, memory_over(arena_region, sizeof arena_region)), av::Status::ok);
  CHECK(backend.adopt(blob));
  ClCommand beyond(av::clproto::kCmd_clCreateBufferWithProperties);
  buffer_with_properties_frame(beyond, true, sizeof arena_region, 16u, false);
  CHECK(beyond.decodes(backend));
  CHECK_EQ(beyond.address(), 0u);
  beyond.is_refused_at_every_shorter_length(backend);
  backend.abandon(blob);
  backend.shut_down(); }

TEST(opencl_backend, a_callback_the_host_refuses_gives_its_registration_back)
{ OpenClBackend backend;
  int            device = 0;
  backend.bind_signal(&nothing_to_wake, &device);

  for (std::uint32_t index = 0u; index < 200u; ++index) {
      ClCommand command(av::clproto::kCmd_clSetEventCallback);
      command.frame().write_pod<std::uint64_t>(0u);
      command.frame().write_varint<std::uint64_t>(0u);
      CHECK(command.decodes(backend));
      CHECK(command.result() != CL_OUT_OF_RESOURCES);
  }
  backend.shut_down(); }

TEST(opencl_backend, a_callback_on_a_session_with_no_device_to_wake_is_refused)
{ OpenClBackend backend;
  ClCommand command(av::clproto::kCmd_clSetEventCallback);
  command.frame().write_pod<std::uint64_t>(0u);
  command.frame().write_varint<std::uint64_t>(0u);
  CHECK(command.decodes(backend));
  CHECK_EQ(command.result(), CL_INVALID_OPERATION);
  command.is_refused_at_every_shorter_length(backend);
  backend.shut_down(); }

namespace {
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

    static constexpr std::size_t mapping_bytes = 64u;
    static constexpr std::uint32_t mappings = 300u;
    static constexpr std::size_t   buffer_bytes = mappings * mapping_bytes;

    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }
    cl_mem           buffer() const { return buffer_; }

private:
    cl_context       context_;
    cl_command_queue queue_;
    cl_mem           buffer_;
};

}

TEST(opencl_backend, shared_allocations_grow_and_are_refused_before_the_host_when_no_room_can_be_had)
{ HostOpenCl  host;
  HostSession session(host);
  if (session.context() == nullptr) return;
  av::MockPlatform platform;
  OpenClBackend    backend(platform);
  static void     *held[HostSession::mappings] = {};
  std::uint32_t    made = 0u;
  for (; made < 200u; ++made) {
      held[made] = backend.on_clSVMAlloc(session.context(), CL_MEM_READ_WRITE, 64u, 0u);
      if (held[made] == nullptr) return;
      CHECK(backend.svm_for(reinterpret_cast<std::uintptr_t>(held[made]), 64u) == held[made]);
  }
  platform.fail_allocation_at(1u);
  while (made < HostSession::mappings &&
         (held[made] = backend.on_clSVMAlloc(session.context(), CL_MEM_READ_WRITE, 64u, 0u)) != nullptr)
      made++;
  CHECK(made < HostSession::mappings);
  for (std::uint32_t index = 0u; index < made; ++index) backend.on_clSVMFree(session.context(), held[index]);
  void *const after = backend.on_clSVMAlloc(session.context(), CL_MEM_READ_WRITE, 64u, 0u);
  CHECK(after != nullptr);
  backend.on_clSVMFree(session.context(), after);
  CHECK(!backend.lost());
  backend.shut_down(); }

TEST(opencl_backend, mappings_grow_and_are_refused_before_the_host_when_no_room_can_be_had)
{ HostOpenCl  host;
  HostSession session(host);
  if (session.queue() == nullptr || session.buffer() == nullptr) return;
  av::MockPlatform platform;
  OpenClBackend    backend(platform);
  static void     *held[HostSession::mappings] = {};
  std::uint32_t    made = 0u;
  for (; made < 200u; ++made) {
      cl_int error = CL_SUCCESS;
      held[made] = backend.on_clEnqueueMapBuffer(session.queue(), session.buffer(), CL_TRUE, CL_MAP_WRITE,
                                                 made * HostSession::mapping_bytes, HostSession::mapping_bytes, 0u,
                                                 nullptr, nullptr, &error);
      if (held[made] == nullptr) return;
      CHECK_EQ(error, CL_SUCCESS);
  }
  platform.fail_allocation_at(1u);
  cl_int refused = CL_SUCCESS;
  while (made < HostSession::mappings &&
         (held[made] = backend.on_clEnqueueMapBuffer(session.queue(), session.buffer(), CL_TRUE, CL_MAP_WRITE,
                                                     made * HostSession::mapping_bytes, HostSession::mapping_bytes,
                                                     0u, nullptr, nullptr, &refused)) != nullptr)
      made++;
  CHECK(made < HostSession::mappings);
  CHECK_EQ(refused, CL_OUT_OF_HOST_MEMORY);
  platform.fail_allocation_at(1u);
  CHECK(backend.on_clEnqueueMapBuffer(session.queue(), session.buffer(), CL_TRUE, CL_MAP_WRITE, 0u,
                                      HostSession::mapping_bytes, 0u, nullptr, nullptr, nullptr) == nullptr);
  platform.fail_allocation_at(0u);
  for (std::uint32_t index = 0u; index < made; ++index)
      CHECK_EQ(backend.on_clEnqueueUnmapMemObject(session.queue(), session.buffer(), held[index], 0u, nullptr,
                                                  nullptr),
               CL_SUCCESS);
  CHECK_EQ(::clFinish(session.queue()), CL_SUCCESS);
  CHECK(!backend.lost());
  backend.shut_down(); }

TEST(opencl_backend, callback_registrations_grow_and_are_refused_only_with_no_room)
{ HostOpenCl  host;
  HostSession session(host);
  if (session.context() == nullptr) return;
  av::MockPlatform platform;
  OpenClBackend    backend(platform);
  int              device = 0;
  backend.bind_signal(&nothing_to_wake, &device);
  constexpr std::uint32_t many = 200u;
  static cl_event events[many + 64u] = {};
  for (cl_event &event : events) {
      cl_int error = CL_SUCCESS;
      event = ::clCreateUserEvent(session.context(), &error);
      if (event == nullptr) return;
  }
  for (std::uint32_t index = 0u; index < many; ++index) {
      const cl_int registered = backend.on_clSetEventCallback(events[index], CL_COMPLETE);
      if (registered != CL_SUCCESS) {
          for (cl_event event : events) CHECK_EQ(::clReleaseEvent(event), CL_SUCCESS);
          backend.shut_down();
          return;
      }
  }
  platform.fail_allocation_at(1u);
  cl_int        registered = CL_SUCCESS;
  std::uint32_t index = many;
  while (index < many + 64u && (registered = backend.on_clSetEventCallback(events[index], CL_COMPLETE)) == CL_SUCCESS)
      index++;
  CHECK_EQ(registered, CL_OUT_OF_HOST_MEMORY);
  for (cl_event event : events) CHECK_EQ(::clSetUserEventStatus(event, CL_COMPLETE), CL_SUCCESS);
  for (cl_event event : events) CHECK_EQ(::clReleaseEvent(event), CL_SUCCESS);
  backend.shut_down(); }
