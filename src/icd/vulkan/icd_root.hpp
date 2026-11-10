#ifndef AV_ICD_ROOT_HPP
#define AV_ICD_ROOT_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_reporter.hpp"
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/device_memory.hpp"
#include "icd/vulkan/entry_points.hpp"
#include "icd/vulkan/extension_policy.hpp"
#include "icd/vulkan/fence_wakes.hpp"
#include "icd/vulkan/handle_registry.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "icd/vulkan/icd_entry_lookup.hpp"
#include "icd/vulkan/instance_span.hpp"
#include "icd/vulkan/descriptor_template_table.hpp"
#include "proto/vulkan/host_copy_images.hpp"
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
class KeyedLog;
}

namespace av {
class BlockSource;
class ClientTransport;
class Kmt;
class ThreadLock;
}

namespace av::icd {

class ServiceThread;
class ThreadWake;
class WindowChains;
class WindowSurface;
class MemoryTypeTable;
struct IcdRootProbe;

class IcdRoot final : public StateReporter {
public:
    static constexpr uint32_t latest_interface = 7u;
    static constexpr uint32_t oldest_interface = 5u;

    IcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock, ThreadLock &memory_lock,
            KeyedLog &log, IcdBinding &binding, WindowSurface &windows, WindowChains &chains,
            ServiceThread &present_thread, ThreadWake &handed, ThreadWake &shown, ThreadLock &present_lock,
            ServiceThread &wake_thread);
    ~IcdRoot();
    IcdRoot(const IcdRoot &) = delete;
    IcdRoot &operator=(const IcdRoot &) = delete;

