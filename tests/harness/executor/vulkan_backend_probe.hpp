/* Reaches into a VulkanBackend so a test can decode into a driver with no GPU
 * behind it while the backend decides the handles, and read the registry that
 * decides them. Declared a friend by VulkanBackend; nothing outside the tests
 * uses it. */
#ifndef AV_VULKAN_BACKEND_PROBE_HPP
#define AV_VULKAN_BACKEND_PROBE_HPP

#include "executor/vulkan/vulkan_backend.hpp"

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class HandleRegistry;
class WireArena;
class WireReader;
class WireWriter;

struct VulkanBackendProbe {
    explicit VulkanBackendProbe(VulkanBackend &target);

    [[nodiscard]] bool run_through(vkproto::ServerDispatch &driver, uint32_t command_id,
                                   WireReader &payload, WireWriter &reply, WireArena &arena);
    const HandleRegistry &handles() const;
    /* Dma-buf imports the driver made and was not yet asked to free. */
    uint32_t              imports_held() const;
    /* A slot in the backend's template table, taken as a create does: what
     * fills it in a test without asking the driver for 512 templates
     * (feature 047). */
    [[nodiscard]] Status  reserve_template(VkDevice device, const VkDescriptorUpdateTemplateEntry *entry);
    /* The reserved slot's handle, as a create does once the driver answered. */
    static void           name_template(VulkanBackend &target, VkDescriptorUpdateTemplate made);
    /* The guest's memory types learnt as the guest's first question about
     * `physical` learns them, and the view learnt. */
    void                    learn_types(VkPhysicalDevice physical);
    const GuestMemoryTypes &types() const;
    /* The guest shown `host` with `backed` as the source's types, whatever
     * the GPU's are: a real driver's types laid out as another GPU's, so a
     * device test translates through a layout this host does not have. */
    void                    show_types(const VkPhysicalDeviceMemoryProperties &host, uint32_t backed);
    /* Whether the device imports go to was made able to import a dma-buf. */
    bool                    memory_import() const;
    /* Whether the backend still holds `fd` as one it exported: what says
     * whether an import took it (feature 047, US3). */
    bool                  holds_fd(int fd) const;
    /* What the backend makes of a driver's answer to an export, `fd` being
     * what the driver wrote: the answer a refusing driver gets, which neither
     * GPU measured gives (feature 047, US3). */
    VkResult              export_answered(VkResult answered, int &fd);

    VulkanBackend &backend;
};

}  // namespace av

#endif  // AV_VULKAN_BACKEND_PROBE_HPP
