/* A descriptor update template through the whole stack (feature 047,
 * contracts/descriptor_templates.md): the ICD's own entry points, the real
 * driver and device core, the executor library, and this machine's GPU.
 *
 * What only this layer can prove is that the two halves of the blob's length
 * agree. The guest sizes it from the template it remembers; the device refuses
 * a length its own template does not account for. A host without Vulkan has
 * nothing here to judge, and the test returns without checking. */
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

/* One device on the av adapter, through the ICD's entry points as the Windows
 * loader would call them. */
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
        static_cast<void>(entry<PFN_vkCreateDevice>(instance_, "vkCreateDevice")(physical_device, &device, nullptr,
                                                                                 &device_));
    }
    ~IcdDevice()
    {
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

    VkInstance instance() const { return instance_; }
    VkDevice   device() const { return device_; }

private:
    av::icd::IcdRoot &root_;
    VkInstance        instance_;
    VkDevice          device_;
};

}  // namespace

/* A template made, a descriptor set updated through it, and the device still
 * answering afterwards - which is what says the frame was not refused. */
TEST(fullstack_templates, a_descriptor_set_updated_through_a_template_reaches_the_host)
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
    CHECK(guest.device() != VK_NULL_HANDLE);
    if (guest.device() == VK_NULL_HANDLE) return;
    const VkDevice device = guest.device();

    /* One storage buffer binding, so a template of one buffer descriptor has
     * something to be made for. */
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0u;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1u;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo layout_create = {};
    layout_create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_create.bindingCount = 1u;
    layout_create.pBindings = &binding;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkCreateDescriptorSetLayout>(guest.instance(), "vkCreateDescriptorSetLayout")(
                 device, &layout_create, nullptr, &layout),
             VK_SUCCESS);

    VkDescriptorPoolSize size = {};
    size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    size.descriptorCount = 1u;
    VkDescriptorPoolCreateInfo pool_create = {};
    pool_create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create.maxSets = 1u;
    pool_create.poolSizeCount = 1u;
    pool_create.pPoolSizes = &size;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkCreateDescriptorPool>(guest.instance(), "vkCreateDescriptorPool")(
                 device, &pool_create, nullptr, &pool),
             VK_SUCCESS);
    VkDescriptorSetAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool = pool;
    allocate.descriptorSetCount = 1u;
    allocate.pSetLayouts = &layout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkAllocateDescriptorSets>(guest.instance(), "vkAllocateDescriptorSets")(device,
                                                                                                     &allocate, &set),
             VK_SUCCESS);

    /* The template: one buffer descriptor at offset 0. */
    VkDescriptorUpdateTemplateEntry entry = {};
    entry.dstBinding = 0u;
    entry.descriptorCount = 1u;
    entry.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    entry.offset = 0u;
    entry.stride = sizeof(VkDescriptorBufferInfo);
    VkDescriptorUpdateTemplateCreateInfo template_create = {};
    template_create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
    template_create.descriptorUpdateEntryCount = 1u;
    template_create.pDescriptorUpdateEntries = &entry;
    template_create.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
    template_create.descriptorSetLayout = layout;
    VkDescriptorUpdateTemplate made = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkCreateDescriptorUpdateTemplate>(guest.instance(),
                                                               "vkCreateDescriptorUpdateTemplate")(
                 device, &template_create, nullptr, &made),
             VK_SUCCESS);
    CHECK(made != VK_NULL_HANDLE);
    /* Both sides sized it the same way, which is the point of the whole
     * mechanism: the guest's number is what the device expects. */
    CHECK_EQ(process.root().templates().bytes_of(made), sizeof(VkDescriptorBufferInfo));

    /* A buffer to point the descriptor at, and the update itself. */
    VkBufferCreateInfo buffer_create = {};
    buffer_create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create.size = 256u;
    buffer_create.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    CHECK_EQ(guest.entry<PFN_vkCreateBuffer>(guest.instance(), "vkCreateBuffer")(device, &buffer_create, nullptr,
                                                                                 &buffer),
             VK_SUCCESS);
    VkMemoryRequirements requirements = {};
    guest.entry<PFN_vkGetBufferMemoryRequirements>(guest.instance(), "vkGetBufferMemoryRequirements")(device, buffer,
                                                                                                      &requirements);
    VkDescriptorBufferInfo described = {};
    described.buffer = buffer;
    described.offset = 0u;
    described.range = VK_WHOLE_SIZE;
    guest.entry<PFN_vkUpdateDescriptorSetWithTemplate>(guest.instance(), "vkUpdateDescriptorSetWithTemplate")(
        device, set, made, &described);

    /* The device answering afterwards is what says the frame was carried: a
     * length the host's own template did not account for would have refused the
     * frame and faulted the context. */
    CHECK_EQ(guest.entry<PFN_vkDeviceWaitIdle>(guest.instance(), "vkDeviceWaitIdle")(device), VK_SUCCESS);
    CHECK_EQ(process.log().holding("fault"), 0u);

    guest.entry<PFN_vkDestroyBuffer>(guest.instance(), "vkDestroyBuffer")(device, buffer, nullptr);
    guest.entry<PFN_vkDestroyDescriptorUpdateTemplate>(guest.instance(), "vkDestroyDescriptorUpdateTemplate")(
        device, made, nullptr);
    CHECK_EQ(process.root().templates().bytes_of(made), 0u);
    guest.entry<PFN_vkDestroyDescriptorPool>(guest.instance(), "vkDestroyDescriptorPool")(device, pool, nullptr);
    guest.entry<PFN_vkDestroyDescriptorSetLayout>(guest.instance(), "vkDestroyDescriptorSetLayout")(device, layout,
                                                                                                    nullptr); }
  CHECK(process.root().invariants_hold()); }
