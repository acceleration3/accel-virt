#include "device/abi/av_executor.h"
#include "executor/exported_memory.hpp"
#include "core/log/driver_log.hpp"
#include "executor/executor_log.hpp"
#include "executor/import_table.hpp"
#include "executor/owned_fd.hpp"
#include "executor/source_memory.hpp"
#include "executor/opencl/opencl_backend.hpp"
#include "executor/opengl/gl_context_attributes.hpp"
#include "executor/opengl/opengl_backend.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/shim/opencl_device.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libvulkan\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libnvidia\n"
           "leak:nvidia_icd\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libgallium\n"
           "leak:libLLVM\n"
           "leak:libz\n";
}

extern "C" const char *__asan_default_options(void);
extern "C" const char *__asan_default_options(void)
{
    return "detect_leaks=0";
}

using av::OpenClBackend;

namespace {
std::uint64_t detail = 0u;

constexpr std::uint64_t page = 4096u;

struct Lines {
    Lines() : memory(), imports(memory), sink(), executor(sink), log(executor.opencl()) {}

    av::MockPlatform memory;
    av::ImportTable  imports;
    av::RecordingLog sink;
    av::ExecutorLog  executor;
    av::DriverLog   &log;
};

class Hosts {
public:
    Hosts() : sink_(), log_(sink_), host_(log_), count_(0u)
    {
        host_.open();
        for (std::uint32_t i = 0u; i < host_.gpu_count(); i++) {
            const av::OpenClDevice *const device = host_.opencl_device(i);
            const av::GlDisplay *const    display = host_.gl_display(i);
            if (device == nullptr || display == nullptr) continue;
            devices_[count_] = device;
            displays_[count_] = display;
            host_.gpu_uuid(i, uuids_[count_]);
            count_++;
        }
    }
    ~Hosts() { host_.close(); }
    std::uint32_t                 count() const { return count_; }
    const av::OpenClDevice       *device(std::uint32_t index) const { return devices_[index]; }
    const av::GlDisplay          *display(std::uint32_t index) const { return displays_[index]; }
    const std::uint8_t           *uuid(std::uint32_t index) const { return uuids_[index]; }

private:
    av::RecordingLog        sink_;
    av::ExecutorLog         log_;
    av::LoaderHostApis      host_;
    const av::OpenClDevice *devices_[16] = {};
    const av::GlDisplay    *displays_[16] = {};
    std::uint8_t            uuids_[16][16] = {};
    std::uint32_t           count_;
};

class Exported {
public:
    Exported() : instance_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE), memory_(VK_NULL_HANDLE), fd_(-1) {}
    ~Exported()
    {
        if (fd_ >= 0) ::close(fd_);
        if (memory_ != VK_NULL_HANDLE) ::vkFreeMemory(device_, memory_, nullptr);
        if (device_ != VK_NULL_HANDLE) ::vkDestroyDevice(device_, nullptr);
        if (instance_ != VK_NULL_HANDLE) ::vkDestroyInstance(instance_, nullptr);
    }
    Exported(const Exported &) = delete;
    Exported &operator=(const Exported &) = delete;

    bool make(const std::uint8_t *uuid)
    {
        VkApplicationInfo application = {};
        application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application.apiVersion = (1u << 22) | (2u << 12);
        VkInstanceCreateInfo instance = {};
        instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance.pApplicationInfo = &application;
        if (::vkCreateInstance(&instance, nullptr, &instance_) != VK_SUCCESS) return false;
        std::uint32_t count = 0u;
        ::vkEnumeratePhysicalDevices(instance_, &count, nullptr);
        if (count == 0u) return false;
        VkPhysicalDevice found[8] = {};
        if (count > 8u) count = 8u;
        ::vkEnumeratePhysicalDevices(instance_, &count, found);
        VkPhysicalDevice physical = VK_NULL_HANDLE;
        for (std::uint32_t index = 0u; index < count; index++) {
            VkPhysicalDeviceIDProperties identity = {};
            identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
            VkPhysicalDeviceProperties2 properties = {};
            properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties.pNext = &identity;
            ::vkGetPhysicalDeviceProperties2(found[index], &properties);
            if (std::memcmp(identity.deviceUUID, uuid, VK_UUID_SIZE) == 0) physical = found[index];
        }
        if (physical == VK_NULL_HANDLE) return false;
        float                   priority = 1.0F;
        VkDeviceQueueCreateInfo queue = {};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueCount = 1u;
        queue.pQueuePriorities = &priority;
        const char *const extensions[] = {"VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf"};
        VkDeviceCreateInfo made = {};
        made.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        made.queueCreateInfoCount = 1u;
        made.pQueueCreateInfos = &queue;
        made.enabledExtensionCount = 2u;
        made.ppEnabledExtensionNames = extensions;
        if (::vkCreateDevice(physical, &made, nullptr, &device_) != VK_SUCCESS) return false;
        auto *const get_fd =
            reinterpret_cast<PFN_vkGetMemoryFdKHR>(::vkGetDeviceProcAddr(device_, "vkGetMemoryFdKHR"));
        if (get_fd == nullptr) return false;
        VkPhysicalDeviceMemoryProperties types = {};
        ::vkGetPhysicalDeviceMemoryProperties(physical, &types);
        VkExportMemoryAllocateInfo exportable = {};
        exportable.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        exportable.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        for (std::uint32_t index = 0u; index < types.memoryTypeCount; index++) {
            const VkMemoryPropertyFlags flags = types.memoryTypes[index].propertyFlags;
            if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0u ||
                (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0u)
                continue;
            VkMemoryAllocateInfo allocate = {};
            allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocate.pNext = &exportable;
            allocate.allocationSize = page;
            allocate.memoryTypeIndex = index;
            if (::vkAllocateMemory(device_, &allocate, nullptr, &memory_) != VK_SUCCESS) continue;
            VkMemoryGetFdInfoKHR asking = {};
            asking.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
            asking.memory = memory_;
            asking.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
            if (get_fd(device_, &asking, &fd_) == VK_SUCCESS) break;
            ::vkFreeMemory(device_, memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }
        if (fd_ < 0) return false;
        fill(0xABCD0000u);
        return true;
    }

    void fill(std::uint32_t first)
    {
        void *mapped = nullptr;
        CHECK_EQ(::vkMapMemory(device_, memory_, 0u, page, 0u, &mapped), VK_SUCCESS);
        auto *const words = static_cast<std::uint32_t *>(mapped);
        for (std::uint32_t index = 0u; index < page / sizeof(std::uint32_t); index++)
            words[index] = first + index;
        ::vkUnmapMemory(device_, memory_);
    }
    std::uint32_t word(std::uint32_t index)
    {
        void *mapped = nullptr;
        CHECK_EQ(::vkMapMemory(device_, memory_, 0u, page, 0u, &mapped), VK_SUCCESS);
        const std::uint32_t value = static_cast<std::uint32_t *>(mapped)[index];
        ::vkUnmapMemory(device_, memory_);
        return value;
    }
    int handle() const { return fd_; }

private:
    VkInstance     instance_;
    VkDevice       device_;
    VkDeviceMemory memory_;
    int            fd_;
};

cl_context guest_context(OpenClBackend &backend, const av::OpenClDevice &device)
{
    cl_device_id          named = static_cast<cl_device_id>(device.device());
    cl_context_properties props[] = {CL_CONTEXT_PLATFORM,
                                     reinterpret_cast<cl_context_properties>(device.platform()), 0};
    cl_int                made = CL_SUCCESS;
    cl_context const      context = backend.on_clCreateContext(props, 1u, &named, &made);
    if (context != nullptr) backend.made(av::clproto::HandleKind::context, context);
    return context;
}

}

