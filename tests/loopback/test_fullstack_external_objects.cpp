/* Two devices in one process sharing memory and a semaphore through the Win32
 * external objects (feature 047, US3, research.md §10): the ICD's own entry
 * points, the real driver and device core, the executor library, and this
 * machine's GPU.
 *
 * What only this layer can prove is that the translation works end to end. A
 * guest device is a host device, so the sharing is between two host devices,
 * over the fd extensions the host carries - and the proof is that the second
 * device sees what the first wrote into the shared memory, after waiting on
 * the semaphore the first signalled. A host without Vulkan, or one whose
 * driver cannot share by fd, has nothing here to judge and the test says so
 * and returns. */
#include "device/abi/av_executor.h"
#include "harness/executor/executor_library_file.hpp"
#include "harness/executor/widest_host_gpu.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/memory_win32_handle_record.hpp"
#include "icd/vulkan/semaphore_win32_handle_record.hpp"

#include <cstdint>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libglapi\n"
           "leak:libvulkan\n"
           "leak:libnvidia\n"
           "leak:nvidia_icd\n"
           "leak:libvulkan_intel\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libz\n"
           "leak:libLLVM\n"
           "leak:libgallium\n"
           "leak:libav_executor\n";
}

using av::DdiKmt;
using av::ExecutorLibraryFile;
using av::FullStackRig;
using av::LockTrace;
using av::MockPlatform;
using av::RecordingLog;
using av::RecordingThreadLock;
using av::WidestHostGpu;

namespace {

/* The ICD as one guest process holds it. */
class IcdProcess {
public:
    explicit IcdProcess(DdiKmt &kmt)
        : lock_(trace_, 'H'), send_lock_(trace_, 'S'), memory_lock_(trace_, 'M'),
          root_(memory_, kmt, lock_, send_lock_, memory_lock_, log_, av::icd::icd_binding, windows_, chains_)
    {
    }

    av::icd::IcdRoot &root() { return root_; }
    RecordingLog     &log() { return log_; }

private:
    MockPlatform              memory_;
    LockTrace                 trace_;
    RecordingThreadLock       lock_;
    RecordingThreadLock       send_lock_;
    RecordingThreadLock       memory_lock_;
    RecordingLog              log_;
    av::ScriptedWindowSurface windows_;
    av::ScriptedWindowChains  chains_;
    av::icd::IcdRoot          root_;
};

/* Two devices on the av adapter's one physical device, through the ICD's
 * entry points as the Windows loader would call them: what a DXGI-shared
 * resource lives between. */
class IcdDevice {
public:
    explicit IcdDevice(av::icd::IcdRoot &root) : root_(root), instance_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE)
    {
        VkApplicationInfo application = {};
        application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        /* 1.3, written out: VK_API_VERSION_1_3 is an old-style cast the build
         * refuses. */
        application.apiVersion = (1u << 22) | (3u << 12);
        VkInstanceCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create.pApplicationInfo = &application;
        if (entry<PFN_vkCreateInstance>(nullptr, "vkCreateInstance")(&create, nullptr, &instance_) != VK_SUCCESS)
            return;
        std::uint32_t         count = 1u;
        VkPhysicalDevice      physical_device = VK_NULL_HANDLE;
        const auto            enumerate =
            entry<VkResult (*)(VkInstance, std::uint32_t *, VkPhysicalDevice *)>(instance_,
                                                                                 "vkEnumeratePhysicalDevices");
        if (enumerate(instance_, &count, &physical_device) != VK_SUCCESS || count == 0u) return;
        float                   priority = 1.0F;
        VkDeviceQueueCreateInfo queue = {};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueCount = 1u;
        queue.pQueuePriorities = &priority;
        VkDeviceCreateInfo device = {};
        device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device.queueCreateInfoCount = 1u;
        device.pQueueCreateInfos = &queue;
        const char *const shared[] = {"VK_KHR_external_memory_win32", "VK_KHR_external_semaphore_win32"};
        device.enabledExtensionCount = 2u;
        device.ppEnabledExtensionNames = shared;
        physical_device_ = physical_device;
        static_cast<void>(entry<PFN_vkCreateDevice>(instance_, "vkCreateDevice")(physical_device, &device, nullptr,
                                                                                 &device_));
        if (device_ == VK_NULL_HANDLE) return;
        static_cast<void>(entry<PFN_vkCreateDevice>(instance_, "vkCreateDevice")(physical_device, &device, nullptr,
                                                                                 &second_));
    }
    ~IcdDevice()
    {
        if (second_ != VK_NULL_HANDLE)
            entry<PFN_vkDestroyDevice>(instance_, "vkDestroyDevice")(second_, nullptr);
        if (device_ != VK_NULL_HANDLE)
            entry<PFN_vkDestroyDevice>(instance_, "vkDestroyDevice")(device_, nullptr);
        if (instance_ != VK_NULL_HANDLE)
            entry<PFN_vkDestroyInstance>(instance_, "vkDestroyInstance")(instance_, nullptr);
    }
    IcdDevice(const IcdDevice &) = delete;
    IcdDevice &operator=(const IcdDevice &) = delete;

