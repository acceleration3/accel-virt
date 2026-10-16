/* One guest context's Vulkan, run on the host's own.
 *
 * The generated `RealVulkanDispatch` already forwards all 386 commands the
 * profile carries into the host loader, so what this adds is not forwarding:
 * it is the three answers the ABI needs and the generated code does not give -
 * whether an id is carried at all, whether a decode failed, and whether the
 * host device is gone. Deriving from the generated dispatch rather than
 * reimplementing it is also what makes widening feature 063's profile a
 * behaviour change instead of a build break. */
#ifndef AV_VULKAN_BACKEND_HPP
#define AV_VULKAN_BACKEND_HPP

#include "core/support/extension_names.hpp"
#include "executor/vulkan/host_exported_fds.hpp"
#include "executor/vulkan/vulkan_object_kind.hpp"
#include "executor/vulkan/guest_memory_types.hpp"
#include "executor/host_object_registry.hpp"
#include "executor/session_backend.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/vulkan/vulkan_submit_timing.hpp"
#include "executor/shim/physical_device_list.hpp"
#include "executor/shim/vulkan_memory_import.hpp"
#include "proto/generated/vulkan/vk_real_dispatch.h"

#include <stdint.h>

namespace av {

class SharedImageShape;

class HostGpu;
class HostMemorySource;
class ImportTable;
class SourceMemory;

class VulkanBackend final : public vkproto::RealVulkanDispatch, public SessionBackend {
public:
    /* Its tables grow from a heap of its own; a test hands it the source to
     * grow from instead. */
    VulkanBackend();
    explicit VulkanBackend(BlockSource &blocks);

    /* Binds this context to the GPU the device is bound to, which must outlive
     * it: the one physical device an enumeration answers, and the memory
     * source the guest's host-visible memory comes from (null: none); and to
     * its session's imports, which memory the guest frees is looked up in. */
    void initialize(const HostGpu &gpu, HostMemorySource *source, ImportTable &imports);

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    /* The device the guest made, lost through the layer that loses one on
     * request (tests/gpu_fault): false with no device or no such layer. The
     * guest learns it as a Vulkan application does, VK_ERROR_DEVICE_LOST
     * from its next submission or wait. */
    [[nodiscard]] bool lose_device() override;
    void end_batch() override;
    bool poll() override;
    /* Its commands finish as they run: nothing to wake the worker for. */
    void bind_signal(void (*signal)(void *context), void *context) override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;

    /* Made with what the host needs of an instance (enable_identity,
     * adopt_host_version). */
    VkResult on_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkInstance *pInstance) override;

    /* The descriptor update templates this context made, kept with what one
     * update through each reads, because the length of a blob a WithTemplate
     * command carries is the template's and the guest's claim about it is
     * checked against this (feature 047). */
    VkResult on_vkCreateDescriptorUpdateTemplate(VkDevice device,
                                                 const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate) override;
    /* The bytes one update through `made` reads, for the generated decoder's
     * hook; 0 for a template this context never made. */
    uint64_t template_bytes(VkDescriptorUpdateTemplate made) const;

