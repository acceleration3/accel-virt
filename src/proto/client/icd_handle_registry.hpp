#ifndef AV_PROTO_ICD_HANDLE_REGISTRY_HPP
#define AV_PROTO_ICD_HANDLE_REGISTRY_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class ClientTransport;
}

namespace av::vkproto {
class HandleTranslation;
}

namespace av::icd {

class Connection;

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

Connection *connection_of(VkInstance instance);
Connection *connection_of(VkPhysicalDevice physical_device);
Connection *connection_of(VkDevice device);
Connection *connection_of(VkQueue queue);
Connection *connection_of(VkCommandBuffer command_buffer);

void destroy_wrapper(VkInstance instance);

ClientTransport &record_target(VkCommandBuffer command_buffer);

vkproto::HandleTranslation &wire_handles();

}

#endif
