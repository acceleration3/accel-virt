/* Reaches into a VulkanBackend so a test can decode into a driver with no GPU
 * behind it while the backend decides the handles, and read the registry that
 * decides them. Declared a friend by VulkanBackend; nothing outside the tests
 * uses it. */
#ifndef AV_VULKAN_BACKEND_PROBE_HPP
#define AV_VULKAN_BACKEND_PROBE_HPP

#include "device/executor/vulkan_backend.hpp"

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
    /* Host-pointer imports the driver made and was not yet asked to free. */
    uint32_t              imports_held() const;
    /* A slot in the backend's template table, taken as a create does: what
     * fills it in a test without asking the driver for 512 templates
     * (feature 047). */
    [[nodiscard]] Status  reserve_template(VkDevice device, const VkDescriptorUpdateTemplateEntry *entry);
    /* The reserved slot's handle, as a create does once the driver answered. */
    static void           name_template(VulkanBackend &target, VkDescriptorUpdateTemplate made);
    /* Whether the backend still holds `fd` as one it exported: what says
     * whether an import took it (feature 047, US3). */
    bool                  holds_fd(int fd) const;

    VulkanBackend &backend;
};

}  // namespace av

#endif  // AV_VULKAN_BACKEND_PROBE_HPP
