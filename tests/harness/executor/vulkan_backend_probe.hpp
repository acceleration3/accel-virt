/* Reaches into a VulkanBackend so a test can decode into a driver with no GPU
 * behind it while the backend decides the handles, and read the registry that
 * decides them. Declared a friend by VulkanBackend; nothing outside the tests
 * uses it. */
#ifndef AV_VULKAN_BACKEND_PROBE_HPP
#define AV_VULKAN_BACKEND_PROBE_HPP

#include "device/executor/vulkan_backend.hpp"

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

    VulkanBackend &backend;
};

}  // namespace av

#endif  // AV_VULKAN_BACKEND_PROBE_HPP
