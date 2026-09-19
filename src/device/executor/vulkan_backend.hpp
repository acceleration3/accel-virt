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

#include "device/executor/adopted_blobs.hpp"
#include "device/executor/extension_list.hpp"
#include "device/executor/handle_registry.hpp"
#include "device/executor/host_exported_fds.hpp"
#include "device/executor/host_template_table.hpp"
#include "device/executor/importable_types.hpp"
#include "device/executor/session_backend.hpp"
#include "device/shim/physical_device_list.hpp"
#include "device/shim/vulkan_host_import.hpp"
#include "proto/generated/vulkan/vk_real_dispatch.h"

#include <stdint.h>

namespace av {

class HostGpu;
class ImportedBlob;

class VulkanBackend final : public vkproto::RealVulkanDispatch, public SessionBackend {
public:
    VulkanBackend();

    /* Binds this context to the GPU the device is bound to, which must outlive
     * it: the one physical device an enumeration answers. */
    void initialize(const HostGpu &gpu);

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;

    /* The two commands whose results this backend keeps: the instance and the
     * device a dying context still owns, so shut_down can destroy them. */
    VkResult on_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkInstance *pInstance) override;
    void     on_vkDestroyInstance(VkInstance instance,
                                  const VkAllocationCallbacks *pAllocator) override;

    /* The descriptor update templates this context made, because the length of
     * a blob a WithTemplate command carries is the template's and the guest's
     * claim about it is checked against this (feature 047). */
    VkResult on_vkCreateDescriptorUpdateTemplate(VkDevice device,
                                                 const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate) override;
    void     on_vkDestroyDescriptorUpdateTemplate(VkDevice device,
                                                  VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                                  const VkAllocationCallbacks *pAllocator) override;
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

    /* The driver's memory types, host visible only where a host pointer
     * imports (feature 045, T033): the guest maps nothing but its own RAM. */
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
    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;

    /* One host object shared with every other context of this device (feature
     * 053). Vulkan is the only API here that exports anything, so this is the
     * only backend that overrides these: memory as the handle type this GPU
     * imports, a fence as an opaque-fd timeline semaphore. A wire handle the
     * guest names that this backend never made is refused, since the guest
     * would otherwise name any number and have the driver read it. */
    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     int &fd) override;
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     uint64_t &wire_handle) override;
    void               release_object(uint32_t kind, uint64_t wire_handle) override;

    /* Destroys whatever the guest still owned - the instance it made, and the
     * device with it - and forgets every handle. A context's host objects are
     * the context's, and a guest that never destroys them must not be able to
     * leak them by dying (feature 042). */
    void     shut_down();

    /* Commands run since this backend was made, which is what a conformance
     * run counts. */
    uint64_t ran() const;
    /* Records a handle this backend is about to hand the guest, made through
     * whatever the command being decoded named with made_through. A registry
     * with no room left loses the device rather than hand out one it would
     * refuse when the guest sent it back. */
    void     made(uint64_t raw, HandleKind kind = HandleKind::other);
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
    bool     knows_handle(uint64_t raw, HandleKind kind) const;
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
    void enable_identity(VkInstanceCreateInfo &create);
    /* Adds VK_KHR_external_memory and VK_EXT_external_memory_host to the
     * extensions a device is made with, unless the guest named them or its
     * list has no room. */
    void enable_host_import(VkDeviceCreateInfo &create);

    HostTemplateTable templates_;
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
     * `host_types` added as a structure of type `external`: only on the
     * device imports go to, made able to import, with types the driver
     * allows, and a chain that does not already say for itself. */
    bool backs_with_host(VkDevice device, const void *next, VkStructureType external,
                         VkExternalMemoryHandleTypeFlags host_types) const;
    /* Whether the driver imports the blob's pointer into the type it asks
     * for on the guest's device. */
    bool importable(const ImportedBlob &blob);
    /* The blob as device memory of the guest's device, with `memory` what it
     * became: refused for a type the driver will not import that handle into,
     * and for a handle it will not take. */
    [[nodiscard]] bool adopt_dmabuf(const ImportedBlob &blob, uint64_t &memory);
    [[nodiscard]] bool adopt_pointer(const ImportedBlob &blob, uint64_t &memory);
    /* Narrows `properties` of `physical` to what imports, learning the mask
     * the first time `physical` is asked about. */
    void restrict_to_importable(VkPhysicalDevice physical, VkPhysicalDeviceMemoryProperties &properties);
    /* Measures what `physical` imports, once: the kind and the memory types
     * it takes that kind into are decided together, because a guest shown the
     * types of one kind and handed memory of the other would bind neither.
     * Nothing where it is already known. */
    void             decide_import(VkPhysicalDevice physical);
    /* The handle type the decided kind is imported from, which is what a
     * buffer or an image must be made with to be bound to it. */
    VkExternalMemoryHandleTypeFlagBits external_type(VkPhysicalDevice physical);
    /* The lowest memory type in `mask` this device has, or max_memory_types
     * when the mask is empty: a shared descriptor carries no type of its own,
     * unlike a blob, whose guest asked for one. */
    uint32_t                           first_memory_type(uint32_t mask) const;
    /* The bound GPU's physical device on `instance`, or VK_NULL_HANDLE. */
    VkPhysicalDevice bound_device(VkInstance instance);

    /* VK_API_VERSION_1_0, spelled out: the macro is full of C casts this
     * build refuses. Nothing below it is a version at all. */
    static constexpr uint32_t lowest_api_version = 1u << 22;

    /* Every Vulkan command id has this in its top 16 bits; the device filters
     * by API before calling, so anything else is the device disagreeing with
     * itself rather than a guest doing something. */
    static constexpr uint32_t vulkan_api = 1u;

    /* VkPhysicalDeviceMemoryProperties::memoryTypeCount's ceiling, which is
     * also how many bits a memory-type mask can have. */
    static constexpr uint32_t max_memory_types = 32u;

    /* The device the imports belong to. A VkDeviceMemory is a device's, and
     * the guest makes exactly one per context through vkCreateDevice, so this
     * is taken from the command that made it rather than guessed. */
    const HostGpu     *gpu_;
    PhysicalDeviceList listed_;
    /* The names the create being run is made with. One list serves instances
     * and devices: a create is one command, and nothing reads it afterwards. */
    ExtensionList      extensions_;
    VkInstance     instance_;
    VkDevice         device_;
    VkPhysicalDevice physical_;
    /* The device imports go to was made with VK_EXT_external_memory_host. */
    bool             host_import_;
    HandleRegistry   handles_;
    AdoptedBlobs     adopted_;
    VulkanHostImport import_;
    ImportableTypes  importable_;
    uint64_t parent_;
    uint64_t ran_;
    bool     lost_;
};

}  // namespace av

#endif  // AV_VULKAN_BACKEND_HPP
