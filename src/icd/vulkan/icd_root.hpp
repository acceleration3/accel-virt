/* The ICD as the Windows Vulkan loader reaches it (contracts/loader_interface.md):
 * the answers to the loader's own entry points, the instances and their
 * connections, and every dispatchable handle made through them.
 *
 * Once the host has faulted an instance's context, every entry point on its
 * objects that answers a VkResult answers VK_ERROR_DEVICE_LOST before anything
 * is asked of the Kmt; queries with no result answer nothing; frees and
 * destroys give back locally what the kernel holds for the process.
 *
 * Three locks, always taken in one order. The memory lock serializes every
 * device's memory, and is taken before either of the others. The send lock is
 * held by a connection while a command is encoded and sent, and the handle
 * translation takes the root's own lock inside it; nothing here holds the root's lock while
 * calling into a wrapper or taking another lock, so the reverse never happens. Instance creation is serialized by the
 * loader, which holds its loader_lock around the whole of vkCreateInstance
 * (Vulkan-Loader bde79ad2dd83, trampoline.c:554-804): the connection opened for
 * an instance about to be made is therefore handed to the wrapper through one
 * pointer. */
#ifndef AV_ICD_ROOT_HPP
#define AV_ICD_ROOT_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_reporter.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/device_memory.hpp"
#include "icd/vulkan/entry_points.hpp"
#include "icd/vulkan/extension_policy.hpp"
#include "icd/vulkan/handle_registry.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "icd/vulkan/icd_entry_lookup.hpp"
#include "icd/vulkan/instance_span.hpp"
#include "icd/vulkan/descriptor_template_table.hpp"
#include "icd/vulkan/guest_handle_translation.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/vulkan/external_objects.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/memory_win32_handle_record.hpp"
#include "icd/vulkan/semaphore_win32_handle_record.hpp"
#include "icd/vulkan/win32_handle_kind.hpp"
#include "icd/vulkan/presentation.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class ClientTransport;
class Kmt;
class ThreadLock;
}  // namespace av

namespace av::icd {

class ServiceThread;
class ThreadWake;
class WindowChains;
class WindowSurface;
class MemoryTypeTable;
struct IcdRootProbe;

class IcdRoot final : public StateReporter {
public:
    /* The loader-ICD interface versions this answers (loader_interface.md). */
    static constexpr uint32_t latest_interface = 7u;
    static constexpr uint32_t oldest_interface = 5u;