namespace av {
struct OpenClBackendProbe {
    OpenClBackend &backend;

    bool refuse() const { return backend.bounced_.refusal(CL_OUT_OF_RESOURCES); }
    bool copy() const { return backend.copy_gl_objects(); }
    GlContext *vehicle() const { return backend.vehicle_.make_current() ? &backend.vehicle_ : nullptr; }
};

}

TEST(opencl_backend_device, a_descriptor_another_api_wrote_is_read_back_through_a_cl_mem)
{ Hosts hosts;
  if (hosts.count() == 0u) { std::fprintf(stderr, "this host runs no OpenCL beside OpenGL\n"); return; }
  for (std::uint32_t index = 0u; index < hosts.count(); index++) {
      Exported written;
      if (!written.make(hosts.uuid(index))) {
          std::fprintf(stderr, "gpu %u exports no writable dma-buf\n", index);
          continue;
      }
      Lines lines;
      OpenClBackend backend(lines.log);
      CHECK(backend.initialize(hosts.device(index), hosts.display(index), nullptr, lines.imports));
      cl_context const context = guest_context(backend, *hosts.device(index));
      if (context == nullptr) { std::fprintf(stderr, "gpu %u makes no context\n", index); backend.shut_down(); continue; }

      std::uint64_t shared = 0u;
      if (!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, av::SharedImageShape(),
                                 0u, shared, detail)) {
          std::fprintf(stderr, "gpu %u imports no shared object into OpenCL\n", index);
          backend.shut_down();
          continue;
      }
      CHECK(shared != 0u);
      CHECK(backend.handle_for(av::clproto::HandleKind::mem, shared) == reinterpret_cast<void *>(shared));

      cl_int             made = CL_SUCCESS;
      cl_command_queue const queue = ::clCreateCommandQueueWithProperties(
          context, static_cast<cl_device_id>(hosts.device(index)->device()), nullptr, &made);
      CHECK_EQ(made, CL_SUCCESS);
      std::uint32_t readback[8] = {0};
      CHECK_EQ(::clEnqueueReadBuffer(queue, reinterpret_cast<cl_mem>(shared), CL_TRUE, 0u, sizeof readback,
                                     readback, 0u, nullptr, nullptr),
               CL_SUCCESS);
      CHECK_EQ(readback[0], 0xABCD0000u);
      CHECK_EQ(readback[1], 0xABCD0001u);

      std::uint32_t sent[8] = {0};
      for (std::uint32_t i = 0u; i < 8u; i++) sent[i] = 0x5A5A0000u + i;
      CHECK_EQ(::clEnqueueWriteBuffer(queue, reinterpret_cast<cl_mem>(shared), CL_TRUE, 0u, sizeof sent, sent,
                                      0u, nullptr, nullptr),
               CL_SUCCESS);
      CHECK_EQ(::clFinish(queue), CL_SUCCESS);
      CHECK_EQ(written.word(0u), 0x5A5A0000u);
      CHECK_EQ(written.word(1u), 0x5A5A0001u);

      backend.release_object(AV_SHARED_MEMORY, shared, detail);
      CHECK(backend.handle_for(av::clproto::HandleKind::mem, shared) == nullptr);
      CHECK(::fcntl(written.handle(), F_GETFD) != -1);
      static_cast<void>(::clReleaseCommandQueue(queue));
      backend.shut_down();
      written.fill(0xABCD0000u);
  } }

TEST(opencl_backend_device, a_context_that_can_share_leaves_gl_sharing_in_the_device_s_extensions)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  cl_device_id const device = static_cast<cl_device_id>(hosts.device(0u)->device());
  char   host_list[4096] = {};
  size_t host_bytes = 0u;
  CHECK_EQ(::clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof host_list, host_list, &host_bytes),
           CL_SUCCESS);
  char   answered[4096] = {};
  size_t bytes = 0u;
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof answered, answered, &bytes),
           CL_SUCCESS);
  CHECK_EQ(bytes, std::strlen(answered) + 1u);
  CHECK(std::strncmp(answered, host_list, host_bytes - 1u) == 0);
  CHECK(std::strstr(answered, " cl_khr_external_memory") != nullptr);
  if (std::strstr(host_list, "cl_khr_gl_sharing") == nullptr)
      std::fprintf(stderr, "this host's OpenCL lists no cl_khr_gl_sharing\n");
  backend.shut_down(); }

