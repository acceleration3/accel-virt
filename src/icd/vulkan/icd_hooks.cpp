/* The hooks the generated Vulkan ICD wrappers call by name (063
 * contracts/generated_code.md §F), and the ICD's strong handle_to_wire for the
 * five dispatchable types, which overrides the generated weak identity so a
 * wrapped handle crosses the wire as the host's own. Each is one line into the
 * bound root, which is the only state here (icd_binding.hpp).
 *
 * The strong definitions are why this file is not in av_core: the executor
 * library links av_core and defines its own. They sit beside hooks every
 * wrapper references, so the object is always pulled into a link that uses the
 * wrappers, and the weak defaults never win there. */
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/icd_connection.hpp"
#include "proto/icd_handle_registry.hpp"

namespace av::icd {

constinit IcdBinding icd_binding;

using Kind = LoaderHandle::Kind;

Connection *acquire_default_connection() { return icd_binding.root().acquire_connection(); }

void release_connection(Connection *connection) { icd_binding.root().release_connection(connection); }

template <>
VkInstance wrap_server_handle<VkInstance>(uint64_t host, Connection *connection)
{ return static_cast<VkInstance>(icd_binding.root().wrap(Kind::instance, host, connection)); }

template <>
VkPhysicalDevice wrap_server_handle<VkPhysicalDevice>(uint64_t host, Connection *connection)
{ return static_cast<VkPhysicalDevice>(icd_binding.root().wrap(Kind::physical_device, host, connection)); }

template <>
VkDevice wrap_server_handle<VkDevice>(uint64_t host, Connection *connection)
{ return static_cast<VkDevice>(icd_binding.root().wrap(Kind::device, host, connection)); }

template <>
VkQueue wrap_server_handle<VkQueue>(uint64_t host, Connection *connection)
{ return static_cast<VkQueue>(icd_binding.root().wrap(Kind::queue, host, connection)); }

template <>
VkCommandBuffer wrap_server_handle<VkCommandBuffer>(uint64_t host, Connection *connection)
{ return static_cast<VkCommandBuffer>(icd_binding.root().wrap(Kind::command_buffer, host, connection)); }

Connection *connection_of(VkInstance instance) { return icd_binding.root().connection_of(instance, Kind::instance); }

Connection *connection_of(VkPhysicalDevice physical_device)
{ return icd_binding.root().connection_of(physical_device, Kind::physical_device); }

Connection *connection_of(VkDevice device) { return icd_binding.root().connection_of(device, Kind::device); }

Connection *connection_of(VkQueue queue) { return icd_binding.root().connection_of(queue, Kind::queue); }

Connection *connection_of(VkCommandBuffer command_buffer)
{ return icd_binding.root().connection_of(command_buffer, Kind::command_buffer); }

void destroy_wrapper(VkInstance instance) { icd_binding.root().destroy_wrapper(instance, Kind::instance); }

ClientTransport &record_target(VkCommandBuffer command_buffer) { return icd_binding.root().record_target(command_buffer); }

}  // namespace av::icd

namespace av::vkproto {

uint64_t handle_to_wire(VkInstance v) noexcept { return icd::icd_binding.root().wire(v, icd::Kind::instance); }

uint64_t handle_to_wire(VkPhysicalDevice v) noexcept
{ return icd::icd_binding.root().wire(v, icd::Kind::physical_device); }

uint64_t handle_to_wire(VkDevice v) noexcept { return icd::icd_binding.root().wire(v, icd::Kind::device); }

uint64_t handle_to_wire(VkQueue v) noexcept { return icd::icd_binding.root().wire(v, icd::Kind::queue); }

uint64_t handle_to_wire(VkCommandBuffer v) noexcept
{ return icd::icd_binding.root().wire(v, icd::Kind::command_buffer); }

}  // namespace av::vkproto
