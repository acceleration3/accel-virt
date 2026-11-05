#ifndef AV_VULKAN_BACKEND_HPP
#define AV_VULKAN_BACKEND_HPP

#include "core/support/extension_names.hpp"
#include "core/support/object_pool.hpp"
#include "executor/vulkan/guest_device.hpp"
#include "executor/vulkan/vulkan_object_kind.hpp"
#include "executor/vulkan/guest_memory_types.hpp"
#include "executor/host_object_registry.hpp"
#include "executor/session_backend.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/vulkan/vulkan_submit_timing.hpp"
#include "executor/shim/physical_device_list.hpp"
#include "proto/generated/vulkan/vk_real_dispatch.h"

#include <stdint.h>

namespace av {

class DriverLog;
class OwnedFd;
class SharedImageShape;

class HostGpu;
class HostMemorySource;
class ImportTable;
class SourceMemory;

class VulkanBackend final : public vkproto::RealVulkanDispatch, public SessionBackend {
public:
    explicit VulkanBackend(DriverLog &lines);
    VulkanBackend(BlockSource &blocks, DriverLog &lines);

    void initialize(const HostGpu &gpu, HostMemorySource *source, ImportTable &imports);

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool lose_device() override;
    void end_batch() override;
    bool poll() override;
    void bind_signal(void (*signal)(void *context), void *context) override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;

    VkResult on_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkInstance *pInstance) override;

    VkResult on_vkCreateDescriptorUpdateTemplate(VkDevice device,
                                                 const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate) override;
    uint64_t template_bytes(VkDescriptorUpdateTemplate made) const override;

    VkResult on_vkCreateDevice(VkPhysicalDevice physicalDevice,
                               const VkDeviceCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) override;
    void     on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex,
                                 VkQueue *pQueue) override;
    VkResult on_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits,
                              VkFence fence) override;

    void on_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                                VkPhysicalDeviceMemoryProperties *pMemoryProperties) override;
    void on_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                                 VkPhysicalDeviceMemoryProperties2 *pMemoryProperties) override;
    void on_vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice,
                                                    VkPhysicalDeviceMemoryProperties2 *pMemoryProperties) override;

    VkResult on_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer) override;
    VkResult on_vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                              const VkAllocationCallbacks *pAllocator, VkImage *pImage) override;

    void on_vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer,
                                          VkMemoryRequirements *pMemoryRequirements) override;
    void on_vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo,
                                           VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetBufferMemoryRequirements2KHR(VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo,
                                              VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetImageMemoryRequirements(VkDevice device, VkImage image,
                                         VkMemoryRequirements *pMemoryRequirements) override;
    void on_vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo,
                                          VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo,
                                             VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements *pInfo,
                                                VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements *pInfo,
                                                   VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo,
                                               VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceImageMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo,
                                                  VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo,
                                                     uint32_t *pSparseMemoryRequirementCount,
                                                     VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements) override;
    void on_vkGetDeviceImageSparseMemoryRequirementsKHR(
        VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, uint32_t *pSparseMemoryRequirementCount,
        VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements) override;
    void on_vkGetDeviceImageSubresourceLayoutKHR(VkDevice device, const VkDeviceImageSubresourceInfo *pInfo,
                                                 VkSubresourceLayout2 *pLayout) override;

    void on_vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                         const VkAllocationCallbacks *pAllocator) override;

    VkResult on_vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR *pGetFdInfo,
                                 int *pFd) override;
    VkResult on_vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR *pGetFdInfo,
                                    int *pFd) override;
    VkResult on_vkGetFenceFdKHR(VkDevice device, const VkFenceGetFdInfoKHR *pGetFdInfo, int *pFd) override;
    VkResult on_vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd,
                                           VkMemoryFdPropertiesKHR *pMemoryFdProperties) override;
    VkResult on_vkImportSemaphoreFdKHR(VkDevice device,
                                       const VkImportSemaphoreFdInfoKHR *pImportSemaphoreFdInfo) override;
    VkResult on_vkImportFenceFdKHR(VkDevice device, const VkImportFenceFdInfoKHR *pImportFenceFdInfo) override;
    VkResult on_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkDeviceMemory *pMemory) override;

    VkResult on_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount,
                                           VkPhysicalDevice *pPhysicalDevices) override;
    VkResult on_vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
                                                VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties) override;
    VkResult on_vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
                                                   VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties) override;

    [[nodiscard]] bool adopt(SourceMemory &memory, uint64_t &handle) override;
    void abandon(const SourceMemory &memory, uint64_t handle) override;

    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     OwnedFd &descriptor) override;
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     const SharedImageShape &shape, uint64_t into,
                                     uint64_t &wire_handle, uint64_t &detail) override;
    void               release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail) override;

    void     shut_down() override;

    uint64_t ran() const;
    bool     room_for(uint64_t handles) override;
    void     made(vkproto::HandleKind kind, uint64_t handle) override;
    bool     handle_for(vkproto::HandleKind kind, uint64_t wire, uint64_t &handle) const override;
    void     made_through(uint64_t parent) override;
    void     ended(uint64_t raw) override;
    void     children_ended(uint64_t raw) override;
    bool     knows_handle(uint64_t raw) const;
    bool     knows_handle(uint64_t raw, VulkanObjectKind kind) const;