    VkResult           negotiate(uint32_t *version);
    PFN_vkVoidFunction instance_proc_addr(VkInstance instance, const char *name);
    PFN_vkVoidFunction physical_device_proc_addr(const char *name) const;
    PFN_vkVoidFunction device_proc_addr(VkDevice device, const char *name);
    VkResult           enumerate_instance_extensions(const char *layer, uint32_t *count,
                                                     VkExtensionProperties *properties);
    VkResult           enumerate_instance_version(uint32_t *version);
    VkResult           create_instance(const VkInstanceCreateInfo *create, const VkAllocationCallbacks *allocator,
                                       VkInstance *instance);
    VkResult           enumerate_adapter_physical_devices(VkInstance instance, uint64_t luid, uint32_t *count,
                                                          VkPhysicalDevice *physical_devices);
    VkResult           enumerate_physical_devices(VkInstance instance, uint32_t *count,
                                                  VkPhysicalDevice *physical_devices);
    using GroupEnumerator = VkResult(VKAPI_CALL *)(VkInstance, uint32_t *, VkPhysicalDeviceGroupProperties *);
    VkResult           enumerate_physical_device_groups(GroupEnumerator enumerate, VkInstance instance,
                                                        uint32_t *count, VkPhysicalDeviceGroupProperties *groups);
    VkResult           enumerate_device_extensions(VkPhysicalDevice physical_device, const char *layer,
                                                   uint32_t *count, VkExtensionProperties *properties);
    VkResult           create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *create,
                                     const VkAllocationCallbacks *allocator, VkDevice *device);
    void               destroy_device(VkDevice device, const VkAllocationCallbacks *allocator);
    void               destroy_instance(VkInstance instance, const VkAllocationCallbacks *allocator);
    void               physical_device_memory_properties(VkPhysicalDevice physical_device,
                                                         VkPhysicalDeviceMemoryProperties *properties);
    void               physical_device_memory_properties2(VkPhysicalDevice physical_device,
                                                          VkPhysicalDeviceMemoryProperties2 *properties);
    VkResult           create_semaphore(VkDevice device, const VkSemaphoreCreateInfo *info,
                                        const VkAllocationCallbacks *allocator, VkSemaphore *semaphore);
    VkResult           allocate_memory(VkDevice device, const VkMemoryAllocateInfo *info,
                                       const VkAllocationCallbacks *allocator, VkDeviceMemory *memory);
    void               free_memory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator);
    VkResult           memory_host_pointer_properties(VkDevice device, VkExternalMemoryHandleTypeFlagBits type,
                                                      const void *pointer, VkMemoryHostPointerPropertiesEXT *properties);
    VkResult           map_memory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
                                  VkMemoryMapFlags flags, void **data);
    VkResult           map_memory2(VkDevice device, const VkMemoryMapInfoKHR *info, void **data);
    VkResult           mapped_ranges(VkDevice device, const char *entry);
    void               physical_device_properties(VkPhysicalDevice physical_device,
                                                  VkPhysicalDeviceProperties *properties);
    void               physical_device_properties2(VkPhysicalDevice physical_device,
                                                   VkPhysicalDeviceProperties2 *properties);
    void               device_queue(VkDevice device, uint32_t family, uint32_t index, VkQueue *queue);
    void               device_queue2(VkDevice device, const VkDeviceQueueInfo2 *info, VkQueue *queue);
    VkResult           allocate_command_buffers(VkDevice device, const VkCommandBufferAllocateInfo *info,
                                                VkCommandBuffer *command_buffers);
    void               free_command_buffers(VkDevice device, VkCommandPool pool, uint32_t count,
                                            const VkCommandBuffer *command_buffers);
    VkResult           begin_command_buffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *info);
    VkResult           end_command_buffer(VkCommandBuffer command_buffer);
    VkResult           reset_command_buffer(VkCommandBuffer command_buffer, VkCommandBufferResetFlags flags);
    VkResult           wait_for_fences(VkDevice device, uint32_t count, const VkFence *fences, VkBool32 wait_all,
                                       uint64_t timeout);
    VkResult           wait_semaphores(VkDevice device, const VkSemaphoreWaitInfo *info, uint64_t timeout);
    VkResult           queue_wait_idle(VkQueue queue);
    VkResult           query_pool_results(VkDevice device, VkQueryPool pool, uint32_t first, uint32_t count,
                                          size_t size, void *data, VkDeviceSize stride, VkQueryResultFlags flags);
    VkResult           device_wait_idle(VkDevice device);
    VkResult           signal_acquire(VkDevice device, VkSemaphore semaphore, VkFence fence);
    VkResult           force_context_fault(VkDevice device);

    Connection      *acquire_connection();
    void             release_connection(Connection *connection);
    void            *wrap(LoaderHandle::Kind kind, uint64_t host, Connection *connection);
    Presentation    &presentation();
    vkproto::HandleTranslation &wire_handles();
    VkResult create_descriptor_update_template(VkDevice device,
                                               const VkDescriptorUpdateTemplateCreateInfo *create,
                                               const VkAllocationCallbacks *allocator,
                                               VkDescriptorUpdateTemplate *made);
    void     destroy_descriptor_update_template(VkDevice device, VkDescriptorUpdateTemplate made,
                                                const VkAllocationCallbacks *allocator);
    VkResult create_image(VkDevice device, const VkImageCreateInfo *create, const VkAllocationCallbacks *allocator,
                          VkImage *made);
    void     destroy_image(VkDevice device, VkImage image, const VkAllocationCallbacks *allocator);
    VkResult export_memory_handle(VkDevice device, const MemoryGetWin32HandleRecord *request, void **handle);
    VkResult export_semaphore_handle(VkDevice device, const SemaphoreGetWin32HandleRecord *request,
                                     void **handle);
    VkResult import_semaphore_handle(VkDevice device, const ImportSemaphoreWin32HandleRecord *request);
    VkResult memory_handle_properties(VkDevice device, void *handle);
    VkResult last_kernel_status(VkDevice device, uint32_t *status);
    void     external_buffer_properties(VkPhysicalDevice physical_device,
                                        const VkPhysicalDeviceExternalBufferInfo *info,
                                        VkExternalBufferProperties               *properties);
    void     external_semaphore_properties(VkPhysicalDevice physical_device,
                                           const VkPhysicalDeviceExternalSemaphoreInfo *info,
                                           VkExternalSemaphoreProperties               *properties);
    void     destroy_semaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks *allocator);
    bool             present_context(VkDevice device, VkPhysicalDevice &physical, uint64_t &host_device,
                                     Connection *&connection);
    PFN_vkVoidFunction entry(const char *name) const;
    Connection      *connection_of(void *handle, LoaderHandle::Kind kind);
    void             destroy_wrapper(void *handle, LoaderHandle::Kind kind);
    CommandBatch    &record_target(VkCommandBuffer command_buffer);

    bool invariants_hold() const;

    void report(StateWriter &writer) override;

