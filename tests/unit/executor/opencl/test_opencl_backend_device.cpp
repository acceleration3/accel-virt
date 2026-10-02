/* One guest context's OpenCL naming memory another API allocated, on this
 * host's own GPU (feature 053, T019).
 *
 * This is the part of the feature that has no second opinion. rusticl - the
 * only OpenCL on this machine, and the one the guest suite runs against - has
 * no `cl_khr_external_memory`, so a shared object cannot be handed to it as a
 * descriptor at all. It does have `cl_khr_gl_sharing`, and OpenGL takes the
 * descriptor. So the device imports through OpenGL and lets OpenCL name the
 * result, and what these tests judge is that the bytes on the other side are
 * the same bytes: a dma-buf written before the import is read back out of a
 * `cl_mem`, and what OpenCL writes is there when the descriptor's own mapping
 * is read again.
 *
 * A host with no OpenCL, no EGL display, or no GL import returns without
 * judging, as every device-backed test here does. */
#include "device/abi/av_executor.h"
#include "executor/opencl/opencl_backend.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/shim/opencl_device.hpp"
#include "executor/shim/probe_dma_buf.hpp"
#include "harness/framework/test_registry.hpp"

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

using av::OpenClBackend;

namespace {

/* One page, which is what udmabuf and every exporter here agree on. */
constexpr std::uint64_t page = 4096u;

/* The host's GPUs, with the two things a shared object needs from each: the
 * OpenCL device it is named on and the EGL display it is imported through. */
class Hosts {
public:
    Hosts() : count_(0u)
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
    /* The GPU's own UUID, which the exporting Vulkan device is chosen by: a
     * descriptor from another card is one this GPU's OpenCL would be reading
     * across a bus, and the point of the test is one allocation. */
    const std::uint8_t           *uuid(std::uint32_t index) const { return uuids_[index]; }

private:
    av::LoaderHostApis      host_;
    const av::OpenClDevice *devices_[16] = {};
    const av::GlDisplay    *displays_[16] = {};
    std::uint8_t            uuids_[16][16] = {};
    std::uint32_t           count_;
};

/* A shared object as this device really holds one: memory a Vulkan device
 * allocated and exported as a dma-buf, with known bytes in it.
 *
 * Not av::ProbeDmaBuf, which is a udmabuf - system memory wrapped as a
 * descriptor. OpenGL imports one of those happily and rusticl then faults
 * reading it, so a test built on one would be testing a shape this device
 * never produces: every shared object here comes out of vkGetMemoryFdKHR
 * (research.md §21). */
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

    /* False, holding nothing, on a host whose Vulkan cannot export a dma-buf
     * from memory a CPU can write - which is what the bytes are checked
     * through. */
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
        /* The one the device bound, by the UUID both APIs agree on - not
         * whichever is discrete. A descriptor exported from another GPU is a
         * different allocation as far as this one is concerned. */
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

    /* The words the allocation holds, written and read through Vulkan's own
     * mapping: the other side of what OpenCL sees. */
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

/* The guest's half: a context made through the backend, as the generated
 * dispatch would make one when the guest sends clCreateContext. */
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

}  // namespace

/* The whole chain, and the only claim that matters: the bytes are the same
 * bytes. A descriptor written before the import is read out of the cl_mem the
 * guest is given. */
TEST(opencl_backend_device, a_descriptor_another_api_wrote_is_read_back_through_a_cl_mem)
{ Hosts hosts;
  if (hosts.count() == 0u) { std::fprintf(stderr, "this host runs no OpenCL beside OpenGL\n"); return; }
  for (std::uint32_t index = 0u; index < hosts.count(); index++) {
      Exported written;
      if (!written.make(hosts.uuid(index))) {
          std::fprintf(stderr, "gpu %u exports no writable dma-buf\n", index);
          continue;
      }
      OpenClBackend backend;
      CHECK(backend.initialize(hosts.device(index), hosts.display(index), nullptr));
      cl_context const context = guest_context(backend, *hosts.device(index));
      if (context == nullptr) { std::fprintf(stderr, "gpu %u makes no context\n", index); backend.shut_down(); continue; }

      std::uint64_t shared = 0u;
      if (!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page,
                                 shared)) {
          std::fprintf(stderr, "gpu %u imports no shared object into OpenCL\n", index);
          backend.shut_down();
          continue;
      }
      CHECK(shared != 0u);
      /* A cl_mem the guest names in its commands like any it made. */
      CHECK(backend.handle_for(av::clproto::HandleKind::mem, shared) == reinterpret_cast<void *>(shared));

      /* What the guest would do with it: read it on a queue of its own. The
       * object is acquired for its whole life, so nothing brackets this. */
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

      /* And the other direction: what OpenCL writes is in the descriptor the
       * exporting API still holds. One allocation, not a copy of one. */
      std::uint32_t sent[8] = {0};
      for (std::uint32_t i = 0u; i < 8u; i++) sent[i] = 0x5A5A0000u + i;
      CHECK_EQ(::clEnqueueWriteBuffer(queue, reinterpret_cast<cl_mem>(shared), CL_TRUE, 0u, sizeof sent, sent,
                                      0u, nullptr, nullptr),
               CL_SUCCESS);
      CHECK_EQ(::clFinish(queue), CL_SUCCESS);
      CHECK_EQ(written.word(0u), 0x5A5A0000u);
      CHECK_EQ(written.word(1u), 0x5A5A0001u);

      /* Released, and the descriptor is still the caller's: an import took a
       * duplicate, as every other import on this device does. */
      backend.release_object(AV_SHARED_MEMORY, shared);
      CHECK(backend.handle_for(av::clproto::HandleKind::mem, shared) == nullptr);
      CHECK(::fcntl(written.handle(), F_GETFD) != -1);
      /* A handle this context no longer holds releases nothing, twice over. */
      backend.release_object(AV_SHARED_MEMORY, shared);
      static_cast<void>(::clReleaseCommandQueue(queue));
      backend.shut_down();
      /* Put back for the next GPU. */
      written.fill(0xABCD0000u);
  } }

