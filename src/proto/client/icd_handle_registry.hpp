/* What the generated Vulkan ICD wrappers ask of the handle registry (063
 * contracts/generated_code.md §F), for the five dispatchable handle types. The
 * definitions are in src/icd/vulkan/icd_hooks.cpp, each one line into the bound
 * IcdRoot; the unwrap in the other direction is the ICD's strong handle_to_wire
 * in the same file. */
#ifndef AV_PROTO_ICD_HANDLE_REGISTRY_HPP
#define AV_PROTO_ICD_HANDLE_REGISTRY_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class ClientTransport;
}

namespace av::icd {

class Connection;

/* The handle for the host's object on `connection`: the one already handed out
 * for it, a new one, or null when none can be made. Only the five
 * specializations exist. */
template <typename H>
H wrap_server_handle(uint64_t host, Connection *connection);
template <>
VkInstance wrap_server_handle<VkInstance>(uint64_t host, Connection *connection);
template <>
VkPhysicalDevice wrap_server_handle<VkPhysicalDevice>(uint64_t host, Connection *connection);
template <>
VkDevice wrap_server_handle<VkDevice>(uint64_t host, Connection *connection);
template <>
VkQueue wrap_server_handle<VkQueue>(uint64_t host, Connection *connection);
template <>
VkCommandBuffer wrap_server_handle<VkCommandBuffer>(uint64_t host, Connection *connection);

/* The connection a handle this ICD made sends through; null for any other. */
Connection *connection_of(VkInstance instance);
Connection *connection_of(VkPhysicalDevice physical_device);
Connection *connection_of(VkDevice device);
Connection *connection_of(VkQueue queue);
Connection *connection_of(VkCommandBuffer command_buffer);

/* The instance's handle and everything made through it. */
void destroy_wrapper(VkInstance instance);

/* Where a command buffer's commands go: its own CommandBatch, which ships them
 * when the command buffer ends. */
ClientTransport &record_target(VkCommandBuffer command_buffer);

}  // namespace av::icd

#endif  // AV_PROTO_ICD_HANDLE_REGISTRY_HPP