TEST(opencl_backend_device, an_import_with_no_guest_context_is_refused_and_costs_the_descriptor_nothing)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  Exported written;
  if (!written.make(hosts.uuid(0u))) { backend.shut_down(); return; }

  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, av::SharedImageShape(), 0u, made, detail));
  CHECK_EQ(made, 0u);

  if (guest_context(backend, *hosts.device(0u)) == nullptr) { backend.shut_down(); return; }
  if (backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, av::SharedImageShape(), 0u, made, detail)) {
      CHECK(made != 0u);
      backend.release_object(AV_SHARED_MEMORY, made, detail);
  }
  if (backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, av::SharedImageShape(), 0u, made, detail))
      CHECK(backend.handle_for(av::clproto::HandleKind::mem, made) != nullptr);
  backend.shut_down(); }

TEST(opencl_backend_device, a_property_list_longer_than_the_drivers_own_is_refused)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  cl_device_id named = static_cast<cl_device_id>(hosts.device(0u)->device());

  cl_context_properties overlong[34] = {};
  for (std::uint32_t at = 0u; at + 1u < 32u; at += 2u) {
      overlong[at] = CL_CONTEXT_PLATFORM;
      overlong[at + 1u] = reinterpret_cast<cl_context_properties>(hosts.device(0u)->platform());
  }
  cl_int error = CL_SUCCESS;
  CHECK(backend.on_clCreateContext(overlong, 1u, &named, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  CHECK(backend.on_clCreateContext(overlong, 1u, &named, nullptr) == nullptr);

  cl_context const first = guest_context(backend, *hosts.device(0u));
  if (first == nullptr) { backend.shut_down(); return; }
  cl_context_properties list[] = {CL_CONTEXT_PLATFORM,
                                  reinterpret_cast<cl_context_properties>(hosts.device(0u)->platform()), 0};
  cl_context const second = backend.on_clCreateContext(list, 1u, &named, nullptr);
  CHECK(second != nullptr);
  backend.made(av::clproto::HandleKind::context, second);
  CHECK_EQ(backend.on_clReleaseContext(second), CL_SUCCESS);
  backend.released(av::clproto::HandleKind::context, second);
  backend.shut_down(); }

TEST(opencl_backend_device, an_import_of_the_wrong_shape_makes_nothing)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Exported written;
  if (!written.make(hosts.uuid(0u))) return;

  Lines lines;

  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  cl_context const context = guest_context(backend, *hosts.device(0u));
  if (context == nullptr) { backend.shut_down(); return; }

  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MONITORED_FENCE, AV_SHARED_HANDLE_DMA_BUF, written.handle(), 0u, av::SharedImageShape(),
                               0u, made, detail));
  CHECK(!backend.import_object(AV_SHARED_NATIVE_FENCE, AV_SHARED_HANDLE_DMA_BUF, written.handle(), 0u, av::SharedImageShape(),
                               0u, made, detail));
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_OPAQUE_FD, written.handle(), page, av::SharedImageShape(), 0u, made, detail));
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, -1, page, av::SharedImageShape(), 0u, made, detail));
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), 0u, av::SharedImageShape(), 0u, made, detail));
  CHECK(!backend.import_object(AV_SHARED_NATIVE_FENCE + 1u, AV_SHARED_HANDLE_DMA_BUF, written.handle(),
                               page, av::SharedImageShape(), 0u, made, detail));
  CHECK_EQ(made, 0u);
  std::uint32_t handle_kind = 9u;
  av::OwnedFd   fd;
  CHECK(!backend.export_object(AV_SHARED_MEMORY, 1u, handle_kind, fd));
  CHECK_EQ(handle_kind, 0u);
  CHECK(!fd.held());
  backend.shut_down(); }

TEST(opencl_backend_device, each_import_is_released_with_the_gl_objects_it_answered)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Exported written;
  if (!written.make(hosts.uuid(0u))) return;

  Lines lines;

  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  cl_context const context = guest_context(backend, *hosts.device(0u));
  if (context == nullptr) { backend.shut_down(); return; }
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  if (!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, av::SharedImageShape(), 0u, first, detail)) {
      backend.shut_down();
      return;
  }
  const std::uint64_t first_detail = detail;
  CHECK(backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, av::SharedImageShape(), 0u, second, detail));
  const std::uint64_t second_detail = detail;
  CHECK(first != second);
  CHECK(first_detail != second_detail);
  CHECK((first_detail >> 32u) != 0u);
  CHECK((first_detail & 0xFFFFFFFFu) != 0u);
  backend.release_object(AV_SHARED_MEMORY, second, second_detail);
  backend.release_object(AV_SHARED_MEMORY, first, first_detail);
  CHECK(backend.handle_for(av::clproto::HandleKind::mem, first) == nullptr);
  CHECK(backend.handle_for(av::clproto::HandleKind::mem, second) == nullptr);
  backend.shut_down();
  CHECK(::fcntl(written.handle(), F_GETFD) != -1); }

