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

    VulkanBackend &backend;
};

}

#endif