    /* A device is made able to import a host pointer whether or not the guest
     * asked, because every host-visible allocation the guest makes arrives as
     * one (feature 045); a physical device that cannot is made the device the
     * guest asked for. */
    VkResult on_vkCreateDevice(VkPhysicalDevice physicalDevice,
                               const VkDeviceCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) override;
    void     on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex,
                                 VkQueue *pQueue) override;
    VkResult on_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits,
                              VkFence fence) override;

    /* The guest's view of the GPU's memory (GuestMemoryTypes): host visible
     * only where the memory source backs a type, a device-local type where
     * the GPU's are all host visible, in the spec's order. The rest of the
     * structure - a budget's heaps - is the driver's. */
    void on_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                                VkPhysicalDeviceMemoryProperties *pMemoryProperties) override;
    void on_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                                 VkPhysicalDeviceMemoryProperties2 *pMemoryProperties) override;
    void on_vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice,
                                                    VkPhysicalDeviceMemoryProperties2 *pMemoryProperties) override;

    /* Made for host allocation wherever the driver allows it, because the
     * guest's ICD cannot know at create whether the memory bound later will be
     * a chunk the host imports (feature 045, T035); the guest is answered the
     * requirements of what was made. */
    VkResult on_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer) override;
    VkResult on_vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                              const VkAllocationCallbacks *pAllocator, VkImage *pImage) override;

    /* The driver's memoryTypeBits as the guest's types. An image that cannot
     * be bound to the memory source's memory - one not made with its handle
     * type, which is how a driver keeps an optimal image compressed - is not
     * offered the types the source backs, unless no other type would do: a
     * compressed image in memory the guest maps is one whose metadata the
     * guest's mapping can corrupt. The create-info spellings answer what the
     * made object would. */
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
    /* The decoder refuses a frame whose create info is left out, so every
     * one of these has one. */
    void on_vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements *pInfo,
                                                VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements *pInfo,
                                                   VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo,
                                               VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceImageMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo,
                                                  VkMemoryRequirements2 *pMemoryRequirements) override;
    /* The other create-info queries of an image, asked of the image as
     * on_vkCreateImage would make it. */
    void on_vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo,
                                                     uint32_t *pSparseMemoryRequirementCount,
                                                     VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements) override;
    void on_vkGetDeviceImageSparseMemoryRequirementsKHR(
        VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, uint32_t *pSparseMemoryRequirementCount,
        VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements) override;
    void on_vkGetDeviceImageSubresourceLayoutKHR(VkDevice device, const VkDeviceImageSubresourceInfo *pInfo,
                                                 VkSubresourceLayout2 *pLayout) override;

    /* Memory an import made is the import's to release: freed here and again
     * when the device releases the blob would be a double free in the driver. */
    void on_vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                         const VkAllocationCallbacks *pAllocator) override;

    /* The fds this context exported, and what an import may name (feature 047,
     * US3): an fd a guest sends is a claim that this context exported it, and
     * one it made up names something else in this process (constitution V).
     * The table owns what it exported, so an export nothing imported is closed
     * with its device rather than held until the process exits. */
    VkResult on_vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR *pGetFdInfo,
                                 int *pFd) override;
    VkResult on_vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR *pGetFdInfo,
                                    int *pFd) override;
    /* Of an fd this context exported only, in the guest's types. */
    VkResult on_vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd,
                                           VkMemoryFdPropertiesKHR *pMemoryFdProperties) override;
    /* Of the host type behind the guest's: VK_ERROR_OUT_OF_DEVICE_MEMORY for
     * a type the guest was never shown, which no driver checks. */
    VkResult on_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkDeviceMemory *pMemory) override;
    VkResult on_vkImportSemaphoreFdKHR(VkDevice device,
                                       const VkImportSemaphoreFdInfoKHR *pImportSemaphoreFdInfo) override;

    /* The physical device of the bound GPU and no other, as a list of one and
     * as a group of one: the host's own lists name every GPU it has, and a
     * handle the guest was never shown is one it can never name (feature 044,
     * T034). A context not bound, or whose instance does not list its GPU,
     * is shown none. */
    VkResult on_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount,
                                           VkPhysicalDevice *pPhysicalDevices) override;
    VkResult on_vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
                                                VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties) override;
    VkResult on_vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
                                                   VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties) override;

    /* Imports the guest's pages into the guest's device as the memory type
     * the blob asks for - refused for a kind this device does not import, and
     * for a type the driver will not import that handle into - and makes the
     * memory a handle the guest may name. */
    [[nodiscard]] bool adopt(const SourceMemory &memory, uint64_t &handle) override;
    void abandon(const SourceMemory &memory, uint64_t handle) override;

    /* One host object shared with every other context of this device (feature
     * 053). Vulkan is the only API here that exports anything, so this is the
     * only backend that overrides these: memory as the handle type this GPU
     * imports, a fence as an opaque-fd timeline semaphore. A wire handle the
     * guest names that this backend never made is refused, since the guest
     * would otherwise name any number and have the driver read it. */
    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     int &fd) override;
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     const SharedImageShape &shape,
                                     uint64_t &wire_handle, uint64_t &detail) override;
    void               release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail) override;

    /* Destroys whatever the guest still owned - every device it made, then
     * every instance - and forgets every handle. A context's host objects are
     * the context's, and a guest that never destroys them must not be able to
     * leak them by dying (feature 042). */
    void     shut_down() override;

    /* Commands run since this backend was made, which is what a conformance
     * run counts. */
    uint64_t ran() const;
    /* Whether `handles` more can be remembered: asked by the decoder before
     * a command makes any, which refuses the command as out of host memory
     * when there is no room. */
    bool     room_for(uint64_t handles);
    /* Records a handle this backend is about to hand the guest, made through
     * whatever the command being decoded named with made_through, into the
     * room room_for made for it. */
    void     made(uint64_t raw, VulkanObjectKind kind = VulkanObjectKind::other);
    /* The object the command being decoded makes its handles through - its
     * device, its pool - so that they end when it does. Nothing between
     * commands: every run forgets it when the decode returns. */
    void     made_through(uint64_t parent);
    /* A destroy or free the decoder ran: `raw`, and everything made through
     * it, is no longer a handle the guest can name. */
    void     ended(uint64_t raw);
    /* A reset that freed what was made through `raw` and left `raw`. */
    void     children_ended(uint64_t raw);
    /* Whether this backend made the handle a guest just named. What the
     * generated decoder asks through wire_to_handle. */
    bool     knows_handle(uint64_t raw) const;
    /* And that it made it as this kind, which is what the five handles the
     * loader dispatches on are judged by. */
    bool     knows_handle(uint64_t raw, VulkanObjectKind kind) const;
    /* The backend whose handles the decoder is inside. One context runs on one
     * worker and one call at a time - the ABI says so - so the answer is the
     * thread's, which is the only shape a hook with no user pointer can take. */
    static VulkanBackend *deciding();

