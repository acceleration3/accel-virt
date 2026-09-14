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

#include "device/executor/handle_registry.hpp"
#include "device/executor/session_backend.hpp"
#include "device/shim/vulkan_host_import.hpp"
#include "proto/generated/vulkan/vk_real_dispatch.h"

#include <stdint.h>

namespace av {

class ImportedBlob;

class VulkanBackend final : public vkproto::RealVulkanDispatch, public SessionBackend {
public:
    VulkanBackend();

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;

    /* The two commands this backend interposes rather than forwards
     * untouched: an imported blob has to become memory of some device, and
     * nothing else says which. */
    /* The two commands whose results this backend keeps: the instance and the
     * device a dying context still owns, so shut_down can destroy them. */
    VkResult on_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkInstance *pInstance) override;
    void     on_vkDestroyInstance(VkInstance instance,
                                  const VkAllocationCallbacks *pAllocator) override;

    VkResult on_vkCreateDevice(VkPhysicalDevice physicalDevice,
                               const VkDeviceCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) override;
    void     on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator) override;

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

    /* Every Vulkan command id has this in its top 16 bits; the device filters
     * by API before calling, so anything else is the device disagreeing with
     * itself rather than a guest doing something. */
    static constexpr uint32_t vulkan_api = 1u;

    /* The device the imports belong to. A VkDeviceMemory is a device's, and
     * the guest makes exactly one per context through vkCreateDevice, so this
     * is taken from the command that made it rather than guessed. */
    VkInstance     instance_;
    VkDevice         device_;
    HandleRegistry   handles_;
    VulkanHostImport import_;
    uint64_t parent_;
    uint64_t ran_;
    bool     lost_;
};

}  // namespace av

#endif  // AV_VULKAN_BACKEND_HPP