private:
    [[nodiscard]] Status open_connection(Connection *&opened, uint64_t luid);
    void                 span_adapters(const VkInstanceCreateInfo &forwarded, const VkAllocationCallbacks *allocator,
                                       VkInstance own);
    InstanceSpan        *span_of(VkInstance instance);
    void                 destroy_one_instance(VkInstance instance, const VkAllocationCallbacks *allocator);
    void                 learn_types(Connection &connection, VkPhysicalDevice physical_device,
                                     MemoryTypeTable &types, const char *entry);
    void                 learn_types_with_budget(Connection &connection, VkPhysicalDevice physical_device,
                                                 MemoryTypeTable &types, const char *entry,
                                                 VkPhysicalDeviceMemoryBudgetPropertiesEXT &hosted);
    LoaderHandle        *wrap_under(LoaderHandle::Kind kind, uint64_t host, Connection *connection,
                                    LoaderHandle *parent);
    void                *wrap_child(LoaderHandle::Kind kind, uint64_t host, LoaderHandle &parent);
    LoaderHandle        *handle_of(void *handle, LoaderHandle::Kind kind);
    LoaderHandle        *child_of(const LoaderHandle &parent, LoaderHandle::Kind kind, uint32_t index);
    uint32_t             handles_on_connections() const;
    DeviceMemory        *memory_of(LoaderHandle &device);
    LoaderHandle        *allocating_device(VkDevice device);
    void                 release_device_memory(LoaderHandle &device);
    void                 release_idle_fences(LoaderHandle &device);

    BlockSource           &memory_;
    Kmt                   &kmt_;
    ThreadLock            &lock_;
    ThreadLock            &send_lock_;
    ThreadLock            &memory_lock_;
    KeyedLog              &lines_;
    IcdBinding            &binding_;
    uint32_t               api_version_;
    EntryPoints            before_instance_;
    EntryPoints            entry_points_;
    ExtensionPolicy        extensions_;
    ObjectPool<CommandBatch> batches_;
    ObjectPool<DeviceMemory> memories_;
    HandleRegistry         handles_;
    SharedHandles           shared_handles_;
    ObjectPool<Connection> connections_;
    Connection            *pending_;
    ObjectPool<InstanceSpan> spans_;
    IcdEntryLookup         entries_;
    Presentation           presentation_;
    FenceWakes             wakes_;
    DescriptorTemplateTable templates_;
    HostCopyImages          host_copies_;
    GuestHandleTranslation  translation_;
    ExternalObjects         externals_;
    uint32_t                kernel_refusal_;
    uint64_t                idle_skipped_;
    uint64_t                idle_waited_;
    Win32HandleKind         memory_handles_;
    Win32HandleKind         semaphore_handles_;
    const ImportMemoryWin32HandleRecord *imported_memory_record(const void *chain) const;
    bool                    shares_memory(const void *chain) const;
    bool                    shares_semaphore(const void *chain) const;
    VkResult                shared_allocation(LoaderHandle &made, VkDevice device, const VkMemoryAllocateInfo &info,
                                              const VkAllocationCallbacks *allocator, VkDeviceMemory &memory);
    VkResult                publish(LoaderHandle &made, VkDevice device, uint32_t kind, uint64_t size,
                                    VkDeviceMemory memory, VkSemaphore semaphore);
    VkResult                imported_allocation(LoaderHandle &made, const ImportMemoryWin32HandleRecord &importing,
                                                uint64_t size, VkDeviceMemory &memory);
    VkResult                opened_answer(Status opened, const char *api);
    void                    release_shared(LoaderHandle &made, const ExternalObjectSlot &row);
    void                    refused(const char *what);
    void                    learn_host_sharing(LoaderHandle &made, VkPhysicalDevice physical_device);
    VkFlags                 memory_share_type(const Connection &connection) const;

    friend struct IcdRootProbe;
};

}

#endif
