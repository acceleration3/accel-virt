#include "core/trace/function_scope.hpp"
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/client/icd_connection.hpp"
#include "proto/client/icd_handle_registry.hpp"

namespace av::icd {

constinit IcdBinding icd_binding;

using Kind = LoaderHandle::Kind;

Connection *acquire_default_connection()
{
    AV_SCOPE("acquire_default_connection");
    return icd_binding.root().acquire_connection();
}

void release_connection(Connection *connection)
{
    AV_SCOPE("release_connection");
    icd_binding.root().release_connection(connection);
}

template <>
VkInstance wrap_server_handle<VkInstance>(uint64_t host, Connection *connection)
{
    AV_SCOPE("wrap_server_handle");
    return static_cast<VkInstance>(icd_binding.root().wrap(Kind::instance, host, connection));
}

template <>
VkPhysicalDevice wrap_server_handle<VkPhysicalDevice>(uint64_t host, Connection *connection)
{
    AV_SCOPE("wrap_server_handle");
    return static_cast<VkPhysicalDevice>(icd_binding.root().wrap(Kind::physical_device, host, connection));
}

template <>
VkDevice wrap_server_handle<VkDevice>(uint64_t host, Connection *connection)
{
    AV_SCOPE("wrap_server_handle");
    return static_cast<VkDevice>(icd_binding.root().wrap(Kind::device, host, connection));
}

template <>
VkQueue wrap_server_handle<VkQueue>(uint64_t host, Connection *connection)
{
    AV_SCOPE("wrap_server_handle");
    return static_cast<VkQueue>(icd_binding.root().wrap(Kind::queue, host, connection));
}

template <>
VkCommandBuffer wrap_server_handle<VkCommandBuffer>(uint64_t host, Connection *connection)
{
    AV_SCOPE("wrap_server_handle");
    return static_cast<VkCommandBuffer>(icd_binding.root().wrap(Kind::command_buffer, host, connection));
}

Connection *connection_of(VkInstance instance)
{
    AV_SCOPE("connection_of");
    return icd_binding.root().connection_of(instance, Kind::instance);
}

Connection *connection_of(VkPhysicalDevice physical_device)
{
    AV_SCOPE("connection_of");
    return icd_binding.root().connection_of(physical_device, Kind::physical_device);
}

Connection *connection_of(VkDevice device)
{
    AV_SCOPE("connection_of");
    return icd_binding.root().connection_of(device, Kind::device);
}

Connection *connection_of(VkQueue queue)
{
    AV_SCOPE("connection_of");
    return icd_binding.root().connection_of(queue, Kind::queue);
}

Connection *connection_of(VkCommandBuffer command_buffer)
{
    AV_SCOPE("connection_of");
    return icd_binding.root().connection_of(command_buffer, Kind::command_buffer);
}

void destroy_wrapper(VkInstance instance)
{
    AV_SCOPE("destroy_wrapper");
    icd_binding.root().destroy_wrapper(instance, Kind::instance);
}

ClientTransport &record_target(VkCommandBuffer command_buffer)
{
    AV_SCOPE("record_target");
    return icd_binding.root().record_target(command_buffer);
}

vkproto::HandleTranslation &wire_handles()
{
    AV_SCOPE("wire_handles");
    return icd_binding.root().wire_handles();
}

}