/* A host that really can share says so where the guest's ICD reads it: the
 * ability is measured when the context binds, and the name stays in the
 * device's extensions only because a context was made against the vehicle and
 * released again. */
TEST(opencl_backend_device, a_context_that_can_share_leaves_gl_sharing_in_the_device_s_extensions)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  OpenClBackend backend;
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr));
  cl_device_id const device = static_cast<cl_device_id>(hosts.device(0u)->device());
  char   host_list[4096] = {};
  size_t host_bytes = 0u;
  CHECK_EQ(::clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof host_list, host_list, &host_bytes),
           CL_SUCCESS);
  char   answered[4096] = {};
  size_t bytes = 0u;
  CHECK_EQ(backend.on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof answered, answered, &bytes),
           CL_SUCCESS);
  /* Untouched: what the host said is what the guest is told. */
  CHECK_EQ(bytes, host_bytes);
  CHECK(std::strcmp(answered, host_list) == 0);
  /* And it is there only if the host has it at all - this says which of the
   * two the run proved. */
  if (std::strstr(host_list, "cl_khr_gl_sharing") == nullptr)
      std::fprintf(stderr, "this host's OpenCL lists no cl_khr_gl_sharing\n");
  backend.shut_down(); }

/* An import on a host that can share and a guest that has made no context:
 * there is nothing for the cl_mem to belong to, and the descriptor is left
 * untouched for the context that will come.
 *
 * A descriptor of the wrong *kind* is deliberately not tested here. An
 * ordinary file passes `glImportMemoryFdEXT` on this host and then faults
 * inside radeonsi's `si_resource_from_memobj`, so a test that fed one in would
 * be testing a shape this device never produces: every descriptor that reaches
 * an import came out of `vkGetMemoryFdKHR` on this same GPU (research.md
 * §21). */
TEST(opencl_backend_device, an_import_with_no_guest_context_is_refused_and_costs_the_descriptor_nothing)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  OpenClBackend backend;
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr));
  Exported written;
  if (!written.make(hosts.uuid(0u))) { backend.shut_down(); return; }

  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, made));
  CHECK_EQ(made, 0u);

  /* And with a context, the same descriptor imports: the refusal cost it
   * nothing. */
  if (guest_context(backend, *hosts.device(0u)) == nullptr) { backend.shut_down(); return; }
  if (backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, made)) {
      CHECK(made != 0u);
      backend.release_object(AV_SHARED_MEMORY, made);
  }
  /* One still held when the context closes is released once: the registry
   * lets go of it before it releases everything the guest made. */
  if (backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, made))
      CHECK(backend.handle_for(av::clproto::HandleKind::mem, made) != nullptr);
  backend.shut_down(); }

/* The guest's own property list is copied into the one this driver sends, and
 * a list too long for it is refused rather than truncated - a truncated one
 * would be a context made with properties the guest did not write. */