private:
    friend struct VulkanBackendProbe;

    /* Decodes one command into `driver` with this backend deciding its
     * handles. Always this backend's own driver but in a test, which puts one
     * with no GPU behind it there. */
    [[nodiscard]] bool run_through(vkproto::ServerDispatch &driver, uint32_t command_id,
                                   WireReader &payload, WireWriter &reply, WireArena &arena);

    /* Adds VK_KHR_get_physical_device_properties2 to the extensions an
     * instance is made with, unless the guest enabled it or its list has no
     * room. On an instance made for Vulkan 1.0 without it, the loader answers
     * vkGetPhysicalDeviceProperties2 from the 1.0 call and never fills
     * VkPhysicalDeviceIDProperties, so no physical device would be the bound
     * GPU. Enabling a promoted extension is valid at any version, and the
     * guest's own instance - in the guest - never sees it. */
    [[nodiscard]] Status enable_identity(VkInstanceCreateInfo &create);
    /* Raises the Vulkan version an instance is made for to the host loader's.
     * The server calls a promoted alias - vkCmdPipelineBarrier2KHR - through
     * its core symbol, the only one the loader exports, and Mesa fills a core
     * slot only on an instance made for that core version: a guest asking for
     * 1.2 with VK_KHR_synchronization2 enabled would reach a null slot and
     * take the host process down. The device still gets no more than its GPU
     * supports, and an alias does exactly what its core command does. */
    void adopt_host_version(VkInstanceCreateInfo &create);
    /* Adds the dma-buf import extensions, then the modifier extension, to the
     * ones a device is made with, unless the guest named them; `importing` is
     * the count before the modifier extension. no_memory when the list cannot
     * be made; a list the loader refuses is left as the guest sent it. */
    [[nodiscard]] Status enable_memory_import(VkDeviceCreateInfo &create, uint32_t &importing);

    /* `raw`, made through parent_, held as `kind` with `bytes`: one already
     * held keeps what it was made through and what it is - a queue fetched
     * again, a physical device enumerated again. */
    void keep(uint64_t raw, VulkanObjectKind kind, uint64_t bytes);
    /* A device or an instance the guest never destroyed, destroyed. */
    void let_go(uint64_t raw, VulkanObjectKind kind);

    /* Where the tables below grow from: first, as they are destroyed after it. */
    HostHeap          heap_;
    BlockSource      &blocks_;
    HostExportedFds   fds_;
    /* Whether every fd the chain of an allocation names is one this context
     * exported, answering the last of them in `fd` - -1 for a chain that
     * imports nothing. False is the one case an allocation is refused here. */
    [[nodiscard]] bool exported(const void *next, int &fd);
    /* `answered`, with an fd the driver took forgotten. */
    VkResult           imported(VkResult answered, int fd);
    /* An fd the driver answered, kept as this context's; VK_ERROR_TOO_MANY_OBJECTS
     * when the table is full, with the fd closed since nothing can name it. */
    VkResult           remembered(VkDevice device, VkResult answered, int *fd);
    /* Whether something made on `device` with the chain at `next` gets
     * `types` added as a structure of type `external`: only on the device
     * imports go to, made able to import, with types the driver allows, and a
     * chain that does not already say for itself. */
    bool backs_with_source(VkDevice device, const void *next, VkStructureType external,
                           VkExternalMemoryHandleTypeFlags types) const;
    /* `create` as on_vkCreateBuffer makes it, `external` added to its chain
     * where the memory source's memory may back it. */
    void for_source(VkDevice device, VkBufferCreateInfo &create, VkExternalMemoryBufferCreateInfo &external);
    /* The same for an image, and whether the image as made names the
     * source's handle type, so that the source's memory may be bound to it. */
    bool for_source(VkDevice device, VkImageCreateInfo &create, VkExternalMemoryImageCreateInfo &external);
    /* The guest's view of the GPU's memory types, learnt once: the memory
     * source's, which is what the guest's requests to it are translated by;
     * `physical`'s own, with nothing host visible, where there is no source
     * or it has no types to show. */
    void learn_types(VkPhysicalDevice physical);
    /* An image's memoryTypeBits, the driver's, as the guest is answered them. */
    uint32_t image_bits(uint32_t host_bits, bool external) const;
    /* The lowest memory type in `mask` this device has, or max_memory_types
     * when the mask is empty: a shared descriptor carries no type of its own,
     * unlike a blob, whose guest asked for one. */
    uint32_t                           first_memory_type(uint32_t mask) const;
    /* The bound GPU's physical device on `instance`, or VK_NULL_HANDLE. */
    VkPhysicalDevice bound_device(VkInstance instance);

    /* VK_API_VERSION_1_0, spelled out: the macro is full of C casts this
     * build refuses. Nothing below it is a version at all. */
    static constexpr uint32_t lowest_api_version = 1u << 22;

    /* VkPhysicalDeviceMemoryProperties::memoryTypeCount's ceiling, which is
     * also how many bits a memory-type mask can have. */
    static constexpr uint32_t max_memory_types = 32u;

    const HostGpu     *gpu_;
    HostMemorySource  *source_;
    PhysicalDeviceList listed_;
    /* The names the create being run is made with. One list serves instances
     * and devices: a create is one command, and nothing reads it afterwards. */
    ExtensionNames     extensions_;
    /* The application an instance being made names, at the host's version. */
    VkApplicationInfo  application_;
    /* The device the imports belong to. A VkDeviceMemory is a device's, and
     * an import arrives with no command to name one, so this is the device
     * the guest made last; every device it made is in handles_, and goes
     * with the context. */
    VkDevice         device_;
    VkPhysicalDevice physical_;
    /* The device imports go to was made able to import a dma-buf. */
    bool             memory_import_;
    /* Every handle this context made and nothing else, each with what it was
     * made through - the null handle or one never made for nothing - and, a
     * template's, the bytes one update through it reads. A handle crosses the
     * wire as a number, and the host answers one only if it made it: the
     * generated decoder used to hand any number to the driver as a pointer
     * (found by tests/fuzz/fuzz_executor_vulkan.cpp on its first run, in
     * vkDestroyInstance). */
    HostObjectRegistry<uint64_t, VulkanObjectKind, uint64_t> handles_;
    /* The session's imports: a VkDeviceMemory one names is the import's to
     * release, not the guest's free. Null between sessions. */
    ImportTable     *imports_;
    VulkanMemoryImport import_;
    GuestMemoryTypes types_;
    bool             types_learnt_;
    /* AV_VK_SUBMIT_TIMING: the GPU's time in and between submissions. */
    VulkanSubmitTiming timing_;
    uint64_t parent_;
    uint64_t ran_;
};

}  // namespace av

#endif  // AV_VULKAN_BACKEND_HPP
