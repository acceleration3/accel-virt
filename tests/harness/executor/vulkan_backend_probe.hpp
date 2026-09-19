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
    /* The kind the backend decides this device imports, decided afresh, and
     * the memory types it takes that kind into learnt with it. */
    uint32_t              decide_import(VkPhysicalDevice physical);
    /* Decides the host pointer instead of probing, as a host with no udmabuf
     * leaves the backend deciding: the kernel this runs on may make dma-bufs,
     * and the pointer path has to be testable on one that does. */
    void                  import_as_pointer(VkPhysicalDevice physical);
    /* And the dma-buf instead, whether or not this kernel makes one: which
     * kind a GPU decides is the host's business, and both halves of what the
     * backend does with that decision have to be reachable from a test
     * (feature 053). */
    void                  import_as_dmabuf(VkPhysicalDevice physical);
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