    /* Binds itself into `binding` until destroyed. The present thread and
     * what it is woken and locked by are Presentation's. */
    IcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock, ThreadLock &memory_lock,
            LogSink &log, IcdBinding &binding, WindowSurface &windows, WindowChains &chains,
            ServiceThread &present_thread, ThreadWake &handed, ThreadWake &shown, ThreadLock &present_lock);
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
    /* The device-level entry point of that name for a device this ICD made,
     * which the loader builds the device's dispatch table from; null for any
     * other device or level. */
    PFN_vkVoidFunction device_proc_addr(VkDevice device, const char *name);
    VkResult           enumerate_instance_extensions(const char *layer, uint32_t *count,
                                                     VkExtensionProperties *properties);
    VkResult           enumerate_instance_version(uint32_t *version);
    VkResult           create_instance(const VkInstanceCreateInfo *create, const VkAllocationCallbacks *allocator,
                                       VkInstance *instance);
    /* The physical devices of the instance on the adapter of `luid`, which
     * may be one of the instances `instance` spans (InstanceSpan); none for an
     * adapter it spans none on. */
    VkResult           enumerate_adapter_physical_devices(VkInstance instance, uint64_t luid, uint32_t *count,
                                                          VkPhysicalDevice *physical_devices);
    /* Every instance's devices `instance` spans, in their order: the primary
     * display's adapter's first. VK_INCOMPLETE when they do not all fit. */
    VkResult           enumerate_physical_devices(VkInstance instance, uint32_t *count,
                                                  VkPhysicalDevice *physical_devices);
    /* The same for device groups, each a group of one adapter's devices:
     * `enumerate` is the generated call the application's spelling names. */
    using GroupEnumerator = VkResult(VKAPI_CALL *)(VkInstance, uint32_t *, VkPhysicalDeviceGroupProperties *);
    VkResult           enumerate_physical_device_groups(GroupEnumerator enumerate, VkInstance instance,
                                                        uint32_t *count, VkPhysicalDeviceGroupProperties *groups);
    /* The host's list filtered by the extension policy; the host is asked how
     * many it has, then for all of them. */
    VkResult           enumerate_device_extensions(VkPhysicalDevice physical_device, const char *layer,
                                                   uint32_t *count, VkExtensionProperties *properties);
    /* A device naming an extension the policy would not report is refused
     * before the host sees it; one made is parented to its physical device. */
    VkResult           create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *create,
                                     const VkAllocationCallbacks *allocator, VkDevice *device);
    /* A device's host-visible memory is released before the device goes, and
     * an instance's devices' before the instance: the host refuses to unbind a
     * chunk it still has imported. */
    void               destroy_device(VkDevice device, const VkAllocationCallbacks *allocator);
    void               destroy_instance(VkInstance instance, const VkAllocationCallbacks *allocator);
    /* The host's memory types as the adapter presents them (MemoryTypeTable),
     * with VK_EXT_memory_budget's structures in the chain answered from the
     * adapter's budgets; all zero when the host's answer cannot be taken. */
    void               physical_device_memory_properties(VkPhysicalDevice physical_device,
                                                         VkPhysicalDeviceMemoryProperties *properties);
    void               physical_device_memory_properties2(VkPhysicalDevice physical_device,
                                                          VkPhysicalDeviceMemoryProperties2 *properties);
    /* DeviceMemory, under the memory lock: a device this ICD did not make
     * through a physical device allocates nothing and maps nothing. */
    VkResult           allocate_memory(VkDevice device, const VkMemoryAllocateInfo *info,
                                       const VkAllocationCallbacks *allocator, VkDeviceMemory *memory);
    void               free_memory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator);
    /* vkGetMemoryHostPointerPropertiesEXT (DeviceMemory::host_pointer_types). */
    VkResult           memory_host_pointer_properties(VkDevice device, VkExternalMemoryHandleTypeFlagBits type,
                                                      const void *pointer, VkMemoryHostPointerPropertiesEXT *properties);
    VkResult           map_memory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
                                  VkMemoryMapFlags flags, void **data);
    /* The same mapping out of a `VkMemoryMapInfoKHR` (feature 047). A placed
     * node in its chain, or the placed flag, is refused rather than ignored:
     * this ICD's view of a chunk is the address VidMm chose (research.md §9). */
    VkResult           map_memory2(VkDevice device, const VkMemoryMapInfoKHR *info, void **data);
    /* vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges, named by
     * `entry`: a mapping is the memory itself, so there is nothing to carry,
     * and only a device this ICD did not make, or a lost one, is refused. */
    VkResult           mapped_ranges(VkDevice device, const char *entry);
    void               physical_device_properties(VkPhysicalDevice physical_device,
                                                  VkPhysicalDeviceProperties *properties);
    void               physical_device_properties2(VkPhysicalDevice physical_device,
                                                   VkPhysicalDeviceProperties2 *properties);
    /* Queues and command buffers are made through their device, so destroying
     * the device destroys their handles and every command buffer's batch. */
    void               device_queue(VkDevice device, uint32_t family, uint32_t index, VkQueue *queue);
    void               device_queue2(VkDevice device, const VkDeviceQueueInfo2 *info, VkQueue *queue);
    VkResult           allocate_command_buffers(VkDevice device, const VkCommandBufferAllocateInfo *info,
                                                VkCommandBuffer *command_buffers);
    void               free_command_buffers(VkDevice device, VkCommandPool pool, uint32_t count,
                                            const VkCommandBuffer *command_buffers);
    /* Begin drops what the batch held and records into it; end ships it in one
     * publish; reset drops it and goes to the host at once, since a buffer
     * reset may never be recorded into again. */
    VkResult           begin_command_buffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *info);
    VkResult           end_command_buffer(VkCommandBuffer command_buffer);
    VkResult           reset_command_buffer(VkCommandBuffer command_buffer, VkCommandBufferResetFlags flags);
    /* Every host wait is a slice of at most 500 ms (SlicedWait). A queue waits
     * idle on a fence submitted with nothing, which completes after all prior
     * work on the queue; a device waits idle on each queue it handed out. */
    VkResult           wait_for_fences(VkDevice device, uint32_t count, const VkFence *fences, VkBool32 wait_all,
                                       uint64_t timeout);
    VkResult           wait_semaphores(VkDevice device, const VkSemaphoreWaitInfo *info, uint64_t timeout);
    VkResult           queue_wait_idle(VkQueue queue);
    /* vkGetQueryPoolResults; one that waits (VK_QUERY_RESULT_WAIT_BIT) polls
     * the host until every query is available (QueryWaitProbe). */
    VkResult           query_pool_results(VkDevice device, VkQueryPool pool, uint32_t first, uint32_t count,
                                          size_t size, void *data, VkDeviceSize stride, VkQueryResultFlags flags);
    VkResult           device_wait_idle(VkDevice device);
    /* Signals what an application gave vkAcquireNextImageKHR, on the first
     * queue `device` handed out: an empty submission that signals `semaphore`
     * and `fence`, either of which may be null. The image is already idle -
     * a present copies synchronously - but the host still needs a signal
     * operation before a submission waits on the semaphore: without one radv
     * blocks inside vkQueueSubmit, holding the context's worker for good. */
    VkResult           signal_acquire(VkDevice device, VkSemaphore semaphore, VkFence fence);
    /* The probe's lost step (feature 045): the instance's context is sent a
     * vkCreateInstance cut after its structure type, which the host faults it
     * for. VK_ERROR_DEVICE_LOST once it has; VK_ERROR_INITIALIZATION_FAILED
     * when the host answered instead, or for a device this did not make. It
     * faults only the caller's own context. */
    VkResult           force_context_fault(VkDevice device);

    /* What the generated wrappers' hooks forward to (icd_hooks.cpp). */
    Connection      *acquire_connection();
    void             release_connection(Connection *connection);
    void            *wrap(LoaderHandle::Kind kind, uint64_t host, Connection *connection);
    /* The WSI entries' state (feature 046). */
    Presentation    &presentation();
    /* What every command this ICD encodes translates its handles through. */
    vkproto::HandleTranslation &wire_handles();
    /* vkCreateDescriptorUpdateTemplate through the generated wrapper,
     * with what it made remembered; a create the host refused is not. */
    VkResult create_descriptor_update_template(VkDevice device,
                                               const VkDescriptorUpdateTemplateCreateInfo *create,
                                               const VkAllocationCallbacks *allocator,
                                               VkDescriptorUpdateTemplate *made);
    void     destroy_descriptor_update_template(VkDevice device, VkDescriptorUpdateTemplate made,
                                                const VkAllocationCallbacks *allocator);
    /* The Win32 external objects of VK_KHR_external_memory_win32 and
     * VK_KHR_external_semaphore_win32 (feature 047, US3, research.md §10). A
     * handle is this ICD's own and names an object of one device; an import
     * asks the host for a fresh fd of that object and forwards the host's own
     * import in the Win32 structure's place, which works because both host
     * devices are in the one process the device library runs in. Sharing
     * between two instances is not: the fd is a context's, and each instance
     * has its own. */
    VkResult export_memory_handle(VkDevice device, const MemoryGetWin32HandleRecord *request, void **handle);
    VkResult export_semaphore_handle(VkDevice device, const SemaphoreGetWin32HandleRecord *request,
                                     void **handle);
    VkResult import_semaphore_handle(VkDevice device, const ImportSemaphoreWin32HandleRecord *request);
    /* Which memory types a handle from outside this process may be imported
     * as: none, because a handle this ICD did not export names nothing
     * (contracts/external_objects.md). */
    VkResult memory_handle_properties(VkDevice device, void *handle);
    /* The NTSTATUS of the last D3DKMT call this driver made that *refused*,
     * for a probe that cannot see the driver's own Kmt: every kernel call
     * behind feature 053 answers the application VK_ERROR_TOO_MANY_OBJECTS, so
     * without this a guest run cannot say which of them refused.
     *
     * The refusal is latched rather than read back from the Kmt at the end,
     * because `last_code()` is the last call of any kind: a driver that has
     * just failed to share something goes on to free what it half-made, and
     * those calls succeed and wipe it. Two guest runs reported zero that way.
     * `device` is there for the loader to dispatch on and is checked like any
     * other handle. */
    VkResult last_kernel_status(VkDevice device, uint32_t *status);
    /* What the host says of its own opaque fd, said in Win32's spelling; every
     * other handle type is answered as absent, since nothing here turns a
     * Direct3D object into a host one. */
    void     external_buffer_properties(VkPhysicalDevice physical_device,
                                        const VkPhysicalDeviceExternalBufferInfo *info,
                                        VkExternalBufferProperties               *properties);
    void     external_semaphore_properties(VkPhysicalDevice physical_device,
                                           const VkPhysicalDeviceExternalSemaphoreInfo *info,
                                           VkExternalSemaphoreProperties               *properties);
    /* vkDestroySemaphore through the generated wrapper, with a handle that
     * named it forgotten: a handle outliving what it names is one an import
     * could still follow. */
    void     destroy_semaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks *allocator);
    /* What a swapchain of `device` needs: its physical device, and the
     * connection whose adapter its vertical blank is waited on and whose wire
     * its window's chain is opened through; false for a device this ICD did
     * not make. */
    bool             present_context(VkDevice device, VkPhysicalDevice &physical, Connection *&connection);
    /* The entry point of that name, at whatever level it is: what the ICD's own
     * swapchains call, having already found the device they belong to. */
    PFN_vkVoidFunction entry(const char *name) const;
    Connection      *connection_of(void *handle, LoaderHandle::Kind kind);
    void             destroy_wrapper(void *handle, LoaderHandle::Kind kind);
    /* The loader reached this through the command buffer's own dispatch table,
     * so it is a command buffer this ICD made. */
    CommandBatch    &record_target(VkCommandBuffer command_buffer);

    /* Every connection open, every handle on a live connection, every batch
     * owned by a command buffer, every device memory by a device, and nothing
     * pending outside vkCreateInstance. */
    bool invariants_hold() const;

    /* `avvk/0` with the counts, then each connection with its handles
     * (docs/trace.md). Never waits: with the root's lock held elsewhere only
     * `avvk/0 busy=true` is written. */
    void report(StateWriter &writer) override;