TEST(opencl_backend_device, a_property_list_longer_than_the_drivers_own_is_refused)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  OpenClBackend backend;
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr));
  cl_device_id named = static_cast<cl_device_id>(hosts.device(0u)->device());

  /* Longer than the thirty-two entries the driver copies into its own list. */
  cl_context_properties overlong[34] = {};
  for (std::uint32_t at = 0u; at + 1u < 32u; at += 2u) {
      overlong[at] = CL_CONTEXT_PLATFORM;
      overlong[at + 1u] = reinterpret_cast<cl_context_properties>(hosts.device(0u)->platform());
  }
  cl_int error = CL_SUCCESS;
  CHECK(backend.on_clCreateContext(overlong, 1u, &named, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  /* The same list, with nowhere to put the error: refused just the same. */
  CHECK(backend.on_clCreateContext(overlong, 1u, &named, nullptr) == nullptr);

  /* And a context made with no room asked for the error is still a context -
   * which is also the second one this backend sees, and the second is not the
   * one a shared object is opened into. */
  cl_context const first = guest_context(backend, *hosts.device(0u));
  if (first == nullptr) { backend.shut_down(); return; }
  cl_context_properties list[] = {CL_CONTEXT_PLATFORM,
                                  reinterpret_cast<cl_context_properties>(hosts.device(0u)->platform()), 0};
  cl_context const second = backend.on_clCreateContext(list, 1u, &named, nullptr);
  CHECK(second != nullptr);
  backend.made(av::clproto::HandleKind::context, second);
  static_cast<void>(::clReleaseContext(second));
  backend.shut_down(); }

/* What is refused before any host object is made, which is every question the
 * wire's answer depends on: a kind this API cannot import, a descriptor of the
 * wrong kind, no descriptor, and no size. */
TEST(opencl_backend_device, an_import_of_the_wrong_shape_makes_nothing)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Exported written;
  if (!written.make(hosts.uuid(0u))) return;

  OpenClBackend backend;
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr));
  cl_context const context = guest_context(backend, *hosts.device(0u));
  if (context == nullptr) { backend.shut_down(); return; }

  std::uint64_t made = 9u;
  /* A fence is a host semaphore, which this API names through an extension
   * this host has not got. */
  CHECK(!backend.import_object(AV_SHARED_MONITORED_FENCE, AV_SHARED_HANDLE_DMA_BUF, written.handle(), 0u,
                               made));
  CHECK(!backend.import_object(AV_SHARED_NATIVE_FENCE, AV_SHARED_HANDLE_DMA_BUF, written.handle(), 0u,
                               made));
  /* An opaque fd is another driver's private handle, which OpenGL cannot read
   * - and OpenGL is how this one reaches OpenCL. */
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_OPAQUE_FD, written.handle(), page, made));
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, -1, page, made));
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), 0u, made));
  CHECK(!backend.import_object(AV_SHARED_NATIVE_FENCE + 1u, AV_SHARED_HANDLE_DMA_BUF, written.handle(),
                               page, made));
  CHECK_EQ(made, 0u);
  /* This API exports nothing, whatever it can import. */
  std::uint32_t handle_kind = 9u;
  int           fd = 9;
  CHECK(!backend.export_object(AV_SHARED_MEMORY, 1u, handle_kind, fd));
  backend.shut_down(); }

/* A context torn down with objects still open gives every one of them back:
 * the cl_mem, the buffer under it and the memory object under that. Nothing
 * else on the host would ever free them. */
TEST(opencl_backend_device, a_shut_down_gives_back_what_the_guest_did_not)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  Exported written;
  if (!written.make(hosts.uuid(0u))) return;

  OpenClBackend backend;
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr));
  cl_context const context = guest_context(backend, *hosts.device(0u));
  if (context == nullptr) { backend.shut_down(); return; }
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  if (!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, first)) {
      backend.shut_down();
      return;
  }
  CHECK(backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, written.handle(), page, second));
  CHECK(first != second);
  /* Two imports of one descriptor are two host objects over one allocation,
   * which is what two guest contexts opening one shared object look like. */
  backend.shut_down();
  /* The descriptor outlived both, as it must: it is the device's, and every
   * import took a duplicate. */
  CHECK(::fcntl(written.handle(), F_GETFD) != -1); }

/* A write's bytes are the decode arena's, which the next command decodes over,
 * and rusticl reads a non-blocking write's source when its queue reaches the
 * write rather than at the call. So whatever the guest asked, the write the
 * backend makes is blocking: the source changed the moment the call returns
 * must not be what the buffer ends up holding. The queue is given work first,
 * so a write the host deferred would still be waiting when the source
 * changes. */
TEST(opencl_backend_device, a_write_has_taken_its_bytes_by_the_time_it_returns_whatever_the_guest_asked)
{ Hosts hosts;
  if (hosts.count() == 0u) return;
  OpenClBackend backend;
  CHECK(backend.initialize(hosts.device(0u), hosts.display(0u), nullptr));
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

  /* An image write the same way: a 256x256 RGBA8 image, written with the
   * guest asking not to block, and the source overwritten at once. */
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

  /* And a rectangle of a buffer. */
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
