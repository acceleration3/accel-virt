#include "harness/executor/vulkan_backend_probe.hpp"

namespace av {

VulkanBackendProbe::VulkanBackendProbe(VulkanBackend &target) : backend(target) {}

bool VulkanBackendProbe::run_through(vkproto::ServerDispatch &driver, uint32_t command_id,
                                     WireReader &payload, WireWriter &reply, WireArena &arena)
{ return backend.run_through(driver, command_id, payload, reply, arena); }

const HandleRegistry &VulkanBackendProbe::handles() const { return backend.handles_; }

uint32_t VulkanBackendProbe::imports_held() const { return backend.import_.held(); }

}  // namespace av