private:
    /* A connection opened on the adapter of `luid` (Connection::open). */
    [[nodiscard]] Status open_connection(Connection *&opened, uint64_t luid);
    /* An instance of this driver's on every avgpu adapter `own` is not on,
     * made as `own` was, spanned with it. An adapter that will not take one is
     * left out of the span and logged. */
    void                 span_adapters(const VkInstanceCreateInfo &forwarded, const VkAllocationCallbacks *allocator,
                                       VkInstance own);
    /* The span the application's `instance` heads, or null. Under the root's
     * lock. */
    InstanceSpan        *span_of(VkInstance instance);
    /* One instance and what is still made under it, as vkDestroyInstance takes
     * an instance spanning no other. */
    void                 destroy_one_instance(VkInstance instance, const VkAllocationCallbacks *allocator);
    /* The host's memory types into `types`, unless the connection is lost
     * before or while they are asked for. */
    void                 learn_types(Connection &connection, VkPhysicalDevice physical_device,
                                     MemoryTypeTable &types, const char *entry);
    LoaderHandle        *wrap_under(LoaderHandle::Kind kind, uint64_t host, Connection *connection,
                                    LoaderHandle *parent);
    void                *wrap_child(LoaderHandle::Kind kind, uint64_t host, LoaderHandle &parent);
    LoaderHandle        *handle_of(void *handle, LoaderHandle::Kind kind);
    LoaderHandle        *child_of(const LoaderHandle &parent, LoaderHandle::Kind kind, uint32_t index);
    uint32_t             handles_on_connections() const;
    /* The device's memory, made the first time it is asked for; null when
     * there is no memory for it. Under the memory lock. */
    DeviceMemory        *memory_of(LoaderHandle &device);
    /* The device at `device`, made through a physical device, or null. */
    LoaderHandle        *allocating_device(VkDevice device);
    void                 release_device_memory(LoaderHandle &device);
    void                 release_idle_fences(LoaderHandle &device);

    BlockSource           &memory_;
    Kmt                   &kmt_;
    ThreadLock            &lock_;
    ThreadLock            &send_lock_;
    ThreadLock            &memory_lock_;
    LogSink               &log_;
    KeyedLog              lines_;
    IcdBinding            &binding_;
    uint32_t               api_version_;
    EntryPoints            before_instance_;
    EntryPoints            entry_points_;
    ExtensionPolicy        extensions_;
    /* Before the handles: a command buffer's handle destroys its batch in this
     * pool, so the pool outlives every handle. */
    ObjectPool<CommandBatch> batches_;
    ObjectPool<DeviceMemory> memories_;
    HandleRegistry         handles_;
    /* The NT handles this process holds for the objects it shares, which is
     * what makes one of them nameable in another process (feature 053).
     * Before the connections: each opens what a handle names through it. */
    SharedHandles           shared_handles_;
    ObjectPool<Connection> connections_;
    Connection            *pending_;
    /* One per application instance: the instances it spans. */
    ObjectPool<InstanceSpan> spans_;
    /* After the handles and connections: a swapchain's destruction calls the
     * device it was made on. */
    IcdEntryLookup         entries_;
    Presentation           presentation_;
    DescriptorTemplateTable templates_;
    GuestHandleTranslation  translation_;
    /* The objects this process exported, each with the identity and the NT
     * handle it was published under; what it opened is its connection's
     * (OpenedObjects). */
    ExternalObjects         externals_;
    /* The NTSTATUS of the last kernel call that refused; 0 until one does. */
    uint32_t                kernel_refusal_;
    /* vkQueueWaitIdle calls answered here and sent to the host, for the log. */
    uint64_t                idle_skipped_;
    uint64_t                idle_waited_;
    Win32HandleKind         memory_handles_;
    Win32HandleKind         semaphore_handles_;
    /* A fresh fd of an object of `owner`, asked of the host each time an
     * import names it: a driver takes ownership of an fd it imports, so one fd
     * could not serve two imports. Asked through the importing device's own
     * connection, so an owner of another instance is a handle that context
     * cannot name and the host refuses it - which is what sharing between two
     * instances is, until feature 053. */
    VkResult                host_memory_fd(LoaderHandle &made, VkDevice owner, VkDeviceMemory memory, int &fd);
    VkResult                host_semaphore_fd(LoaderHandle &made, VkDevice owner, VkSemaphore semaphore, int &fd);
    /* The Win32 import node of an allocation's chain, or null. */
    const ImportMemoryWin32HandleRecord *imported_memory_record(const void *chain) const;
    /* Whether an allocation's chain exports or imports a Win32 handle, which
     * is what makes it the host's to allocate whole. */
    bool                    shares_memory(const void *chain) const;
    /* An allocation that shares memory: imported from a handle, or allocated
     * on the host and published so another process can open it. */
    VkResult                shared_allocation(LoaderHandle &made, VkDevice device, const VkMemoryAllocateInfo &info,
                                              const VkAllocationCallbacks *allocator, VkDeviceMemory &memory);
    /* Memory this process published, imported into another of its devices the
     * way 047 did it: a fresh fd, and a VkDeviceMemory of that device's own. */
    VkResult                own_process_allocation(LoaderHandle &made, VkDevice device,
                                                   const ExternalObjectSlot &holder,
                                                   const VkMemoryAllocateInfo &info,
                                                   const VkAllocationCallbacks *allocator,
                                                   VkDeviceMemory &memory);
    /* Memory a Win32 handle names, opened into this context. */
    VkResult                imported_allocation(LoaderHandle &made, VkDevice device,
                                                const ImportMemoryWin32HandleRecord &importing,
                                                const VkMemoryAllocateInfo &info,
                                                const VkAllocationCallbacks *allocator, uint64_t size,
                                                VkDeviceMemory &memory);
    /* An object of this process given an identity another process can name: a
     * one-page carrier allocation the miniport mints into - the host object
     * itself has no guest allocation, and the carrier's pages are never read -
     * the object published on the wire under that id, and an NT handle over
     * the carrier. Exactly one of `memory` and `semaphore` is set.
     * VK_ERROR_TOO_MANY_OBJECTS or VK_ERROR_INVALID_EXTERNAL_HANDLE, with
     * nothing left behind, for every refusal on the way. */
    VkResult                publish(LoaderHandle &made, VkDevice device, uint32_t kind, uint64_t size,
                                    VkDeviceMemory memory, VkSemaphore semaphore);
    /* The object id `handle` names: one this process published, or one another
     * process sent, which is opened here. VK_ERROR_INVALID_EXTERNAL_HANDLE for
     * a handle from nowhere, VK_ERROR_OUT_OF_HOST_MEMORY when there is no row
     * to keep it in; `object_id` is 0 unless VK_SUCCESS. `api` names the
     * entry point in the line a refused handle writes. */
    VkResult                object_of(LoaderHandle &made, void *handle, const char *api, uint64_t &object_id);
    /* Everything one row cost, given back: the device's reference to the
     * object, the NT handle, and the carrier allocation nothing else knows
     * about. */
    void                    release_shared(LoaderHandle &made, const ExternalObjectSlot &row);
    /* A kernel call this driver made refused: written to the log with its
     * NTSTATUS, and kept for a probe to read afterwards. */
    void                    refused(const char *what);
    /* Asks the host, once per connection, whether it has the dma-buf
     * extension a shared allocation is exported through. */
    void                    learn_host_sharing(LoaderHandle &made, VkPhysicalDevice physical_device);
    /* What a shared allocation is exported and imported as on `connection`'s
     * host: a dma-buf where it has one, an opaque fd otherwise. */
    VkFlags                 memory_share_type(const Connection &connection) const;

    friend struct IcdRootProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_ROOT_HPP