private:
    friend struct VulkanBackendProbe;

    [[nodiscard]] Status enable_identity(VkInstanceCreateInfo &create);
    void adopt_host_version(VkInstanceCreateInfo &create);
    [[nodiscard]] Status enable_memory_import(VkDeviceCreateInfo &create, uint32_t &importing);

    void keep(uint64_t raw, VulkanObjectKind kind, uint64_t bytes);
    void let_go(uint64_t raw, VulkanObjectKind kind);
    [[nodiscard]] bool kept_on(const GuestDevice &device, uint64_t made);
    GuestDevice       *forgot(uint64_t made);
    GuestDevice       *device_of(uint64_t device);
    GuestDevice       *owner_of(uint64_t made);
    [[nodiscard]] bool import_memory(GuestDevice &device, int fd, uint64_t size, uint64_t &wire_handle);
    [[nodiscard]] bool import_payload(int fd, uint64_t into, uint64_t &wire_handle, uint64_t &detail);
    bool               imports_nothing(const void *next) const;

    static constexpr uint64_t guest_semaphore = 1u;

    HostHeap          heap_;
    BlockSource      &blocks_;
    bool backs_with_source(VkDevice device, const void *next, VkStructureType external,
                           VkExternalMemoryHandleTypeFlags types) const;
    void for_source(VkDevice device, VkBufferCreateInfo &create, VkExternalMemoryBufferCreateInfo &external);
    bool for_source(VkDevice device, VkImageCreateInfo &create, VkExternalMemoryImageCreateInfo &external);
    void learn_types(VkPhysicalDevice physical);
    uint32_t image_bits(uint32_t host_bits, bool external) const;
    uint32_t                           first_memory_type(uint32_t mask) const;
    uint32_t                           import_type(const SourceMemory &memory, int fd) const;
    VkPhysicalDevice bound_device(VkInstance instance);

    static constexpr uint32_t lowest_api_version = 1u << 22;

    static constexpr uint32_t max_memory_types = 32u;

    const HostGpu     *gpu_;
    HostMemorySource  *source_;
    PhysicalDeviceList listed_;
    ExtensionNames     extensions_;
    VkApplicationInfo  application_;
    ObjectPool<GuestDevice> devices_;
    GuestDevice     *adopting_;
    VkPhysicalDevice physical_;
    bool             memory_import_;
    HostObjectRegistry<uint64_t, VulkanObjectKind, uint64_t> handles_;
    ImportTable     *imports_;
    GuestMemoryTypes types_;
    bool             types_learnt_;
    VulkanSubmitTiming timing_;
    uint64_t parent_;
    uint64_t ran_;
};

}

#endif
