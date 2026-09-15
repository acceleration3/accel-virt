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

    /* A device is made able to import a host pointer whether or not the guest
     * asked, because every host-visible allocation the guest makes arrives as
     * one (feature 045); a physical device that cannot is made the device the
     * guest asked for. */
    VkResult on_vkCreateDevice(VkPhysicalDevice physicalDevice,
                               const VkDeviceCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) override;
    void     on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator) override;

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

    /* Imports a host pointer into the guest's device as the memory type the
     * blob asks for - refused for a type the driver will not import that
     * pointer into - and makes the memory a handle the guest may name. */
    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;

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
    void     made(uint64_t raw);
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
    /* Whether something made on `device` with the chain at `next` gets
     * `host_types` added as a structure of type `external`: only on the
     * device imports go to, made able to import, with types the driver
     * allows, and a chain that does not already say for itself. */
    bool backs_with_host(VkDevice device, const void *next, VkStructureType external,
                         VkExternalMemoryHandleTypeFlags host_types) const;
    /* Whether the driver imports the blob's pointer into the type it asks
     * for on the guest's device. */
    bool importable(const ImportedBlob &blob);
    /* The bound GPU's physical device on `instance`, or VK_NULL_HANDLE. */
    VkPhysicalDevice bound_device(VkInstance instance);

    /* Every Vulkan command id has this in its top 16 bits; the device filters
     * by API before calling, so anything else is the device disagreeing with
     * itself rather than a guest doing something. */
    static constexpr uint32_t vulkan_api = 1u;

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
    uint64_t parent_;
    uint64_t ran_;
    bool     lost_;
};

}  // namespace av

#endif  // AV_VULKAN_BACKEND_HPP