    template <typename Function>
    Function entry(VkInstance instance, const char *name)
    {
        return reinterpret_cast<Function>(root_.instance_proc_addr(instance, name));
    }

    VkInstance       instance() const { return instance_; }
    VkDevice         device() const { return device_; }
    VkDevice         second() const { return second_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }

private:
    av::icd::IcdRoot &root_;
    VkInstance        instance_;
    VkPhysicalDevice  physical_device_ = VK_NULL_HANDLE;
    VkDevice          device_;
    VkDevice          second_ = VK_NULL_HANDLE;
};


/* A queue, a command pool and one command buffer of a device, with a fence to
 * wait on: what each side of the sharing submits through. */
class Submission {
public:
    Submission(IcdDevice &guest, VkDevice device) : guest_(guest), device_(device)
    {
        guest_.entry<PFN_vkGetDeviceQueue>(guest_.instance(), "vkGetDeviceQueue")(device_, 0u, 0u, &queue_);
        VkCommandPoolCreateInfo pool = {};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.queueFamilyIndex = 0u;
        if (guest_.entry<PFN_vkCreateCommandPool>(guest_.instance(), "vkCreateCommandPool")(device_, &pool, nullptr,
                                                                                            &pool_) != VK_SUCCESS)
            return;
        VkCommandBufferAllocateInfo allocate = {};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = pool_;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1u;
        static_cast<void>(guest_.entry<PFN_vkAllocateCommandBuffers>(guest_.instance(),
                                                                     "vkAllocateCommandBuffers")(device_, &allocate,
                                                                                                 &commands_));
        VkFenceCreateInfo fence = {};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        static_cast<void>(
            guest_.entry<PFN_vkCreateFence>(guest_.instance(), "vkCreateFence")(device_, &fence, nullptr, &fence_));
    }
    ~Submission()
    {
        if (fence_ != VK_NULL_HANDLE)
            guest_.entry<PFN_vkDestroyFence>(guest_.instance(), "vkDestroyFence")(device_, fence_, nullptr);
        if (pool_ != VK_NULL_HANDLE)
            guest_.entry<PFN_vkDestroyCommandPool>(guest_.instance(), "vkDestroyCommandPool")(device_, pool_,
                                                                                              nullptr);
    }
    Submission(const Submission &) = delete;
    Submission &operator=(const Submission &) = delete;

    bool ready() const { return commands_ != VK_NULL_HANDLE && fence_ != VK_NULL_HANDLE; }

    void begin()
    {
        VkCommandBufferBeginInfo begin = {};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CHECK_EQ(guest_.entry<PFN_vkBeginCommandBuffer>(guest_.instance(), "vkBeginCommandBuffer")(commands_, &begin),
                 VK_SUCCESS);
    }

    void end()
    { CHECK_EQ(guest_.entry<PFN_vkEndCommandBuffer>(guest_.instance(), "vkEndCommandBuffer")(commands_),
               VK_SUCCESS); }

    /* Submitted, signalling `signal` and waiting on `wait` where either is
     * given; whether the fence was reached. */
    bool submit(VkSemaphore signal, VkSemaphore wait)
    {
        VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo         info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.commandBufferCount = 1u;
        info.pCommandBuffers = &commands_;
        if (signal != VK_NULL_HANDLE) {
            info.signalSemaphoreCount = 1u;
            info.pSignalSemaphores = &signal;
        }
        if (wait != VK_NULL_HANDLE) {
            info.waitSemaphoreCount = 1u;
            info.pWaitSemaphores = &wait;
            info.pWaitDstStageMask = &stage;
        }
        if (guest_.entry<PFN_vkQueueSubmit>(guest_.instance(), "vkQueueSubmit")(queue_, 1u, &info, fence_) !=
            VK_SUCCESS)
            return false;
        const VkResult waited = guest_.entry<PFN_vkWaitForFences>(guest_.instance(), "vkWaitForFences")(
            device_, 1u, &fence_, VK_TRUE, 2000000000u);
        return waited == VK_SUCCESS;
    }

