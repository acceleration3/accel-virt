#include "icd/vulkan/icd_root.hpp"

#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/local_entries.hpp"
#include "icd/vulkan/log_sink.hpp"
#include "icd/vulkan/physical_device_view.hpp"
#include "icd/vulkan/thread_lock.hpp"
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
    {"vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(&local_vkDestroyDevice), Level::device},
    {"vkGetPhysicalDeviceProperties", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceProperties), Level::physical_device},
    {"vkGetPhysicalDeviceProperties2", reinterpret_cast<PFN_vkVoidFunction>(&local_vkGetPhysicalDeviceProperties2), Level::physical_device},
};
constexpr uint32_t local_row_count = sizeof local_rows / sizeof local_rows[0];
/* VK_API_VERSION_1_3, spelled without the header macro's C casts. */
constexpr uint32_t vulkan_1_3 = (1u << 22u) | (3u << 12u);
constexpr size_t   line_bytes = 160u;

}  // namespace

IcdRoot::IcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock, LogSink &log,
                 IcdBinding &binding)
    : memory_(memory), kmt_(kmt), lock_(lock), send_lock_(send_lock), log_(log), binding_(binding),
      api_version_(vulkan_1_3), before_instance_(local_rows, before_instance_rows),
      entry_points_(local_rows, local_row_count), handles_(memory), connections_(memory), pending_(nullptr)
{
    binding_.bind(*this);
}

IcdRoot::~IcdRoot() { binding_.unbind(); }

void IcdRoot::note(const char *what, uint64_t value)
{
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append("rvkvk: ");
    text.append(what);
    text.append(" ");
    text.append_decimal(value);
    log_.write(text.text());
}

VkResult IcdRoot::negotiate(uint32_t *version)
{
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
    if (instance == VK_NULL_HANDLE) return before_instance_.find_local(name);
    return connection_of(instance, LoaderHandle::Kind::instance) != nullptr ? entry_points_.find(name) : nullptr;
}

PFN_vkVoidFunction IcdRoot::physical_device_proc_addr(const char *name) const
{
    return entry_points_.find_physical_device_level(name);
}

VkResult IcdRoot::enumerate_instance_extensions(const char *layer, uint32_t *count, VkExtensionProperties *)
{
    if (layer != nullptr) {
        log_.write("rvkvk: instance extensions of a layer asked for; this driver is no layer");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }
    *count = 0u;
    return VK_SUCCESS;
}

VkResult IcdRoot::enumerate_instance_version(uint32_t *version)
{
    *version = api_version_;
    return VK_SUCCESS;
}

Status IcdRoot::open_connection(Connection *&opened)
{
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
    const Connection *const connection = connection_of(instance, LoaderHandle::Kind::instance);
    if (connection == nullptr) {
        log_.write("rvkvk: vk_icdEnumerateAdapterPhysicalDevices on an instance this driver did not make");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
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

void IcdRoot::destroy_device(VkDevice device, const VkAllocationCallbacks *allocator)
{
    if (connection_of(device, LoaderHandle::Kind::device) == nullptr) return;
    vkicd::generated::Wrap_vkDestroyDevice(device, allocator);
    destroy_wrapper(device, LoaderHandle::Kind::device);
}

void IcdRoot::physical_device_properties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties *properties)
{
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    PhysicalDeviceView view(connection->transport(), connection->luid(), api_version_, log_);
    view.properties(physical_device, properties);
}

void IcdRoot::physical_device_properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *properties)
{
    Connection *const connection = connection_of(physical_device, LoaderHandle::Kind::physical_device);
    if (connection == nullptr) return;
    PhysicalDeviceView view(connection->transport(), connection->luid(), api_version_, log_);
    view.properties2(physical_device, properties);
}

Connection *IcdRoot::acquire_connection()
{
    Connection *const adopted = pending_;
    pending_ = nullptr;
    return adopted;
}

void IcdRoot::release_connection(Connection *connection)
{
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    if (connections_.destroy(connection) != Status::ok) log_.write("rvkvk: release of a connection never opened");
}

void *IcdRoot::wrap(LoaderHandle::Kind kind, uint64_t host, Connection *connection)
{
    lock_.acquire();
    ScopeExit     release([&] { lock_.release(); });
    LoaderHandle *parent = kind == LoaderHandle::Kind::instance ? nullptr : handles_.instance_of(connection);
    LoaderHandle *handle = nullptr;
    const Status  status = handles_.wrap(kind, host, connection, parent, handle);
    if (status != Status::ok) note("no handle for host object, status", static_cast<uint64_t>(status));
    return handle;
}

Connection *IcdRoot::connection_of(void *handle, LoaderHandle::Kind kind)
{
    lock_.acquire();
    ScopeExit                 release([&] { lock_.release(); });
    const LoaderHandle *const made = handles_.made(handle, kind);
    return made != nullptr ? made->connection() : nullptr;
}

uint64_t IcdRoot::wire(void *handle, LoaderHandle::Kind kind)
{
    lock_.acquire();
    ScopeExit                 release([&] { lock_.release(); });
    const LoaderHandle *const made = handles_.made(handle, kind);
    return made != nullptr ? made->host() : 0u;
}

void IcdRoot::destroy_wrapper(void *handle, LoaderHandle::Kind kind)
{
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    if (handles_.destroy(handles_.made(handle, kind)) != Status::ok)
        log_.write("rvkvk: destroy of a handle this driver did not make");
}

/* The loader reached this through the command buffer's own dispatch table, so
 * it is a handle this ICD made. */
ClientTransport &IcdRoot::record_target(VkCommandBuffer command_buffer)
{
    return connection_of(command_buffer, LoaderHandle::Kind::command_buffer)->transport();
}

uint32_t IcdRoot::handles_on_connections() const
{
    uint32_t counted = 0u;
    static_cast<void>(connections_.all_of([&](const Connection &connection) {
        counted += handles_.live_on(&connection);
        return true;
    }));
    return counted;
}

bool IcdRoot::invariants_hold() const
{
    /* The entry point tables are constant: a row named twice is a build
     * mistake the root's tests see as a wrong answer, not a state it reaches. */
    if (pending_ != nullptr || !handles_.invariants_hold() || handles_on_connections() != handles_.live()) return false;
    return connections_.all_of([](const Connection &connection) {
        return connection.opened() && connection.invariants_hold();
    });
}

}  // namespace av::icd
