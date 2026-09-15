/* The ICD as the Windows Vulkan loader reaches it (contracts/loader_interface.md):
 * the answers to the loader's own entry points, the instances and their
 * connections, and every dispatchable handle made through them.
 *
 * Two locks, always taken in one order. The send lock is held by a connection
 * while a command is encoded and sent, and handle_to_wire takes the root's own
 * lock inside it; nothing here holds the root's lock while calling into a
 * wrapper, so the reverse never happens. Instance creation is serialized by the
 * loader, which holds its loader_lock around the whole of vkCreateInstance
 * (Vulkan-Loader bde79ad2dd83, trampoline.c:554-804): the connection opened for
 * an instance about to be made is therefore handed to the wrapper through one
 * pointer. */
#ifndef AV_ICD_ROOT_HPP
#define AV_ICD_ROOT_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/entry_points.hpp"
#include "icd/vulkan/handle_registry.hpp"
#include "icd/vulkan/loader_handle.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class BlockSource;
class ClientTransport;
class Kmt;
}  // namespace av

namespace av::icd {

class IcdBinding;
class LogSink;
class ThreadLock;
struct IcdRootProbe;

class IcdRoot {
public:
    /* The loader-ICD interface versions this answers (loader_interface.md). */
    static constexpr uint32_t latest_interface = 7u;
    static constexpr uint32_t oldest_interface = 5u;

    /* Binds itself into `binding` until destroyed. */
    IcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock, LogSink &log,
            IcdBinding &binding);
    ~IcdRoot();
    IcdRoot(const IcdRoot &) = delete;
    IcdRoot &operator=(const IcdRoot &) = delete;

    VkResult           negotiate(uint32_t *version);
    /* With no instance, only what the loader may ask before one exists; with an
     * instance this made, every entry point; with anything else, null. */
    PFN_vkVoidFunction instance_proc_addr(VkInstance instance, const char *name);
    /* Only an entry point the loader dispatches on a physical device: it names
     * no instance, and answering it anything else would have the loader call
     * that function with the wrong object. */
    PFN_vkVoidFunction physical_device_proc_addr(const char *name) const;
    VkResult           enumerate_instance_extensions(const char *layer, uint32_t *count,
                                                     VkExtensionProperties *properties);
    VkResult           enumerate_instance_version(uint32_t *version);
    VkResult           create_instance(const VkInstanceCreateInfo *create, const VkAllocationCallbacks *allocator,
                                       VkInstance *instance);
    VkResult           enumerate_adapter_physical_devices(VkInstance instance, uint64_t luid, uint32_t *count,
                                                          VkPhysicalDevice *physical_devices);
    void               destroy_device(VkDevice device, const VkAllocationCallbacks *allocator);
    void               physical_device_properties(VkPhysicalDevice physical_device,
                                                  VkPhysicalDeviceProperties *properties);
    void               physical_device_properties2(VkPhysicalDevice physical_device,
                                                   VkPhysicalDeviceProperties2 *properties);

    /* What the generated wrappers' hooks forward to (icd_hooks.cpp). */
    Connection      *acquire_connection();
    void             release_connection(Connection *connection);
    void            *wrap(LoaderHandle::Kind kind, uint64_t host, Connection *connection);
    Connection      *connection_of(void *handle, LoaderHandle::Kind kind);
    uint64_t         wire(void *handle, LoaderHandle::Kind kind);
    void             destroy_wrapper(void *handle, LoaderHandle::Kind kind);
    ClientTransport &record_target(VkCommandBuffer command_buffer);

    /* Every connection open, every handle on a live connection, and nothing
     * pending outside vkCreateInstance. */
    bool invariants_hold() const;

private:
    [[nodiscard]] Status open_connection(Connection *&opened);
    void                 note(const char *what, uint64_t value);
    uint32_t             handles_on_connections() const;

    BlockSource           &memory_;
    Kmt                   &kmt_;
    ThreadLock            &lock_;
    ThreadLock            &send_lock_;
    LogSink               &log_;
    IcdBinding            &binding_;
    uint32_t               api_version_;
    EntryPoints            before_instance_;
    EntryPoints            entry_points_;
    HandleRegistry         handles_;
    ObjectPool<Connection> connections_;
    Connection            *pending_;

    friend struct IcdRootProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_ROOT_HPP