    VkCommandBuffer commands() const { return commands_; }

private:
    IcdDevice      &guest_;
    VkDevice        device_;
    VkQueue         queue_ = VK_NULL_HANDLE;
    VkCommandPool   pool_ = VK_NULL_HANDLE;
    VkCommandBuffer commands_ = VK_NULL_HANDLE;
    VkFence         fence_ = VK_NULL_HANDLE;
};

/* The first memory type of `allowed` with every flag of `wanted`. */
std::uint32_t type_with(const VkPhysicalDeviceMemoryProperties &properties, std::uint32_t allowed,
                        VkMemoryPropertyFlags wanted)
{ for (std::uint32_t index = 0u; index < properties.memoryTypeCount; index++) {
      if ((allowed & (1u << index)) == 0u) continue;
      if ((properties.memoryTypes[index].propertyFlags & wanted) == wanted) return index;
  }
  return properties.memoryTypeCount; }

constexpr VkDeviceSize shared_bytes = 4096u;
constexpr std::uint32_t written = 0xABCDEF01u;

}  // namespace

/* One device writes into shared memory and signals; the other waits on the
 * imported semaphore, copies the shared memory into its own staging, and the
 * guest reads back what the first wrote. Every handle in it is one this ICD
 * exported, and everything the host saw was an fd. */