TEST(opencl_backend_device, a_write_has_taken_its_bytes_by_the_time_it_returns_whatever_the_guest_asked)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  cl_context const context = guest_context(backend, *hosts.device(0u));
  if (context == nullptr) { backend.shut_down(); return; }
  cl_int                 error = CL_SUCCESS;
  const cl_command_queue queue = ::clCreateCommandQueueWithProperties(
      context, static_cast<cl_device_id>(hosts.device(0u)->device()), nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  constexpr std::size_t bytes = std::size_t{4} << 20;
  cl_mem const          target = ::clCreateBuffer(context, CL_MEM_READ_WRITE, bytes, nullptr, &error);
  cl_mem const          busy = ::clCreateBuffer(context, CL_MEM_READ_WRITE, bytes, nullptr, &error);
  CHECK(target != nullptr && busy != nullptr);
  static std::uint8_t source[bytes];
  static std::uint8_t read_back[bytes];
  for (std::uint32_t copy = 0u; copy < 32u; ++copy)
      CHECK_EQ(::clEnqueueCopyBuffer(queue, busy, target, 0u, 0u, bytes, 0u, nullptr, nullptr), CL_SUCCESS);
  std::memset(source, 0xAB, bytes);
  CHECK_EQ(backend.on_clEnqueueWriteBuffer(queue, target, CL_FALSE, 0u, bytes, source, 0u, nullptr, nullptr),
           CL_SUCCESS);
  std::memset(source, 0x11, bytes);
  CHECK_EQ(::clEnqueueReadBuffer(queue, target, CL_TRUE, 0u, bytes, read_back, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(read_back[0], 0xABu);
  CHECK_EQ(read_back[bytes - 1u], 0xABu);

  const cl_image_format format = {CL_RGBA, CL_UNSIGNED_INT8};
  cl_image_desc         desc = {};
  desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  desc.image_width = 256u;
  desc.image_height = 256u;
  cl_mem const image = ::clCreateImage(context, CL_MEM_READ_WRITE, &format, &desc, nullptr, &error);
  CHECK(image != nullptr);
  const std::size_t origin[3] = {0u, 0u, 0u}, region[3] = {256u, 256u, 1u};
  for (std::uint32_t copy = 0u; copy < 32u; ++copy)
      CHECK_EQ(::clEnqueueCopyBuffer(queue, busy, target, 0u, 0u, bytes, 0u, nullptr, nullptr), CL_SUCCESS);
  std::memset(source, 0x5C, 256u * 256u * 4u);
  CHECK_EQ(backend.on_clEnqueueWriteImage(queue, image, CL_FALSE, origin, region, 0u, 0u, source, 0u, nullptr,
                                          nullptr),
           CL_SUCCESS);
  std::memset(source, 0x22, 256u * 256u * 4u);
  CHECK_EQ(::clEnqueueReadImage(queue, image, CL_TRUE, origin, region, 0u, 0u, read_back, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(read_back[0], 0x5Cu);
  CHECK_EQ(read_back[256u * 256u * 4u - 1u], 0x5Cu);

  const std::size_t buffer_origin[3] = {0u, 0u, 0u}, rect[3] = {64u, 4u, 1u};
  std::memset(source, 0x7E, 64u * 4u);
  CHECK_EQ(backend.on_clEnqueueWriteBufferRect(queue, target, CL_FALSE, buffer_origin, buffer_origin, rect, 0u, 0u,
                                               0u, 0u, source, 0u, nullptr, nullptr),
           CL_SUCCESS);
  std::memset(source, 0x33, 64u * 4u);
  CHECK_EQ(::clEnqueueReadBuffer(queue, target, CL_TRUE, 0u, 64u * 4u, read_back, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(read_back[0], 0x7Eu);
  CHECK_EQ(read_back[64u * 4u - 1u], 0x7Eu);

  ::clReleaseMemObject(image);
  ::clReleaseMemObject(busy);
  ::clReleaseMemObject(target);
  ::clReleaseCommandQueue(queue);
  backend.shut_down(); }

TEST(opencl_backend_device, a_read_has_written_its_bytes_by_the_time_it_returns_whatever_the_guest_asked)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  cl_context const context = guest_context(backend, *hosts.device(0u));
  if (context == nullptr) { backend.shut_down(); return; }
  constexpr std::size_t bytes = std::size_t{1} << 20;
  alignas(4096) static std::uint8_t lent[bytes];
  av::SourceMemory memory(1u, 1u, 0u, AV_MEMORY_WRITE_BACK, av::ExportedMemory(1u, -1, lent, sizeof lent));
  std::uint64_t arena = 0u;
  CHECK(backend.adopt(memory, arena));
  CHECK_EQ(arena, reinterpret_cast<std::uintptr_t>(lent));
  cl_int                 error = CL_SUCCESS;
  const cl_command_queue queue = ::clCreateCommandQueueWithProperties(
      context, static_cast<cl_device_id>(hosts.device(0u)->device()), nullptr, &error);
  cl_mem const source = ::clCreateBuffer(context, CL_MEM_READ_WRITE, bytes, nullptr, &error);
  cl_mem const busy = ::clCreateBuffer(context, CL_MEM_READ_WRITE, bytes, nullptr, &error);
  CHECK(source != nullptr && busy != nullptr);
  const std::uint8_t pattern = 0x5Au;
  CHECK_EQ(::clEnqueueFillBuffer(queue, busy, &pattern, 1u, 0u, bytes, 0u, nullptr, nullptr), CL_SUCCESS);
  for (std::uint32_t copy = 0u; copy < 32u; ++copy)
      CHECK_EQ(::clEnqueueCopyBuffer(queue, busy, source, 0u, 0u, bytes, 0u, nullptr, nullptr), CL_SUCCESS);
  std::memset(lent, 0, sizeof lent);
  void *const into = lent;
  CHECK(into == lent);
  CHECK_EQ(backend.on_clEnqueueReadBuffer(queue, source, CL_FALSE, 0u, bytes, into, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(lent[0], pattern);
  CHECK_EQ(lent[bytes - 1u], pattern);

  std::memset(lent, 0, sizeof lent);
  cl_event read = nullptr;
  CHECK_EQ(backend.on_clEnqueueReadBuffer(queue, source, CL_FALSE, 0u, bytes, into, 0u, nullptr, &read),
           CL_SUCCESS);
  CHECK_EQ(lent[bytes - 1u], pattern);
  CHECK(read != nullptr);
  std::memset(lent, 0, sizeof lent);
  CHECK_EQ(backend.on_clEnqueueReadBuffer(queue, source, CL_FALSE, 0u, bytes, into, 1u, &read, nullptr),
           CL_SUCCESS);
  CHECK_EQ(lent[bytes - 1u], pattern);
  ::clReleaseEvent(read);

  const cl_image_format format = {CL_RGBA, CL_UNSIGNED_INT8};
  cl_image_desc         desc = {};
  desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  desc.image_width = 64u;
  desc.image_height = 64u;
  cl_mem const image = ::clCreateImage(context, CL_MEM_READ_WRITE, &format, &desc, nullptr, &error);
  CHECK(image != nullptr);
  const std::size_t origin[3] = {0u, 0u, 0u}, region[3] = {64u, 64u, 1u};
  const unsigned    fill[4] = {0x31u, 0x31u, 0x31u, 0x31u};
  CHECK_EQ(::clEnqueueFillImage(queue, image, fill, origin, region, 0u, nullptr, nullptr), CL_SUCCESS);
  for (std::uint32_t copy = 0u; copy < 32u; ++copy)
      CHECK_EQ(::clEnqueueCopyBuffer(queue, busy, source, 0u, 0u, bytes, 0u, nullptr, nullptr), CL_SUCCESS);
  std::memset(lent, 0, sizeof lent);
  CHECK_EQ(backend.on_clEnqueueReadImage(queue, image, CL_FALSE, origin, region, 0u, 0u, into, 0u, nullptr,
                                         nullptr),
           CL_SUCCESS);
  CHECK_EQ(lent[0], 0x31u);
  CHECK_EQ(lent[64u * 64u * 4u - 1u], 0x31u);

  const std::size_t rect[3] = {64u, 4u, 1u};
  for (std::uint32_t copy = 0u; copy < 32u; ++copy)
      CHECK_EQ(::clEnqueueCopyBuffer(queue, busy, source, 0u, 0u, bytes, 0u, nullptr, nullptr), CL_SUCCESS);
  std::memset(lent, 0, sizeof lent);
  CHECK_EQ(backend.on_clEnqueueReadBufferRect(queue, source, CL_FALSE, origin, origin, rect, 0u, 0u, 0u, 0u, into,
                                              0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(lent[0], pattern);
  CHECK_EQ(lent[64u * 4u - 1u], pattern);

  ::clReleaseMemObject(image);
  ::clReleaseMemObject(busy);
  ::clReleaseMemObject(source);
  ::clReleaseCommandQueue(queue);
  backend.abandon(memory, arena);
  backend.shut_down(); }

TEST(opencl_backend_device, a_rectangle_the_host_refused_into_the_arena_is_bounced_into_it)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  cl_context const context = guest_context(backend, *hosts.device(0u));
  if (context == nullptr) { backend.shut_down(); return; }
  alignas(4096) static std::uint8_t lent[64u * 1024u];
  av::SourceMemory memory(1u, 1u, 0u, AV_MEMORY_WRITE_BACK, av::ExportedMemory(1u, -1, lent, sizeof lent));
  std::uint64_t arena = 0u;
  CHECK(backend.adopt(memory, arena));
  CHECK_EQ(arena, reinterpret_cast<std::uintptr_t>(lent));
  cl_int                 error = CL_SUCCESS;
  const cl_command_queue queue = ::clCreateCommandQueueWithProperties(
      context, static_cast<cl_device_id>(hosts.device(0u)->device()), nullptr, &error);
  constexpr std::size_t bytes = 16u * 1024u;
  static std::uint8_t   pattern[bytes];
  for (std::size_t i = 0u; i < bytes; i++) pattern[i] = static_cast<std::uint8_t>(i % 251u);
  cl_mem const source = ::clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bytes, pattern, &error);
  CHECK(source != nullptr);
  av::OpenClBackendProbe probe{backend};
  CHECK(probe.refuse());
  const std::size_t from[3] = {8u, 2u, 0u}, at[3] = {4u, 1u, 0u}, region[3] = {32u, 3u, 1u};
  std::memset(lent, 0xEE, sizeof lent);
  void *const into = lent;
  CHECK_EQ(backend.on_clEnqueueReadBufferRect(queue, source, CL_FALSE, from, at, region, 128u, 0u, 64u, 0u, into, 0u,
                                              nullptr, nullptr),
           CL_SUCCESS);
  std::size_t rows_right = 0u;
  for (std::size_t y = 0u; y < region[1]; y++)
      for (std::size_t x = 0u; x < region[0]; x++)
          rows_right += lent[(at[1] + y) * 64u + at[0] + x] ==
                                static_cast<std::uint8_t>(((from[1] + y) * 128u + from[0] + x) % 251u)
                            ? 1u
                            : 0u;
  CHECK_EQ(rows_right, region[0] * region[1]);
  CHECK_EQ(lent[at[1] * 64u + at[0] - 1u], 0xEEu);
  CHECK_EQ(lent[at[1] * 64u + at[0] + region[0]], 0xEEu);
  CHECK_EQ(lent[0], 0xEEu);

  static std::uint8_t own[1024];
  std::memset(own, 0xEE, sizeof own);
  CHECK_EQ(backend.on_clEnqueueReadBufferRect(queue, source, CL_FALSE, from, at, region, 128u, 0u, 64u, 0u, own, 0u,
                                              nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(own[at[1] * 64u + at[0]], static_cast<std::uint8_t>((from[1] * 128u + from[0]) % 251u));

  ::clReleaseMemObject(source);
  ::clReleaseCommandQueue(queue);
  backend.abandon(memory, arena);
  backend.shut_down(); }

TEST(opencl_backend_device, on_clReleaseMemObject_of_a_mapped_buffer_forgets_its_mapping)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  cl_context const context = guest_context(backend, *hosts.device(0u));
  if (context == nullptr) { backend.shut_down(); return; }
  cl_int                 error = CL_SUCCESS;
  const cl_command_queue queue = ::clCreateCommandQueueWithProperties(
      context, static_cast<cl_device_id>(hosts.device(0u)->device()), nullptr, &error);
  cl_mem const released = ::clCreateBuffer(context, CL_MEM_READ_WRITE, 4096u, nullptr, &error);
  cl_mem const kept = ::clCreateBuffer(context, CL_MEM_READ_WRITE, 4096u, nullptr, &error);
  CHECK(queue != nullptr && released != nullptr && kept != nullptr);
  void *const gone = backend.on_clEnqueueMapBuffer(queue, released, CL_TRUE, CL_MAP_WRITE, 0u, 4096u, 0u, nullptr,
                                                   nullptr, &error);
  void *const stays = backend.on_clEnqueueMapBuffer(queue, kept, CL_TRUE, CL_MAP_WRITE, 0u, 4096u, 0u, nullptr,
                                                    nullptr, &error);
  CHECK(gone != nullptr && stays != nullptr);
  const auto address = [](void *pointer) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)); };
  CHECK(backend.svm_for(address(gone), 1u) != nullptr);
  CHECK_EQ(backend.on_clReleaseMemObject(released), CL_SUCCESS);
  CHECK(backend.svm_for(address(gone), 1u) == nullptr);
  CHECK(backend.svm_for(address(stays), 1u) != nullptr);
  CHECK_EQ(backend.on_clEnqueueUnmapMemObject(queue, kept, stays, 0u, nullptr, nullptr), CL_SUCCESS);
  ::clReleaseMemObject(kept);
  ::clReleaseCommandQueue(queue);
  backend.shut_down(); }

TEST(opencl_backend_device, a_kernel_runs_without_the_guest_flushing_it)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  cl_context const context = guest_context(backend, *hosts.device(0u));
  if (context == nullptr) { backend.shut_down(); return; }
  cl_int                 error = CL_SUCCESS;
  cl_device_id const     device = static_cast<cl_device_id>(hosts.device(0u)->device());
  const cl_command_queue queue = ::clCreateCommandQueueWithProperties(context, device, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  const char       *source = "__kernel void fill(__global uint *out) { out[get_global_id(0)] = 7u; }";
  cl_program const  program = ::clCreateProgramWithSource(context, 1u, &source, nullptr, &error);
  CHECK_EQ(::clBuildProgram(program, 1u, &device, nullptr, nullptr, nullptr), CL_SUCCESS);
  cl_kernel const kernel = ::clCreateKernel(program, "fill", &error);
  cl_mem const    out = ::clCreateBuffer(context, CL_MEM_READ_WRITE, 4096u * 4u, nullptr, &error);
  CHECK_EQ(::clSetKernelArg(kernel, 0u, sizeof out, &out), CL_SUCCESS);
  const std::size_t global = 4096u;
  cl_event          done = nullptr;
  CHECK_EQ(backend.on_clEnqueueNDRangeKernel(queue, kernel, 1u, nullptr, &global, nullptr, 0u, nullptr, &done),
           CL_SUCCESS);
  cl_int status = CL_QUEUED;
  for (std::uint32_t look = 0u; look < 5000u && status != CL_COMPLETE; look++) {
      CHECK_EQ(::clGetEventInfo(done, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status, nullptr),
               CL_SUCCESS);
      if (status != CL_COMPLETE) ::usleep(1000u);
  }
  CHECK_EQ(status, CL_COMPLETE);
  for (std::uint32_t again = 0u; again < 8u; again++)
      CHECK_EQ(backend.on_clEnqueueNDRangeKernel(queue, kernel, 1u, nullptr, &global, nullptr, 0u, nullptr,
                                                 nullptr),
               CL_SUCCESS);
  CHECK_EQ(backend.on_clEnqueueTask(queue, kernel, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(backend.on_clFlush(queue), CL_SUCCESS);
  CHECK_EQ(backend.on_clFinish(queue), CL_SUCCESS);
  std::uint32_t first = 0u;
  CHECK_EQ(::clEnqueueReadBuffer(queue, out, CL_TRUE, 0u, sizeof first, &first, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(first, 7u);
  ::clReleaseEvent(done);
  ::clReleaseMemObject(out);
  ::clReleaseKernel(kernel);
  ::clReleaseProgram(program);
  ::clReleaseCommandQueue(queue);
  backend.shut_down(); }

TEST(opencl_backend_device, an_event_the_guest_asks_after_is_flushed_on_its_way)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  cl_context const context = guest_context(backend, *hosts.device(0u));
  if (context == nullptr) { backend.shut_down(); return; }
  cl_int                 error = CL_SUCCESS;
  cl_device_id const     device = static_cast<cl_device_id>(hosts.device(0u)->device());
  const cl_command_queue queue = ::clCreateCommandQueueWithProperties(context, device, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  cl_mem const       buffer = ::clCreateBuffer(context, CL_MEM_READ_WRITE, 4096u, nullptr, &error);
  const std::uint8_t pattern = 0x3Cu;
  CHECK_EQ(::clEnqueueFillBuffer(queue, buffer, &pattern, 1u, 0u, 4096u, 0u, nullptr, nullptr), CL_SUCCESS);
  cl_event marker = nullptr;
  CHECK_EQ(backend.on_clEnqueueMarkerWithWaitList(queue, 0u, nullptr, &marker), CL_SUCCESS);
  cl_int status = CL_QUEUED;
  for (std::uint32_t look = 0u; look < 5000u && status != CL_COMPLETE; look++) {
      CHECK_EQ(backend.on_clGetEventInfo(marker, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status, nullptr),
               CL_SUCCESS);
      if (status != CL_COMPLETE) ::usleep(1000u);
  }
  CHECK_EQ(status, CL_COMPLETE);
  cl_event const user = ::clCreateUserEvent(context, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(backend.on_clGetEventInfo(user, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status, nullptr),
           CL_SUCCESS);
  CHECK_EQ(status, CL_SUBMITTED);
  CHECK_EQ(::clSetUserEventStatus(user, CL_COMPLETE), CL_SUCCESS);
  ::clReleaseEvent(user);
  ::clReleaseEvent(marker);
  ::clReleaseMemObject(buffer);
  ::clReleaseCommandQueue(queue);
  backend.shut_down(); }

namespace {
std::uint32_t gl_buffer(av::GlContext &gl, const void *values, std::size_t bytes)
{
    static_cast<void>(gl.make_current());
    auto const gen = reinterpret_cast<void (*)(GLsizei, GLuint *)>(gl.resolve("glGenBuffers"));
    auto const bind = reinterpret_cast<void (*)(GLenum, GLuint)>(gl.resolve("glBindBuffer"));
    auto const data = reinterpret_cast<void (*)(GLenum, std::intptr_t, const void *, GLenum)>(gl.resolve("glBufferData"));
    auto const finish = reinterpret_cast<void (*)()>(gl.resolve("glFinish"));
    GLuint name = 0u;
    gen(1, &name);
    bind(0x8892u, name);
    data(0x8892u, static_cast<std::intptr_t>(bytes), values, 0x88E4u);
    bind(0x8892u, 0u);
    finish();
    return name;
}

void read_gl_buffer(av::GlContext &gl, std::uint32_t name, void *into, std::size_t bytes)
{
    static_cast<void>(gl.make_current());
    auto const bind = reinterpret_cast<void (*)(GLenum, GLuint)>(gl.resolve("glBindBuffer"));
    auto const get =
        reinterpret_cast<void (*)(GLenum, std::intptr_t, std::intptr_t, void *)>(gl.resolve("glGetBufferSubData"));
    bind(0x8892u, name);
    get(0x8892u, 0, static_cast<std::intptr_t>(bytes), into);
    bind(0x8892u, 0u);
}

bool lists(const char *list, const char *name)
{
    const std::size_t length = std::strlen(name);
    for (const char *at = std::strstr(list, name); at != nullptr; at = std::strstr(at + 1, name))
        if ((at == list || at[-1] == ' ') && (at[length] == ' ' || at[length] == '\0')) return true;
    return false;
}

}

TEST(opencl_backend_device, the_copy_path_says_gl_sharing_and_imports_nothing)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Exported written;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  av::OpenClBackendProbe probe{backend};
  if (probe.vehicle() == nullptr) { backend.shut_down(); return; }
  CHECK(probe.copy());
  cl_device_id const device = static_cast<cl_device_id>(hosts.device(0u)->device());
  char               answered[4096] = {};
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof answered, answered, nullptr), CL_SUCCESS);
  char host_list[4096] = {};
  CHECK_EQ(::clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof host_list, host_list, nullptr), CL_SUCCESS);
  CHECK_EQ(lists(answered, "cl_khr_gl_sharing"), lists(host_list, "cl_khr_gl_sharing"));
  CHECK(!lists(answered, "cl_khr_external_memory"));
  if (guest_context(backend, *hosts.device(0u)) != nullptr && written.make(hosts.uuid(0u))) {
      std::uint64_t made = 9u;
      CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page,
                                   av::SharedImageShape(), 0u, made, detail));
      CHECK_EQ(made, 0u);
  }
  backend.shut_down(); }

TEST(opencl_backend_device, the_copy_path_carries_gl_objects_through_the_backend)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr, lines.imports));
  av::OpenClBackendProbe probe{backend};
  av::GlContext *const   gl = probe.vehicle();
  if (gl == nullptr || !probe.copy()) { backend.shut_down(); return; }
  cl_context const context = guest_context(backend, *hosts.device(0u));
  CHECK(context != nullptr);
  cl_context_properties named[8] = {};
  std::size_t           bytes = 0u;
  CHECK_EQ(::clGetContextInfo(context, CL_CONTEXT_PROPERTIES, sizeof named, named, &bytes), CL_SUCCESS);
  CHECK_EQ(bytes, 3u * sizeof(cl_context_properties));

  std::uint32_t values[64];
  for (std::uint32_t at = 0u; at < 64u; at++) values[at] = at * 3u + 1u;
  const std::uint32_t buffer = gl_buffer(*gl, values, sizeof values);
  cl_int              error = CL_INVALID_VALUE;
  cl_mem const        mem = backend.on_clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, buffer, &error);
  CHECK(mem != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  backend.made(av::clproto::HandleKind::mem, mem);
  cl_command_queue const queue = ::clCreateCommandQueueWithProperties(
      context, static_cast<cl_device_id>(hosts.device(0u)->device()), nullptr, &error);
  CHECK_EQ(backend.on_clEnqueueAcquireGLObjects(queue, 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  std::uint32_t seen[64] = {};
  CHECK_EQ(::clEnqueueReadBuffer(queue, mem, CL_TRUE, 0u, sizeof seen, seen, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK(std::memcmp(seen, values, sizeof values) == 0);
  for (std::uint32_t &value : values) value += 1000u;
  CHECK_EQ(::clEnqueueWriteBuffer(queue, mem, CL_TRUE, 0u, sizeof values, values, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(backend.on_clEnqueueReleaseGLObjects(queue, 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  read_gl_buffer(*gl, buffer, seen, sizeof seen);
  CHECK(std::memcmp(seen, values, sizeof values) == 0);
  cl_gl_object_type type = 0u;
  cl_GLuint         name = 0u;
  CHECK_EQ(backend.on_clGetGLObjectInfo(mem, &type, &name), CL_SUCCESS);
  CHECK_EQ(type, static_cast<cl_gl_object_type>(CL_GL_OBJECT_BUFFER));
  CHECK_EQ(name, buffer);
  cl_GLenum target = 0u;
  CHECK_EQ(backend.on_clGetGLTextureInfo(mem, CL_GL_TEXTURE_TARGET, sizeof target, &target, nullptr),
           CL_INVALID_GL_OBJECT);
  CHECK_EQ(backend.on_clReleaseMemObject(mem), CL_SUCCESS);
  backend.released(av::clproto::HandleKind::mem, mem);
  CHECK_EQ(backend.on_clGetGLObjectInfo(mem, &type, &name), CL_INVALID_GL_OBJECT);

  static_cast<void>(gl->make_current());
  auto const gen_textures = reinterpret_cast<void (*)(GLsizei, GLuint *)>(gl->resolve("glGenTextures"));
  auto const bind_texture = reinterpret_cast<void (*)(GLenum, GLuint)>(gl->resolve("glBindTexture"));
  auto const image2 = reinterpret_cast<void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                                                const void *)>(gl->resolve("glTexImage2D"));
  auto const image3 = reinterpret_cast<void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum,
                                                GLenum, const void *)>(gl->resolve("glTexImage3D"));
  GLuint textures[2] = {};
  gen_textures(2, textures);
  bind_texture(0x0DE1u, textures[0]);
  image2(0x0DE1u, 0, 0x8058, 4, 4, 0, 0x1908u, 0x1401u, nullptr);
  bind_texture(0x0DE1u, 0u);
  bind_texture(0x806Fu, textures[1]);
  image3(0x806Fu, 0, 0x8058, 4, 4, 2, 0, 0x1908u, 0x1401u, nullptr);
  bind_texture(0x806Fu, 0u);
  cl_mem const flat = backend.on_clCreateFromGLTexture2D(context, CL_MEM_READ_ONLY, 0x0DE1u, 0, textures[0], &error);
  CHECK(flat != nullptr);
  cl_mem const deep = backend.on_clCreateFromGLTexture3D(context, CL_MEM_READ_ONLY, 0x806Fu, 0, textures[1], &error);
  CHECK(deep != nullptr);
  CHECK_EQ(backend.on_clGetGLTextureInfo(deep, CL_GL_TEXTURE_TARGET, sizeof target, &target, nullptr), CL_SUCCESS);
  CHECK_EQ(target, 0x806Fu);
  static_cast<void>(gl->make_current());
  auto const gen_renderbuffers = reinterpret_cast<void (*)(GLsizei, GLuint *)>(gl->resolve("glGenRenderbuffers"));
  auto const bind_renderbuffer = reinterpret_cast<void (*)(GLenum, GLuint)>(gl->resolve("glBindRenderbuffer"));
  auto const storage =
      reinterpret_cast<void (*)(GLenum, GLenum, GLsizei, GLsizei)>(gl->resolve("glRenderbufferStorage"));
  GLuint renderbuffer = 0u;
  gen_renderbuffers(1, &renderbuffer);
  bind_renderbuffer(0x8D41u, renderbuffer);
  storage(0x8D41u, 0x8058u, 4, 4);
  bind_renderbuffer(0x8D41u, 0u);
  cl_mem const drawn = backend.on_clCreateFromGLRenderbuffer(context, CL_MEM_READ_ONLY, renderbuffer, &error);
  if (drawn != nullptr) {
      CHECK_EQ(backend.on_clGetGLObjectInfo(drawn, &type, &name), CL_SUCCESS);
      CHECK_EQ(type, static_cast<cl_gl_object_type>(CL_GL_OBJECT_RENDERBUFFER));
  }
  ::clReleaseCommandQueue(queue);
  backend.shut_down();
  if (drawn != nullptr) ::clReleaseMemObject(drawn);
  ::clReleaseMemObject(deep);
  ::clReleaseMemObject(flat); }

TEST(opencl_backend_device, a_gl_context_s_cl_context_reaches_its_objects_by_copies)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Lines lines;
  av::OpenGlBackend guest_gl(lines.executor.opengl());
  if (!guest_gl.initialize(hosts.display(0u), nullptr, nullptr)) return;
  { OpenClBackend refused(lines.log);
    CHECK(!refused.initialize(hosts.device(0u), nullptr, &guest_gl, lines.imports)); }
  OpenClBackend backend(lines.log);
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), &guest_gl, lines.imports));
  av::OpenClBackendProbe probe{backend};
  CHECK(probe.copy());
  av::GlContext                   application;
  const av::GlContextAttributes   plain(nullptr);
  CHECK(application.open(*hosts.display(0u), plain, &guest_gl.context()));
  std::uint8_t values[256];
  for (std::uint32_t at = 0u; at < sizeof values; at++) values[at] = static_cast<std::uint8_t>(at ^ 0x5Au);
  const std::uint32_t buffer = gl_buffer(application, values, sizeof values);
  cl_context const    context = guest_context(backend, *hosts.device(0u));
  cl_int              error = CL_SUCCESS;
  cl_mem const        mem = backend.on_clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, buffer, &error);
  CHECK(mem != nullptr);
  backend.made(av::clproto::HandleKind::mem, mem);
  cl_command_queue const queue = ::clCreateCommandQueueWithProperties(
      context, static_cast<cl_device_id>(hosts.device(0u)->device()), nullptr, &error);
  CHECK_EQ(backend.on_clEnqueueAcquireGLObjects(queue, 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  std::uint8_t seen[256] = {};
  CHECK_EQ(::clEnqueueReadBuffer(queue, mem, CL_TRUE, 0u, sizeof seen, seen, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK(std::memcmp(seen, values, sizeof values) == 0);
  std::memset(values, 0x77, sizeof values);
  CHECK_EQ(::clEnqueueWriteBuffer(queue, mem, CL_TRUE, 0u, sizeof values, values, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(backend.on_clEnqueueReleaseGLObjects(queue, 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  read_gl_buffer(application, buffer, seen, sizeof seen);
  CHECK(std::memcmp(seen, values, sizeof values) == 0);
  ::clReleaseCommandQueue(queue);
  CHECK_EQ(::clRetainMemObject(mem), CL_SUCCESS);
  backend.shut_down();
  cl_uint left = 0u;
  CHECK_EQ(::clGetMemObjectInfo(mem, CL_MEM_REFERENCE_COUNT, sizeof left, &left, nullptr), CL_SUCCESS);
  CHECK_EQ(left, 1u);
  ::clReleaseMemObject(mem);
  application.close();
  guest_gl.shut_down(); }
