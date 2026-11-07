#ifndef AV_VULKAN_BACKEND_PROBE_HPP
#define AV_VULKAN_BACKEND_PROBE_HPP

#include "executor/host_object_registry.hpp"
#include "executor/vulkan/vulkan_backend.hpp"

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class WireArena;
class WireReader;
class WireWriter;

struct VulkanBackendProbe {
    explicit VulkanBackendProbe(VulkanBackend &target);

    [[nodiscard]] bool run_through(vkproto::ServerDispatch &driver, uint32_t command_id,
                                   WireReader &payload, WireWriter &reply, WireArena &arena);
    const HostObjectRegistry<uint64_t, VulkanObjectKind, uint64_t> &handles() const;
    uint32_t              imports_held() const;
    void                    learn_types(VkPhysicalDevice physical);
    const GuestMemoryTypes &types() const;
    void                    show_types(const VkPhysicalDeviceMemoryProperties &host, uint32_t backed);
    bool                    memory_import() const;
    void                    begin_frame(bool recording);
    [[nodiscard]] Status    add_template(uint64_t made, uint64_t device, const VkDescriptorUpdateTemplateEntry *entries,
                                         uint32_t count, const uint32_t *immutable = nullptr,
                                         uint32_t immutable_count = 0u);
    [[nodiscard]] bool      template_named(VkDescriptorUpdateTemplate made, const void *data, uint64_t set,
                                           uint64_t device);
    VulkanGuard            &guard();

    VulkanBackend &backend;
};

}

#endif