TEST(fullstack_external_objects, a_second_device_reads_what_the_first_wrote_into_shared_memory)
{ const WidestHostGpu gpu;
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_VULKAN) == 0u) return;
  const ExecutorLibraryFile library(AV_EXECUTOR_LIBRARY);
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  IcdProcess process(rig.kmt());
  { IcdDevice guest(process.root());
    CHECK(guest.instance() != VK_NULL_HANDLE);
    if (guest.device() == VK_NULL_HANDLE || guest.second() == VK_NULL_HANDLE) {
        /* A host driver with no fd sharing makes no such device, and there is
         * nothing here to judge. */
        process.log().write("fullstack: the host makes no device that shares by fd");
        return;
    }
    const VkDevice first = guest.device();
    const VkDevice second = guest.second();

    VkPhysicalDeviceMemoryProperties properties = {};
    guest.entry<PFN_vkGetPhysicalDeviceMemoryProperties>(guest.instance(),
                                                         "vkGetPhysicalDeviceMemoryProperties")(
        guest.physical_device(), &properties);

    /* The shared buffer, made on both devices over one allocation. */
    VkBufferCreateInfo buffer_create = {};
    buffer_create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create.size = shared_bytes;
    buffer_create.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer shared_on_first = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkCreateBuffer>(guest.instance(), "vkCreateBuffer")(first, &buffer_create, nullptr,
                                                                                 &shared_on_first),
             VK_SUCCESS);
    VkMemoryRequirements requirements = {};
    guest.entry<PFN_vkGetBufferMemoryRequirements>(guest.instance(), "vkGetBufferMemoryRequirements")(
        first, shared_on_first, &requirements);
    const std::uint32_t device_local =
        type_with(properties, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (device_local == properties.memoryTypeCount) {
        process.log().write("fullstack: no device-local memory type the shared buffer may use");
        return;
    }

    VkExportMemoryAllocateInfo exporting = {};
    exporting.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkMemoryAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.pNext = &exporting;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = device_local;
    VkDeviceMemory shared_memory = VK_NULL_HANDLE;
    const VkResult allocated = guest.entry<PFN_vkAllocateMemory>(guest.instance(), "vkAllocateMemory")(
        first, &allocate, nullptr, &shared_memory);
    if (allocated != VK_SUCCESS) {
        /* The host's driver refused an exportable allocation, which is its
         * answer and not this ICD's. */
        process.log().write("fullstack: the host refused an exportable allocation");
        return;
    }
    CHECK_EQ(guest.entry<PFN_vkBindBufferMemory>(guest.instance(), "vkBindBufferMemory")(first, shared_on_first,
                                                                                         shared_memory, 0u),
             VK_SUCCESS);

    /* The handle this ICD answers for it, and the second device's import of
     * it: a fresh fd of the same host memory, under the hood. */
    av::icd::MemoryGetWin32HandleRecord getting = {};
    getting.type_ = av::icd::memory_get_win32_handle_info;
    getting.memory_ = shared_memory;
    getting.handle_type_ = av::icd::opaque_win32_memory;
    void *memory_handle = nullptr;
    CHECK_EQ(guest.entry<VkResult (*)(VkDevice, const av::icd::MemoryGetWin32HandleRecord *, void **)>(
                 guest.instance(), "vkGetMemoryWin32HandleKHR")(first, &getting, &memory_handle),
             VK_SUCCESS);
    CHECK(memory_handle != nullptr);

    av::icd::ImportMemoryWin32HandleRecord importing = {};
    importing.type_ = av::icd::import_memory_win32_handle_info;
    importing.handle_type_ = av::icd::opaque_win32_memory;
    importing.handle_ = memory_handle;
    VkMemoryAllocateInfo importing_allocate = {};
    importing_allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    importing_allocate.pNext = &importing;
    importing_allocate.allocationSize = requirements.size;
    importing_allocate.memoryTypeIndex = device_local;
    VkDeviceMemory imported_memory = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkAllocateMemory>(guest.instance(), "vkAllocateMemory")(second, &importing_allocate,
                                                                                     nullptr, &imported_memory),
             VK_SUCCESS);
    CHECK(imported_memory != VK_NULL_HANDLE);
    VkBuffer shared_on_second = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkCreateBuffer>(guest.instance(), "vkCreateBuffer")(second, &buffer_create, nullptr,
                                                                                 &shared_on_second),
             VK_SUCCESS);
    CHECK_EQ(guest.entry<PFN_vkBindBufferMemory>(guest.instance(), "vkBindBufferMemory")(second, shared_on_second,
                                                                                         imported_memory, 0u),
             VK_SUCCESS);

    /* The semaphore the first signals and the second waits on, through the
     * same translation. */
    VkExportSemaphoreCreateInfo exportable = {};
    exportable.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    exportable.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkSemaphoreCreateInfo semaphore_create = {};
    semaphore_create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create.pNext = &exportable;
    VkSemaphore signalled = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkCreateSemaphore>(guest.instance(), "vkCreateSemaphore")(first, &semaphore_create,
                                                                                       nullptr, &signalled),
             VK_SUCCESS);
    VkSemaphore waited = VK_NULL_HANDLE;
    VkSemaphoreCreateInfo plain = {};
    plain.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    CHECK_EQ(guest.entry<PFN_vkCreateSemaphore>(guest.instance(), "vkCreateSemaphore")(second, &plain, nullptr,
                                                                                       &waited),
             VK_SUCCESS);
    av::icd::SemaphoreGetWin32HandleRecord semaphore_getting = {};
    semaphore_getting.type_ = av::icd::semaphore_get_win32_handle_info;
    semaphore_getting.semaphore_ = signalled;
    semaphore_getting.handle_type_ = av::icd::opaque_win32_semaphore;
    void *semaphore_handle = nullptr;
    CHECK_EQ(guest.entry<VkResult (*)(VkDevice, const av::icd::SemaphoreGetWin32HandleRecord *, void **)>(
                 guest.instance(), "vkGetSemaphoreWin32HandleKHR")(first, &semaphore_getting, &semaphore_handle),
             VK_SUCCESS);
    av::icd::ImportSemaphoreWin32HandleRecord semaphore_importing = {};
    semaphore_importing.type_ = av::icd::import_semaphore_win32_handle_info;
    semaphore_importing.semaphore_ = waited;
    semaphore_importing.handle_type_ = av::icd::opaque_win32_semaphore;
    semaphore_importing.handle_ = semaphore_handle;
    const VkResult semaphore_imported =
        guest.entry<VkResult (*)(VkDevice, const av::icd::ImportSemaphoreWin32HandleRecord *)>(
            guest.instance(), "vkImportSemaphoreWin32HandleKHR")(second, &semaphore_importing);
    CHECK_EQ(semaphore_imported, VK_SUCCESS);

    /* The second device's staging buffer: host-visible, so the guest reads it
     * as its own memory with no round trip. */
    VkBufferCreateInfo staging_create = buffer_create;
    staging_create.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer staging = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkCreateBuffer>(guest.instance(), "vkCreateBuffer")(second, &staging_create, nullptr,
                                                                                 &staging),
             VK_SUCCESS);
    VkMemoryRequirements staging_requirements = {};
    guest.entry<PFN_vkGetBufferMemoryRequirements>(guest.instance(), "vkGetBufferMemoryRequirements")(
        second, staging, &staging_requirements);
    const std::uint32_t host_visible = type_with(properties, staging_requirements.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if (host_visible == properties.memoryTypeCount) {
        process.log().write("fullstack: no host-visible memory type the staging buffer may use");
        return;
    }
    VkMemoryAllocateInfo staging_allocate = {};
    staging_allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    staging_allocate.allocationSize = staging_requirements.size;
    staging_allocate.memoryTypeIndex = host_visible;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkAllocateMemory>(guest.instance(), "vkAllocateMemory")(second, &staging_allocate,
                                                                                     nullptr, &staging_memory),
             VK_SUCCESS);
    CHECK_EQ(guest.entry<PFN_vkBindBufferMemory>(guest.instance(), "vkBindBufferMemory")(second, staging,
                                                                                         staging_memory, 0u),
             VK_SUCCESS);

    /* The first device fills the shared buffer and signals; the second waits
     * on the imported semaphore and copies it into its staging. */
    Submission writing(guest, first);
    Submission reading(guest, second);
    CHECK(writing.ready());
    CHECK(reading.ready());
    writing.begin();
    guest.entry<PFN_vkCmdFillBuffer>(guest.instance(), "vkCmdFillBuffer")(writing.commands(), shared_on_first, 0u,
                                                                          shared_bytes, written);
    writing.end();
    reading.begin();
    VkBufferCopy region = {};
    region.size = shared_bytes;
    guest.entry<PFN_vkCmdCopyBuffer>(guest.instance(), "vkCmdCopyBuffer")(reading.commands(), shared_on_second,
                                                                          staging, 1u, &region);
    reading.end();
    CHECK(writing.submit(signalled, VK_NULL_HANDLE));
    CHECK(reading.submit(VK_NULL_HANDLE, waited));

    /* What the second device read is what the first wrote: one allocation,
     * two devices, a Win32 handle between them. */
    void *mapped = nullptr;
    CHECK_EQ(guest.entry<PFN_vkMapMemory>(guest.instance(), "vkMapMemory")(second, staging_memory, 0u,
                                                                           shared_bytes, 0u, &mapped),
             VK_SUCCESS);
    CHECK(mapped != nullptr);
    if (mapped != nullptr) {
        const auto *const values = static_cast<const std::uint32_t *>(mapped);
        for (std::uint32_t index = 0u; index < shared_bytes / sizeof(std::uint32_t); index++)
            CHECK_EQ(values[index], written);
    }
    guest.entry<PFN_vkUnmapMemory>(guest.instance(), "vkUnmapMemory")(second, staging_memory);

    /* Everything each device made, destroyed through the ICD; the handles go
     * with what they named. */
    guest.entry<PFN_vkDestroySemaphore>(guest.instance(), "vkDestroySemaphore")(second, waited, nullptr);
    guest.entry<PFN_vkDestroySemaphore>(guest.instance(), "vkDestroySemaphore")(first, signalled, nullptr);
    guest.entry<PFN_vkDestroyBuffer>(guest.instance(), "vkDestroyBuffer")(second, staging, nullptr);
    guest.entry<PFN_vkDestroyBuffer>(guest.instance(), "vkDestroyBuffer")(second, shared_on_second, nullptr);
    guest.entry<PFN_vkDestroyBuffer>(guest.instance(), "vkDestroyBuffer")(first, shared_on_first, nullptr);
    guest.entry<PFN_vkFreeMemory>(guest.instance(), "vkFreeMemory")(second, staging_memory, nullptr);
    guest.entry<PFN_vkFreeMemory>(guest.instance(), "vkFreeMemory")(second, imported_memory, nullptr);
    guest.entry<PFN_vkFreeMemory>(guest.instance(), "vkFreeMemory")(first, shared_memory, nullptr);
    process.log().write("fullstack: the second device read what the first wrote"); }
  /* Nothing in it was refused along the way, and it got to the end: the four
   * returns above are the host saying it cannot share, and each says so. */
  CHECK_EQ(process.log().holding("fullstack: the second device read what the first wrote"), 1u);
  CHECK_EQ(process.log().holding("avvk: vkAllocateMemory names a handle this driver did not export"), 0u);
  CHECK_EQ(process.log().holding("avvk: vkImportSemaphoreWin32HandleKHR names a handle"), 0u);
}
