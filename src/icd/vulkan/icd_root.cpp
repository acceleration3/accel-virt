#include "icd/vulkan/icd_root.hpp"

#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/context_fault.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/local_entries.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/vulkan/memory_type_table.hpp"
#include "icd/vulkan/physical_device_view.hpp"
#include "icd/common/sliced_wait.hpp"
#include "icd/common/thread_lock.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_icd_entries.h"

namespace av::icd {

namespace {

using Entry = vkicd::generated::Entry;
using Level = vkicd::generated::DispatchLevel;

/* The first rows are what the loader may ask for with no instance
 * (loader_interface.md); the rest answer only through an instance. */
constexpr uint32_t before_instance_rows = 6u;
const Entry        local_rows[] = {
    {"vkCreateInstance", reinterpret_cast<PFN_vkVoidFunction>(&local_vkCreateInstance), Level::global},
    {"vkEnumerateInstanceExtensionProperties",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkEnumerateInstanceExtensionProperties), Level::global},
    {"vkEnumerateInstanceVersion", reinterpret_cast<PFN_vkVoidFunction>(&local_vkEnumerateInstanceVersion), Level::global},
    {"vk_icdNegotiateLoaderICDInterfaceVersion",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vk_icdNegotiateLoaderICDInterfaceVersion), Level::global},
    {"vk_icdGetPhysicalDeviceProcAddr",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vk_icdGetPhysicalDeviceProcAddr), Level::global},
    {"vk_icdEnumerateAdapterPhysicalDevices",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vk_icdEnumerateAdapterPhysicalDevices), Level::global},
    {"vkGetDeviceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetDeviceProcAddr), Level::device},
    {"vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroyDevice), Level::device},
    {"vkEnumerateDeviceExtensionProperties",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkEnumerateDeviceExtensionProperties), Level::physical_device},
    {"vkCreateDevice", reinterpret_cast<PFN_vkVoidFunction>(&local_vkCreateDevice), Level::physical_device},
    {"vkGetPhysicalDeviceProperties", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceProperties), Level::physical_device},
    {"vkGetPhysicalDeviceProperties2", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceProperties2), Level::physical_device},
    {"vkGetDeviceQueue", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetDeviceQueue), Level::device},
    {"vkGetDeviceQueue2", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetDeviceQueue2), Level::device},
    {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAllocateCommandBuffers), Level::device},
    {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(&local_vkFreeCommandBuffers), Level::device},
    {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&local_vkBeginCommandBuffer), Level::device},
    {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&local_vkEndCommandBuffer), Level::device},
    {"vkResetCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&local_vkResetCommandBuffer), Level::device},
    {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(&local_vkWaitForFences), Level::device},
    {"vkWaitSemaphores", reinterpret_cast<PFN_vkVoidFunction>(&local_vkWaitSemaphores), Level::device},
    {"vkWaitSemaphoresKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkWaitSemaphores), Level::device},
    {"vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(&local_vkQueueWaitIdle), Level::device},
    {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDeviceWaitIdle), Level::device},
    {"vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroyInstance), Level::instance},
    {"vkRvkForceContextFault", reinterpret_cast<PFN_vkVoidFunction>(&local_vkRvkForceContextFault),
            Level::physical_device},
    {"vkGetPhysicalDeviceMemoryProperties",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceMemoryProperties), Level::physical_device},
    {"vkGetPhysicalDeviceMemoryProperties2",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceMemoryProperties2), Level::physical_device},
    {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAllocateMemory), Level::device},
    {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(&local_vkFreeMemory), Level::device},
    {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(&local_vkMapMemory), Level::device},
    {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(&local_vkUnmapMemory), Level::device},
    {"vkFlushMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(&local_vkFlushMappedMemoryRanges), Level::device},
    {"vkInvalidateMappedMemoryRanges",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkInvalidateMappedMemoryRanges), Level::device},
    {"vkCreateWin32SurfaceKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkCreateWin32SurfaceKHR), Level::instance},
    {"vkDestroySurfaceKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroySurfaceKHR), Level::instance},
    {"vkGetPhysicalDeviceSurfaceSupportKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfaceSupportKHR), Level::physical_device},
    {"vkGetPhysicalDeviceWin32PresentationSupportKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceWin32PresentationSupportKHR),
            Level::physical_device},
    {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfaceCapabilitiesKHR), Level::physical_device},
    {"vkGetPhysicalDeviceSurfaceCapabilities2KHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfaceCapabilities2KHR), Level::physical_device},
    {"vkGetPhysicalDeviceSurfaceFormatsKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfaceFormatsKHR), Level::physical_device},
    {"vkGetPhysicalDeviceSurfaceFormats2KHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfaceFormats2KHR), Level::physical_device},
    {"vkGetPhysicalDeviceSurfacePresentModesKHR",
            reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceSurfacePresentModesKHR), Level::physical_device},
    {"vkCreateSwapchainKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkCreateSwapchainKHR), Level::device},
    {"vkDestroySwapchainKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroySwapchainKHR), Level::device},
    {"vkGetSwapchainImagesKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetSwapchainImagesKHR), Level::device},
    {"vkAcquireNextImageKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkAcquireNextImageKHR), Level::device},
    {"vkQueuePresentKHR", reinterpret_cast<PFN_vkVoidFunction>(&local_vkQueuePresentKHR), Level::device},
};
constexpr uint32_t local_row_count = sizeof local_rows / sizeof local_rows[0];
/* VK_API_VERSION_1_3, spelled without the header macro's C casts. */
constexpr uint32_t vulkan_1_3 = (1u << 22u) | (3u << 12u);
constexpr size_t   line_bytes = 160u;
/* The device extensions this ICD reports (research.md §9), each with the test
 * that exercises it through the ICD; tools/check_extension_policy.py holds every
 * row to a test that exists and a name the generator implemented. The probe's
 * arrive with T037. */
constexpr ExtensionPolicy::Row reported_extensions[] = {
    {"VK_EXT_memory_budget", 1u, "icd_memory.the_budget_nodes_in_the_chain_are_the_adapter_s"},
    {"VK_KHR_swapchain", 70u, "icd_presentation.a_swapchain_presents_its_frame_on_the_window"},
};
constexpr uint32_t reported_extension_count = sizeof reported_extensions / sizeof reported_extensions[0];
/* The instance extensions this ICD reports. The *2 queries are its own entry
 * points, and vulkaninfo reads a physical device's LUID only when this
 * extension is listed (the live --vulkan run of 045). */
struct InstanceExtension {
    const char *name;
    uint32_t    spec_version;
};
constexpr InstanceExtension reported_instance_extensions[] = {
    {"VK_KHR_get_physical_device_properties2", 2u},
    {"VK_KHR_surface", 25u},
    {"VK_KHR_win32_surface", 6u},
};
constexpr uint32_t reported_instance_extension_count =
    sizeof reported_instance_extensions / sizeof reported_instance_extensions[0];
/* The application's structure and the nodes after it that a memory properties
 * query looks at; a longer or cyclic chain is looked at this far. */
constexpr uint32_t max_chain = 32u;
/* More than any GPU driver lists: a host claiming more is not asked for them. */
constexpr uint32_t max_host_extensions = 1024u;
/* The most command buffers one vkAllocateCommandBuffers stages the host's
 * handles for, on the stack; the generated wrappers hold the same bound. */
constexpr uint32_t max_allocated_at_once = 1024u;

}  // namespace

IcdRoot::IcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock, ThreadLock &memory_lock,
                 LogSink &log, IcdBinding &binding, WindowSurface &windows)
    : memory_(memory), kmt_(kmt), lock_(lock), send_lock_(send_lock), memory_lock_(memory_lock), log_(log),
      binding_(binding),
      api_version_(vulkan_1_3), before_instance_(local_rows, before_instance_rows),
      entry_points_(local_rows, local_row_count),
      extensions_(reported_extensions, reported_extension_count), batches_(memory), memories_(memory), handles_(memory), connections_(memory), pending_(nullptr),
      entries_(*this), presentation_(memory, kmt, windows, log, *this, entries_)
{
    AV_SCOPE("IcdRoot::IcdRoot");
    binding_.bind(*this);
}

IcdRoot::~IcdRoot()
{
    AV_SCOPE("IcdRoot::~IcdRoot");
    binding_.unbind();
}

void IcdRoot::note(const char *what, uint64_t value)
{
    AV_SCOPE("IcdRoot::note");
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append("rvkvk: ");
    text.append(what);
    text.append(" ");
    text.append_decimal(value);
    log_.write(text.text());
}

bool IcdRoot::lost(const Connection &connection, const char *entry)
{
    AV_SCOPE("IcdRoot::lost");
    if (!connection.faulted()) return false;
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append("rvkvk: ");
    text.append(entry);
    text.append(" on a lost device");
    log_.write(text.text());
    return true;
}

VkResult IcdRoot::negotiate(uint32_t *version)
{
    AV_SCOPE("IcdRoot::negotiate");
    if (version == nullptr) {
        log_.write("rvkvk: loader interface negotiation without a version");
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    if (*version < oldest_interface) {
        note("loader interface version refused:", *version);
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    if (*version > latest_interface) *version = latest_interface;
    return VK_SUCCESS;
}

PFN_vkVoidFunction IcdRoot::instance_proc_addr(VkInstance instance, const char *name)
{
    AV_SCOPE("IcdRoot::instance_proc_addr");
    if (instance == VK_NULL_HANDLE) return before_instance_.find_local(name);
    return connection_of(instance, LoaderHandle::Kind::instance) != nullptr ? entry_points_.find(name) : nullptr;
}

PFN_vkVoidFunction IcdRoot::physical_device_proc_addr(const char *name) const
{
    AV_SCOPE("IcdRoot::physical_device_proc_addr");
    return entry_points_.find_at_level(vkicd::generated::DispatchLevel::physical_device, name);
}

PFN_vkVoidFunction IcdRoot::device_proc_addr(VkDevice device, const char *name)
{
    AV_SCOPE("IcdRoot::device_proc_addr");
    if (connection_of(device, LoaderHandle::Kind::device) == nullptr) return nullptr;
    return entry_points_.find_at_level(vkicd::generated::DispatchLevel::device, name);
}

VkResult IcdRoot::enumerate_instance_extensions(const char *layer, uint32_t *count, VkExtensionProperties *properties)
{
    AV_SCOPE("IcdRoot::enumerate_instance_extensions");
    if (layer != nullptr) {
        log_.write("rvkvk: instance extensions of a layer asked for; this driver is no layer");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }
    if (properties == nullptr) {
        *count = reported_instance_extension_count;
        return VK_SUCCESS;
    }
    const uint32_t written = *count < reported_instance_extension_count ? *count : reported_instance_extension_count;
    for (uint32_t index = 0u; index < written; index++) {
        properties[index] = VkExtensionProperties{};
        __builtin_memcpy(properties[index].extensionName, reported_instance_extensions[index].name,
                         __builtin_strlen(reported_instance_extensions[index].name) + 1u);
        properties[index].specVersion = reported_instance_extensions[index].spec_version;
    }
    *count = written;
    return written < reported_instance_extension_count ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult IcdRoot::enumerate_instance_version(uint32_t *version)
{
    AV_SCOPE("IcdRoot::enumerate_instance_version");
    *version = api_version_;
    return VK_SUCCESS;
}

Status IcdRoot::open_connection(Connection *&opened)
{
    AV_SCOPE("IcdRoot::open_connection");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    const Status made = connections_.create(opened, kmt_, memory_, send_lock_);
    if (made != Status::ok) return made;
    const Status status = opened->open();
    if (status == Status::ok) return Status::ok;
    static_cast<void>(connections_.destroy(opened));
    opened = nullptr;
    return status;
}

VkResult IcdRoot::create_instance(const VkInstanceCreateInfo *create, const VkAllocationCallbacks *allocator,
                                  VkInstance *instance)
{
    AV_SCOPE("IcdRoot::create_instance");
    Connection  *opened = nullptr;
    const Status status = open_connection(opened);
    if (status != Status::ok) {
        note("vkCreateInstance: no Vulkan connection to an rvkgpu adapter, status", static_cast<uint64_t>(status));
        return status == Status::no_memory ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    pending_ = opened;
    const VkResult result = vkicd::generated::Wrap_vkCreateInstance(create, allocator, instance);
    if (result != VK_SUCCESS) {
        note("vkCreateInstance: the host refused, result", static_cast<uint32_t>(result));
        return result;
    }
    if (*instance != VK_NULL_HANDLE) return VK_SUCCESS;
    /* The host made an instance no handle could be made for: closing its
     * context is what destroys it. */
    release_connection(opened);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

VkResult IcdRoot::enumerate_adapter_physical_devices(VkInstance instance, uint64_t luid, uint32_t *count,
                                                     VkPhysicalDevice *physical_devices)
{
    AV_SCOPE("IcdRoot::enumerate_adapter_physical_devices");
    const Connection *const connection = connection_of(instance, LoaderHandle::Kind::instance);
    if (connection == nullptr) {
        log_.write("rvkvk: vk_icdEnumerateAdapterPhysicalDevices on an instance this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (lost(*connection, "vk_icdEnumerateAdapterPhysicalDevices")) return VK_ERROR_DEVICE_LOST;
    if (luid != connection->luid()) {
        *count = 0u;
        return VK_SUCCESS;
    }
    const VkResult result = vkicd::generated::Wrap_vkEnumeratePhysicalDevices(instance, count, physical_devices);
    if (physical_devices == nullptr || (result != VK_SUCCESS && result != VK_INCOMPLETE)) return result;
    for (uint32_t index = 0u; index < *count; index++) {
        if (physical_devices[index] == VK_NULL_HANDLE) return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    return result;
}

VkResult IcdRoot::enumerate_device_extensions(VkPhysicalDevice physical_device, const char *layer, uint32_t *count,
                                              VkExtensionProperties *properties)
{
    AV_SCOPE("IcdRoot::enumerate_device_extensions");
    LoaderHandle *const made = handle_of(physical_device, LoaderHandle::Kind::physical_device);
    if (made == nullptr) {
        log_.write("rvkvk: vkEnumerateDeviceExtensionProperties on a physical device this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (lost(*made->connection(), "vkEnumerateDeviceExtensionProperties")) return VK_ERROR_DEVICE_LOST;
    if (layer != nullptr) {
        log_.write("rvkvk: device extensions of a layer asked for; this driver is no layer");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }
    ClientTransport &transport = made->connection()->transport();
    uint32_t         listed = 0u;
    VkResult result = vkproto::rvk_vkEnumerateDeviceExtensionProperties(transport, physical_device, nullptr, &listed,
                                                                        nullptr);
    if (result != VK_SUCCESS) return result;
    if (listed == 0u) return extensions_.report(nullptr, 0u, count, properties);
    if (listed > max_host_extensions) {
        note("vkEnumerateDeviceExtensionProperties: the host lists more device extensions than 1024:", listed);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    auto *const host =
        static_cast<VkExtensionProperties *>(memory_.allocate(listed * sizeof(VkExtensionProperties), Pool::paged));
    if (host == nullptr) return VK_ERROR_OUT_OF_HOST_MEMORY;
    ScopeExit released([&] { memory_.release(host); });
    result = vkproto::rvk_vkEnumerateDeviceExtensionProperties(transport, physical_device, nullptr, &listed, host);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) return result;
    return extensions_.report(host, listed, count, properties);
}

VkResult IcdRoot::create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *create,
                                const VkAllocationCallbacks *allocator, VkDevice *device)
{
    AV_SCOPE("IcdRoot::create_device");
    LoaderHandle *const made = handle_of(physical_device, LoaderHandle::Kind::physical_device);
    if (made == nullptr) {
        log_.write("rvkvk: vkCreateDevice on a physical device this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (lost(*made->connection(), "vkCreateDevice")) return VK_ERROR_DEVICE_LOST;
    const uint32_t refused = extensions_.first_refused(create->ppEnabledExtensionNames, create->enabledExtensionCount);
    if (refused != create->enabledExtensionCount) {
        const char *const name = create->ppEnabledExtensionNames[refused];
        char              line[line_bytes] = {};
        TextBuffer        text(line, sizeof line);
        text.append("rvkvk: vkCreateDevice names an extension this driver does not report: ");
        text.append(name != nullptr ? name : "(null)");
        log_.write(text.text());
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    uint64_t       host = 0u;
    const VkResult result =
        vkproto::rvk_vkCreateDevice(made->connection()->transport(), physical_device, create, allocator, &host);
    if (result != VK_SUCCESS) return result;
    *device = static_cast<VkDevice>(wrap_child(LoaderHandle::Kind::device, host, *made));
    /* Without a handle the host's device cannot be named to destroy it; it
     * goes when the instance's context closes. */
    return *device != VK_NULL_HANDLE ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
}

void IcdRoot::release_device_memory(LoaderHandle &device)
{
    AV_SCOPE("IcdRoot::release_device_memory");
    memory_lock_.acquire();
    ScopeExit           release([&] { memory_lock_.release(); });
    DeviceMemory *const memory = device.device_memory();
    if (memory == nullptr) return;
    const Status released = memory->release_all();
    if (released != Status::ok) note("device memory not released whole, status", static_cast<uint64_t>(released));
}

void IcdRoot::destroy_device(VkDevice device, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_device");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return;
    /* A swapchain frees its memory through the device: before the device's own. */
    presentation_.forget_device(device);
    release_device_memory(*made);
    vkicd::generated::Wrap_vkDestroyDevice(device, allocator);
    destroy_wrapper(device, LoaderHandle::Kind::device);
}

void IcdRoot::destroy_instance(VkInstance instance, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_instance");
    LoaderHandle *const made = handle_of(instance, LoaderHandle::Kind::instance);
    if (made == nullptr) return;
    for (uint32_t physical = 0u;; physical++) {
        LoaderHandle *const physical_device = child_of(*made, LoaderHandle::Kind::physical_device, physical);
        if (physical_device == nullptr) break;
        for (uint32_t index = 0u;; index++) {
            LoaderHandle *const device = child_of(*physical_device, LoaderHandle::Kind::device, index);
            if (device == nullptr) break;
            presentation_.forget_device(reinterpret_cast<VkDevice>(device));
            release_device_memory(*device);
        }
    }
    presentation_.forget_instance(instance);
    vkicd::generated::Wrap_vkDestroyInstance(instance, allocator);
}

void IcdRoot::learn_types(Connection &connection, VkPhysicalDevice physical_device, MemoryTypeTable &types,
                          const char *entry)
{
    AV_SCOPE("IcdRoot::learn_types");
    if (lost(connection, entry)) return;
    VkPhysicalDeviceMemoryProperties host = {};
    vkproto::rvk_vkGetPhysicalDeviceMemoryProperties(connection.transport(), physical_device, &host);
    if (lost(connection, entry)) return;
    const Status built = types.build(host);
    if (built != Status::ok) note("the host's memory types were not taken, status", static_cast<uint64_t>(built));
}

void IcdRoot::physical_device_memory_properties(VkPhysicalDevice physical_device,
                                                VkPhysicalDeviceMemoryProperties *properties)
{
    AV_SCOPE("IcdRoot::physical_device_memory_properties");
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    MemoryTypeTable types(kmt_, connection->adapter());
    learn_types(*connection, physical_device, types, "vkGetPhysicalDeviceMemoryProperties");
    types.properties(*properties);
}

void IcdRoot::physical_device_memory_properties2(VkPhysicalDevice physical_device,
                                                 VkPhysicalDeviceMemoryProperties2 *properties)
{
    AV_SCOPE("IcdRoot::physical_device_memory_properties2");
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    MemoryTypeTable types(kmt_, connection->adapter());
    learn_types(*connection, physical_device, types, "vkGetPhysicalDeviceMemoryProperties2");
    types.properties(properties->memoryProperties);
    auto *node = static_cast<VkBaseOutStructure *>(properties->pNext);
    for (uint32_t seen = 1u; node != nullptr && seen < max_chain; seen++, node = node->pNext)
        if (node->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT)
            types.budget(*reinterpret_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT *>(node));
}

LoaderHandle *IcdRoot::allocating_device(VkDevice device)
{
    AV_SCOPE("IcdRoot::allocating_device");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made != nullptr && made->parent()->kind() == LoaderHandle::Kind::physical_device) return made;
    log_.write("rvkvk: memory of a device this driver did not make through a physical device");
    return nullptr;
}

DeviceMemory *IcdRoot::memory_of(LoaderHandle &device)
{
    AV_SCOPE("IcdRoot::memory_of");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    if (device.device_memory() != nullptr) return device.device_memory();
    DeviceMemory *made = nullptr;
    if (memories_.create(made, memory_, kmt_, *device.connection(), log_) != Status::ok) {
        log_.write("rvkvk: no memory to keep a device's allocations in");
        return nullptr;
    }
    device.allocate_from(memories_, *made);
    return made;
}

VkResult IcdRoot::allocate_memory(VkDevice device, const VkMemoryAllocateInfo *info,
                                  const VkAllocationCallbacks *allocator, VkDeviceMemory *memory)
{
    AV_SCOPE("IcdRoot::allocate_memory");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return VK_ERROR_DEVICE_LOST;
    if (lost(*made->connection(), "vkAllocateMemory")) return VK_ERROR_DEVICE_LOST;
    memory_lock_.acquire();
    ScopeExit           release([&] { memory_lock_.release(); });
    DeviceMemory *const owned = memory_of(*made);
    if (owned == nullptr) return VK_ERROR_OUT_OF_HOST_MEMORY;
    const auto physical = static_cast<VkPhysicalDevice>(static_cast<void *>(made->parent()));
    return owned->allocate(physical, device, *info, allocator, *memory);
}

void IcdRoot::free_memory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::free_memory");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return;
    memory_lock_.acquire();
    ScopeExit           release([&] { memory_lock_.release(); });
    DeviceMemory *const owned = memory_of(*made);
    if (owned != nullptr) owned->free(device, memory, allocator);
}

VkResult IcdRoot::map_memory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
                             VkMemoryMapFlags flags, void **data)
{
    AV_SCOPE("IcdRoot::map_memory");
    LoaderHandle *const made = allocating_device(device);
    if (made == nullptr) return VK_ERROR_MEMORY_MAP_FAILED;
    if (lost(*made->connection(), "vkMapMemory")) return VK_ERROR_DEVICE_LOST;
    memory_lock_.acquire();
    ScopeExit                 release([&] { memory_lock_.release(); });
    const DeviceMemory *const owned = made->device_memory();
    if (owned == nullptr) return VK_ERROR_MEMORY_MAP_FAILED;
    return owned->map(memory, offset, size, flags, *data);
}

VkResult IcdRoot::mapped_ranges(VkDevice device, const char *entry)
{
    AV_SCOPE("IcdRoot::mapped_ranges");
    const Connection *const connection = connection_of(device, LoaderHandle::Kind::device);
    if (connection == nullptr) {
        log_.write("rvkvk: mapped ranges of a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    return lost(*connection, entry) ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}

void IcdRoot::physical_device_properties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties *properties)
{
    AV_SCOPE("IcdRoot::physical_device_properties");
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    PhysicalDeviceView view(connection->transport(), connection->luid(), api_version_, log_);
    view.properties(physical_device, properties);
}

void IcdRoot::physical_device_properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *properties)
{
    AV_SCOPE("IcdRoot::physical_device_properties2");
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    PhysicalDeviceView view(connection->transport(), connection->luid(), api_version_, log_);
    view.properties2(physical_device, properties);
}

Connection *IcdRoot::acquire_connection()
{
    AV_SCOPE("IcdRoot::acquire_connection");
    Connection *const adopted = pending_;
    pending_ = nullptr;
    return adopted;
}

void IcdRoot::release_connection(Connection *connection)
{
    AV_SCOPE("IcdRoot::release_connection");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    if (connections_.destroy(connection) != Status::ok) log_.write("rvkvk: release of a connection never opened");
}

void *IcdRoot::wrap(LoaderHandle::Kind kind, uint64_t host, Connection *connection)
{
    AV_SCOPE("IcdRoot::wrap");
    lock_.acquire();
    ScopeExit           release([&] { lock_.release(); });
    LoaderHandle *const parent = kind == LoaderHandle::Kind::instance ? nullptr : handles_.instance_of(connection);
    return wrap_under(kind, host, connection, parent);
}

/* Under the root's lock. A command buffer is not a handle without its batch:
 * every vkCmd* on it records there. */
LoaderHandle *IcdRoot::wrap_under(LoaderHandle::Kind kind, uint64_t host, Connection *connection,
                                  LoaderHandle *parent)
{
    AV_SCOPE("IcdRoot::wrap_under");
    LoaderHandle *handle = nullptr;
    const Status  status = handles_.wrap(kind, host, connection, parent, handle);
    if (status != Status::ok) {
        note("no handle for host object, status", static_cast<uint64_t>(status));
        return nullptr;
    }
    if (kind != LoaderHandle::Kind::command_buffer || handle->batch() != nullptr) return handle;
    CommandBatch *batch = nullptr;
    if (batches_.create(batch, memory_, *connection) != Status::ok) {
        note("no batch for host command buffer", host);
        /* The handle was made just now, so the registry cannot refuse it. */
        static_cast<void>(handles_.destroy(handle));
        return nullptr;
    }
    handle->record_into(batches_, *batch);
    return handle;
}

/* The registry refuses a parent destroyed since it was looked up. */
void *IcdRoot::wrap_child(LoaderHandle::Kind kind, uint64_t host, LoaderHandle &parent)
{
    AV_SCOPE("IcdRoot::wrap_child");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    return wrap_under(kind, host, parent.connection(), &parent);
}

LoaderHandle *IcdRoot::handle_of(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::handle_of");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    return handles_.made(handle, kind);
}

LoaderHandle *IcdRoot::child_of(const LoaderHandle &parent, LoaderHandle::Kind kind, uint32_t index)
{
    AV_SCOPE("IcdRoot::child_of");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    return handles_.child(parent, kind, index);
}

Connection *IcdRoot::connection_of(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::connection_of");
    lock_.acquire();
    ScopeExit                 release([&] { lock_.release(); });
    const LoaderHandle *const made = handles_.made(handle, kind);
    return made != nullptr ? made->connection() : nullptr;
}

uint64_t IcdRoot::wire(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::wire");
    lock_.acquire();
    ScopeExit                 release([&] { lock_.release(); });
    const LoaderHandle *const made = handles_.made(handle, kind);
    return made != nullptr ? made->host() : 0u;
}

void IcdRoot::destroy_wrapper(void *handle, LoaderHandle::Kind kind)
{
    AV_SCOPE("IcdRoot::destroy_wrapper");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    if (handles_.destroy(handles_.made(handle, kind)) != Status::ok)
        log_.write("rvkvk: destroy of a handle this driver did not make");
}

CommandBatch &IcdRoot::record_target(VkCommandBuffer command_buffer)
{
    AV_SCOPE("IcdRoot::record_target");
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    return *handles_.made(command_buffer, LoaderHandle::Kind::command_buffer)->batch();
}

void IcdRoot::device_queue(VkDevice device, uint32_t family, uint32_t index, VkQueue *queue)
{
    AV_SCOPE("IcdRoot::device_queue");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return;
    uint64_t host = 0u;
    vkproto::rvk_vkGetDeviceQueue(made->connection()->transport(), device, family, index, &host);
    *queue = static_cast<VkQueue>(wrap_child(LoaderHandle::Kind::queue, host, *made));
}

void IcdRoot::device_queue2(VkDevice device, const VkDeviceQueueInfo2 *info, VkQueue *queue)
{
    AV_SCOPE("IcdRoot::device_queue2");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return;
    uint64_t host = 0u;
    vkproto::rvk_vkGetDeviceQueue2(made->connection()->transport(), device, info, &host);
    *queue = static_cast<VkQueue>(wrap_child(LoaderHandle::Kind::queue, host, *made));
}

VkResult IcdRoot::allocate_command_buffers(VkDevice device, const VkCommandBufferAllocateInfo *info,
                                           VkCommandBuffer *command_buffers)
{
    AV_SCOPE("IcdRoot::allocate_command_buffers");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        log_.write("rvkvk: vkAllocateCommandBuffers on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (lost(*made->connection(), "vkAllocateCommandBuffers")) return VK_ERROR_DEVICE_LOST;
    const uint32_t count = info->commandBufferCount;
    if (count > max_allocated_at_once) {
        note("vkAllocateCommandBuffers: more command buffers at once than", max_allocated_at_once);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    uint64_t       hosts[max_allocated_at_once] = {};
    const VkResult result = vkproto::rvk_vkAllocateCommandBuffers(made->connection()->transport(), device, info, hosts);
    if (result != VK_SUCCESS) return result;
    for (uint32_t wrapped = 0u; wrapped < count; wrapped++) {
        command_buffers[wrapped] =
            static_cast<VkCommandBuffer>(wrap_child(LoaderHandle::Kind::command_buffer, hosts[wrapped], *made));
        if (command_buffers[wrapped] != VK_NULL_HANDLE) continue;
        /* The ones already handed out go back; the host's others stay in their
         * pool until it is reset or destroyed, with no handle to name them by. */
        free_command_buffers(device, info->commandPool, wrapped, command_buffers);
        note("vkAllocateCommandBuffers: host command buffers left in their pool:", count - wrapped);
        for (uint32_t cleared = 0u; cleared < count; cleared++) command_buffers[cleared] = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    return VK_SUCCESS;
}

void IcdRoot::free_command_buffers(VkDevice device, VkCommandPool pool, uint32_t count,
                                   const VkCommandBuffer *command_buffers)
{
    AV_SCOPE("IcdRoot::free_command_buffers");
    Connection *const connection = connection_of(device, LoaderHandle::Kind::device);
    if (connection == nullptr) return;
    vkproto::rvk_vkFreeCommandBuffers(connection->transport(), device, pool, count, command_buffers);
    for (uint32_t index = 0u; index < count; index++)
        if (command_buffers[index] != VK_NULL_HANDLE)
            destroy_wrapper(command_buffers[index], LoaderHandle::Kind::command_buffer);
}

VkResult IcdRoot::begin_command_buffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *info)
{
    AV_SCOPE("IcdRoot::begin_command_buffer");
    if (lost(*connection_of(command_buffer, LoaderHandle::Kind::command_buffer), "vkBeginCommandBuffer"))
        return VK_ERROR_DEVICE_LOST;
    CommandBatch &batch = record_target(command_buffer);
    batch.discard();
    if (vkproto::rvk_vkBeginCommandBuffer(batch, command_buffer, info) == VK_SUCCESS) return VK_SUCCESS;
    log_.write("rvkvk: vkBeginCommandBuffer: no memory to record into");
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

/* A command that could not be recorded, End's own included, is latched in the
 * batch, so what end() answers is the whole recording's answer. */
VkResult IcdRoot::end_command_buffer(VkCommandBuffer command_buffer)
{
    AV_SCOPE("IcdRoot::end_command_buffer");
    CommandBatch &batch = record_target(command_buffer);
    if (lost(*connection_of(command_buffer, LoaderHandle::Kind::command_buffer), "vkEndCommandBuffer")) {
        batch.discard();
        return VK_ERROR_DEVICE_LOST;
    }
    const VkResult recorded = vkproto::rvk_vkEndCommandBuffer(batch, command_buffer);
    const Status   shipped = batch.end();
    if (shipped == Status::ok) return recorded;
    note("vkEndCommandBuffer: batch not shipped, status", static_cast<uint64_t>(shipped));
    return shipped == Status::no_memory ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_DEVICE_LOST;
}

VkResult IcdRoot::reset_command_buffer(VkCommandBuffer command_buffer, VkCommandBufferResetFlags flags)
{
    AV_SCOPE("IcdRoot::reset_command_buffer");
    record_target(command_buffer).discard();
    return vkicd::generated::Wrap_vkResetCommandBuffer(command_buffer, flags);
}

VkResult IcdRoot::wait_for_fences(VkDevice device, uint32_t count, const VkFence *fences, VkBool32 wait_all,
                                  uint64_t timeout)
{
    AV_SCOPE("IcdRoot::wait_for_fences");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        log_.write("rvkvk: vkWaitForFences on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (lost(*made->connection(), "vkWaitForFences")) return VK_ERROR_DEVICE_LOST;
    uint64_t       frequency = 0u;
    const uint64_t start = kmt_.performance_counter(frequency);
    SlicedWait     wait(timeout, start, frequency);
    uint64_t       slice = 0u;
    while (wait.next(kmt_.performance_counter(frequency), slice)) {
        const VkResult result = vkproto::rvk_vkWaitForFences(made->connection()->transport(), device, count, fences,
                                                             wait_all, slice);
        if (result != VK_TIMEOUT) return result;
    }
    return VK_TIMEOUT;
}

VkResult IcdRoot::wait_semaphores(VkDevice device, const VkSemaphoreWaitInfo *info, uint64_t timeout)
{
    AV_SCOPE("IcdRoot::wait_semaphores");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        log_.write("rvkvk: vkWaitSemaphores on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (lost(*made->connection(), "vkWaitSemaphores")) return VK_ERROR_DEVICE_LOST;
    uint64_t       frequency = 0u;
    const uint64_t start = kmt_.performance_counter(frequency);
    SlicedWait     wait(timeout, start, frequency);
    uint64_t       slice = 0u;
    while (wait.next(kmt_.performance_counter(frequency), slice)) {
        const VkResult result = vkproto::rvk_vkWaitSemaphores(made->connection()->transport(), device, info, slice);
        if (result != VK_TIMEOUT) return result;
    }
    return VK_TIMEOUT;
}

/* The host's own vkQueueWaitIdle has no timeout, so it could hold the context's
 * worker past TdrDelay; a fence on an empty submission can be waited for in
 * slices instead. */
VkResult IcdRoot::queue_wait_idle(VkQueue queue)
{
    AV_SCOPE("IcdRoot::queue_wait_idle");
    LoaderHandle *const made = handle_of(queue, LoaderHandle::Kind::queue);
    if (made == nullptr || made->parent()->kind() != LoaderHandle::Kind::device) {
        log_.write("rvkvk: vkQueueWaitIdle on a queue this driver did not make through a device");
        return VK_ERROR_DEVICE_LOST;
    }
    if (lost(*made->connection(), "vkQueueWaitIdle")) return VK_ERROR_DEVICE_LOST;
    const auto        device = static_cast<VkDevice>(static_cast<void *>(made->parent()));
    ClientTransport  &transport = made->connection()->transport();
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence        fence = VK_NULL_HANDLE;
    const VkResult created = vkproto::rvk_vkCreateFence(transport, device, &info, nullptr, &fence);
    if (created != VK_SUCCESS) return created;
    ScopeExit      destroyed([&] { vkproto::rvk_vkDestroyFence(transport, device, fence, nullptr); });
    const VkResult submitted = vkproto::rvk_vkQueueSubmit(transport, queue, 0u, nullptr, fence);
    if (submitted != VK_SUCCESS) return submitted;
    return wait_for_fences(device, 1u, &fence, VK_TRUE, SlicedWait::forever);
}

/* An application may submit only through a queue it was handed, so the queues
 * this ICD handed out are every queue with work on it. */
VkResult IcdRoot::device_wait_idle(VkDevice device)
{
    AV_SCOPE("IcdRoot::device_wait_idle");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) {
        log_.write("rvkvk: vkDeviceWaitIdle on a device this driver did not make");
        return VK_ERROR_DEVICE_LOST;
    }
    if (lost(*made->connection(), "vkDeviceWaitIdle")) return VK_ERROR_DEVICE_LOST;
    for (uint32_t index = 0u;; index++) {
        LoaderHandle *const queue = child_of(*made, LoaderHandle::Kind::queue, index);
        if (queue == nullptr) return VK_SUCCESS;
        const VkResult idle = queue_wait_idle(static_cast<VkQueue>(static_cast<void *>(queue)));
        if (idle != VK_SUCCESS) return idle;
    }
}

VkResult IcdRoot::force_context_fault(VkPhysicalDevice physical_device)
{
    AV_SCOPE("IcdRoot::force_context_fault");
    LoaderHandle *const made = handle_of(physical_device, LoaderHandle::Kind::physical_device);
    if (made == nullptr) {
        log_.write("rvkvk: vkRvkForceContextFault on a physical device this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    Connection &connection = *made->connection();
    if (lost(connection, "vkRvkForceContextFault")) return VK_ERROR_DEVICE_LOST;
    if (ContextFault(connection.transport()).send() != Status::ok) {
        log_.write("rvkvk: the host answered a truncated vkCreateInstance");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_ERROR_DEVICE_LOST;
}

uint32_t IcdRoot::handles_on_connections() const
{
    AV_SCOPE("IcdRoot::handles_on_connections");
    uint32_t counted = 0u;
    static_cast<void>(connections_.all_of([&](const Connection &connection) {
        counted += handles_.live_on(&connection);
        return true;
    }));
    return counted;
}

void IcdRoot::report(StateWriter &writer)
{
    AV_SCOPE("IcdRoot::report");
    writer.open("root", 0u);
    if (!lock_.try_acquire()) {
        writer.flag("busy", true);
        writer.close();
        return;
    }
    ScopeExit release([&] { lock_.release(); });
    writer.flag("busy", false);
    writer.number("connections", connections_.live());
    writer.number("handles", handles_.live());
    writer.number("batches", handles_.batches());
    writer.number("memories", handles_.memories());
    writer.close();
    uint32_t number = 0u;
    static_cast<void>(connections_.all_of([&](const Connection &connection) {
        writer.open("connection", number++);
        connection.report(writer);
        handles_.report(writer, &connection);
        writer.close();
        return true;
    }));
}

bool IcdRoot::invariants_hold() const
{
    AV_SCOPE("IcdRoot::invariants_hold");
    /* The entry point tables are constant: a row named twice is a build
     * mistake the root's tests see as a wrong answer, not a state it reaches. */
    if (pending_ != nullptr || !handles_.invariants_hold() || handles_on_connections() != handles_.live()) return false;
    if (!batches_.invariants_hold() || batches_.live() != handles_.batches()) return false;
    if (!batches_.all_of([](const CommandBatch &batch) { return batch.invariants_hold(); })) return false;
    if (!memories_.invariants_hold() || memories_.live() != handles_.memories()) return false;
    if (!memories_.all_of([](const DeviceMemory &memory) { return memory.invariants_hold(); })) return false;
    return connections_.all_of([](const Connection &connection) {
        return connection.opened() && connection.invariants_hold();
    });
}

PFN_vkVoidFunction IcdRoot::entry(const char *name) const
{
    AV_SCOPE("IcdRoot::entry");
    return entry_points_.find(name);
}

Presentation &IcdRoot::presentation()
{
    AV_SCOPE("IcdRoot::presentation");
    return presentation_;
}

bool IcdRoot::present_context(VkDevice device, VkPhysicalDevice &physical, uint32_t &adapter, uint32_t &kmt_device)
{
    AV_SCOPE("IcdRoot::present_context");
    LoaderHandle *const made = handle_of(device, LoaderHandle::Kind::device);
    if (made == nullptr) return false;
    LoaderHandle *const parent = made->parent();
    physical = static_cast<VkPhysicalDevice>(static_cast<void *>(parent));
    adapter = made->connection()->adapter();
    kmt_device = made->connection()->device();
    return true;
}

}  // namespace av::icd
