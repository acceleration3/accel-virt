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
    uint32_t              imports_held() const;
    [[nodiscard]] Status  reserve_template(VkDevice device, const VkDescriptorUpdateTemplateEntry *entry);
    static void           name_template(VulkanBackend &target, VkDescriptorUpdateTemplate made);
    void                    learn_types(VkPhysicalDevice physical);
    const GuestMemoryTypes &types() const;
    void                    show_types(const VkPhysicalDeviceMemoryProperties &host, uint32_t backed);
    bool                    memory_import() const;
    bool                  holds_fd(int fd) const;
    VkResult              export_answered(VkResult answered, int &fd);

    VulkanBackend &backend;
};

}

#endif
