#include "harness/guest/scripted_host.hpp"

#include "harness/guest/mock_kmt.hpp"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_writer.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::uint64_t memory_size = 8192u;
constexpr std::uint64_t memory_alignment = 4096u;
constexpr std::uint32_t two_physical_devices = 2u;
constexpr std::int32_t  cl_device_not_found = -1;
constexpr std::uint64_t memory_heap_bytes = std::uint64_t{1} << 30u;

}

ScriptedHost::ScriptedHost(MockKmt &kmt) : kmt_(kmt), cut_(uncut), answer_{}, handles_() {}

void ScriptedHost::queue(const WireWriter &writer)
{
    const auto written = static_cast<std::uint32_t>(writer.offset());
    kmt_.queue_answer(answer_, cut_ < written ? cut_ : written);
    cut_ = uncut;
}

void ScriptedHost::cut_next(std::uint32_t bytes) { cut_ = bytes; }

void ScriptedHost::nop() { kmt_.queue_answer(nullptr, 0u); }

void ScriptedHost::d3d_device(std::uint32_t hresult, std::uint32_t level)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod<std::uint32_t>(hresult);
    writer.write_pod<std::uint32_t>(level);
    queue(writer);
}

void ScriptedHost::d3d_finish(std::uint32_t hresult)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod<std::uint32_t>(hresult);
    queue(writer);
}

void ScriptedHost::host_loss(std::uint32_t status)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod<std::uint32_t>(status);
    writer.write_pod<std::uint32_t>(0u);
    queue(writer);
}

void ScriptedHost::transport_run()
{
    for (std::uint32_t wait = 0u; wait < transport_waits; wait++) nop();
}

void ScriptedHost::submit_run()
{
    for (std::uint32_t wait = 0u; wait < submit_waits; wait++) nop();
}

void ScriptedHost::vk_result(std::int32_t result)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod(result);
    queue(writer);
}

void ScriptedHost::vk_handle(std::int32_t result, std::uint64_t handle)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod(result);
    writer.write_pod(handle);
    queue(writer);
}

void ScriptedHost::vk_count(std::int32_t result, std::uint32_t count)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod(result);
    writer.write_pod(count);
    queue(writer);
}

void ScriptedHost::vk_fd(std::int32_t result, int fd)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod(result);
    writer.write_pod(fd);
    queue(writer);
}

void ScriptedHost::vk_external_buffer_properties(std::uint32_t features, std::uint32_t export_from,
                                                 std::uint32_t compatible)
{
    WireWriter                 writer(answer_, sizeof answer_);
    VkExternalBufferProperties properties = {};
    properties.sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES;
    properties.externalMemoryProperties.externalMemoryFeatures = features;
    properties.externalMemoryProperties.exportFromImportedHandleTypes = export_from;
    properties.externalMemoryProperties.compatibleHandleTypes = compatible;
    writer.write_pod<std::int32_t>(0);
    vkproto::serialize_VkExternalBufferProperties(writer, handles_, properties);
    queue(writer);
}

void ScriptedHost::vk_external_semaphore_properties(std::uint32_t features, std::uint32_t export_from,
                                                    std::uint32_t compatible)
{
    WireWriter                    writer(answer_, sizeof answer_);
    VkExternalSemaphoreProperties properties = {};
    properties.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
    properties.externalSemaphoreFeatures = features;
    properties.exportFromImportedHandleTypes = export_from;
    properties.compatibleHandleTypes = compatible;
    writer.write_pod<std::int32_t>(0);
    vkproto::serialize_VkExternalSemaphoreProperties(writer, handles_, properties);
    queue(writer);
}

void ScriptedHost::vk_physical_devices(std::int32_t result, std::uint32_t count)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod(result);
    writer.write_pod(count);
    for (std::uint32_t index = 0u; index < count; index++) writer.write_pod<std::uint64_t>(vk_physical_device + index);
    queue(writer);
}

void ScriptedHost::vk_memory_requirements(std::uint32_t memory_types)
{
    WireWriter           writer(answer_, sizeof answer_);
    VkMemoryRequirements requirements = {};
    requirements.size = memory_size;
    requirements.alignment = memory_alignment;
    requirements.memoryTypeBits = memory_types;
    writer.write_pod<std::int32_t>(0);
    vkproto::serialize_VkMemoryRequirements(writer, requirements);
    queue(writer);
}

void ScriptedHost::vk_memory_properties(std::uint32_t count, std::uint32_t host_visible)
{
    WireWriter                       writer(answer_, sizeof answer_);
    VkPhysicalDeviceMemoryProperties properties = {};
    properties.memoryTypeCount = count;
    for (std::uint32_t type = 0u; type < count && type < VK_MAX_MEMORY_TYPES; type++)
        properties.memoryTypes[type].propertyFlags = (host_visible >> type & 1u) != 0u
                                                         ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                         : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    properties.memoryHeapCount = 1u;
    properties.memoryHeaps[0].size = memory_heap_bytes;
    writer.write_pod<std::int32_t>(0);
    vkproto::serialize_VkPhysicalDeviceMemoryProperties(writer, properties);
    queue(writer);
}

void ScriptedHost::vk_memory_properties_with_usage(std::uint32_t count, std::uint32_t host_visible, std::uint64_t usage)
{
    WireWriter                                writer(answer_, sizeof answer_);
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    budget.heapUsage[0] = usage;
    budget.heapBudget[0] = memory_heap_bytes;
    VkPhysicalDeviceMemoryProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    properties.pNext = &budget;
    properties.memoryProperties.memoryTypeCount = count;
    for (std::uint32_t type = 0u; type < count && type < VK_MAX_MEMORY_TYPES; type++)
        properties.memoryProperties.memoryTypes[type].propertyFlags = (host_visible >> type & 1u) != 0u
                                                                          ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                                          : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    properties.memoryProperties.memoryHeapCount = 1u;
    properties.memoryProperties.memoryHeaps[0].size = memory_heap_bytes;
    writer.write_pod<std::int32_t>(0);
    vkproto::serialize_VkPhysicalDeviceMemoryProperties2(writer, handles_, properties);
    queue(writer);
}

void ScriptedHost::vk_lendable_memory_properties(std::uint32_t count, std::uint32_t lendable)
{
    WireWriter                       writer(answer_, sizeof answer_);
    VkPhysicalDeviceMemoryProperties properties = {};
    properties.memoryTypeCount = count;
    for (std::uint32_t type = 0u; type < count && type < VK_MAX_MEMORY_TYPES; type++)
        properties.memoryTypes[type].propertyFlags =
            (lendable >> type & 1u) != 0u ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                                VK_MEMORY_PROPERTY_HOST_CACHED_BIT
                                          : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    properties.memoryHeapCount = 1u;
    properties.memoryHeaps[0].size = memory_heap_bytes;
    writer.write_pod<std::int32_t>(0);
    vkproto::serialize_VkPhysicalDeviceMemoryProperties(writer, properties);
    queue(writer);
}

void ScriptedHost::gl_string(const char *text)
{
    WireWriter        writer(answer_, sizeof answer_);
    const std::size_t length = std::strlen(text);
    writer.write_varint(1u);
    writer.write_varint(length);
    writer.write(text, length);
    queue(writer);
}

void ScriptedHost::gl_varint(std::uint64_t value)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(value);
    queue(writer);
}

void ScriptedHost::gl_bytes(const std::uint8_t *bytes, std::uint32_t size)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(size);
    writer.write(bytes, size);
    queue(writer);
}

void ScriptedHost::gl_raw(const std::uint8_t *bytes, std::uint32_t size)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write(bytes, size);
    queue(writer);
}

void ScriptedHost::gl_integer(std::int32_t value)
{
    std::uint8_t bytes[sizeof value];
    std::memcpy(bytes, &value, sizeof value);
    gl_bytes(bytes, sizeof bytes);
}

void ScriptedHost::gl_status(std::uint32_t value)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(value);
    queue(writer);
}

void ScriptedHost::gl_resource_property(std::int32_t value)
{
    const std::int32_t written = 1;
    WireWriter         writer(answer_, sizeof answer_);
    writer.write_varint(sizeof written);
    writer.write(&written, sizeof written);
    writer.write_varint(sizeof value);
    writer.write(&value, sizeof value);
    queue(writer);
}

void ScriptedHost::gl_make(const char *renderer, const char *version)
{
    gl_make_profile(renderer, version, 4, 6, 0x2 );
}

void ScriptedHost::gl_make_profile(const char *renderer, const char *version, std::int32_t major,
                                   std::int32_t minor, std::int32_t mask)
{
    arena_import(0x0000600000000077ull, 2u);
    gl_string("av host vendor");
    gl_string(renderer);
    gl_string(version);
    gl_string("4.60");
    gl_integer(3);
    gl_integer(1);
    gl_integer(0);
    gl_integer(major);
    gl_integer(minor);
    gl_integer(mask);
    const bool legacy = mask == 0x2 || (mask == 0 && major > 0 && major < 3);
    if (legacy) gl_integer(0);
}

void ScriptedHost::gl_policy(std::int32_t listed)
{
    gl_integer(listed);
    gl_slices();
}

void ScriptedHost::gl_slices(std::uint32_t supported)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod(supported);
    queue(writer);
}

void ScriptedHost::gl_made(const char *renderer, const char *version)
{
    gl_make(renderer, version);
    gl_slices();
}

void ScriptedHost::gl_window(std::uint32_t framebuffers, std::uint32_t renderbuffers, std::uint32_t first)
{
    std::uint32_t names[5] = {};
    for (std::uint32_t index = 0u; index < 5u; index++) names[index] = first + index;
    gl_bytes(reinterpret_cast<const std::uint8_t *>(names), framebuffers * sizeof names[0]);
    gl_bytes(reinterpret_cast<const std::uint8_t *>(names + framebuffers), sizeof names[0]);
    if (renderbuffers != 0u)
        gl_bytes(reinterpret_cast<const std::uint8_t *>(names + framebuffers + 1u), renderbuffers * sizeof names[0]);
}

void ScriptedHost::arena_import(std::uint64_t memory, std::uint32_t segments)
{
    for (std::uint32_t at = 0u; at < segments; at++) blob_import(0u, memory + at);
}

void ScriptedHost::blob_import(std::uint32_t status, std::uint64_t memory)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_u32(status);
    writer.write_u32(0u);
    writer.write_u64(memory);
    queue(writer);
}

void ScriptedHost::shared_object(std::uint32_t status, std::uint64_t wire_handle)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_u32(status);
    writer.write_u32(0u);
    writer.write_u64(wire_handle);
    queue(writer);
}

void ScriptedHost::vk_queue_families(std::uint32_t count)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod<std::int32_t>(0);
    writer.write_pod(count);
    for (std::uint32_t index = 0u; index < count; index++) {
        VkQueueFamilyProperties family = {};
        family.queueFlags = VK_QUEUE_GRAPHICS_BIT;
        family.queueCount = count;
        vkproto::serialize_VkQueueFamilyProperties(writer, family);
    }
    queue(writer);
}

void ScriptedHost::cl_result(std::int32_t result)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(static_cast<std::int64_t>(result));
    queue(writer);
}

void ScriptedHost::cl_gl_object(std::int32_t result, std::uint32_t type, std::uint32_t name)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(static_cast<std::int64_t>(result));
    writer.write_varint(static_cast<std::uint64_t>(type));
    writer.write_varint(static_cast<std::uint64_t>(name));
    queue(writer);
}

void ScriptedHost::cl_handle(std::uint64_t handle, std::int32_t error)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod(handle);
    writer.write_varint(static_cast<std::int64_t>(error));
    queue(writer);
}

void ScriptedHost::cl_from_binary(std::uint64_t handle, std::int32_t status, std::int32_t error)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod(handle);
    writer.write_varint(static_cast<std::int64_t>(status));
    writer.write_varint(static_cast<std::int64_t>(error));
    queue(writer);
}

void ScriptedHost::cl_count(std::int32_t result, std::uint32_t count)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(static_cast<std::int64_t>(result));
    writer.write_varint(count);
    queue(writer);
}

void ScriptedHost::cl_mapped(std::uint64_t address, std::uint64_t event, std::int32_t error)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod(address);
    writer.write_pod(event);
    writer.write_varint(static_cast<std::int64_t>(error));
    queue(writer);
}

void ScriptedHost::cl_ids(std::int32_t result, const std::uint64_t *ids, std::uint32_t sent, std::uint32_t total)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(static_cast<std::int64_t>(result));
    writer.write_varint(sent);
    for (std::uint32_t index = 0u; index < sent; index++) writer.write_pod(ids[index]);
    writer.write_varint(total);
    queue(writer);
}

void ScriptedHost::cl_formats(std::int32_t result, const void *formats, std::uint32_t sent, std::uint32_t total)
{
    constexpr std::uint32_t format_bytes = 8u;
    WireWriter              writer(answer_, sizeof answer_);
    writer.write_varint(static_cast<std::int64_t>(result));
    if (formats != nullptr) {
        writer.write_varint(sent * format_bytes);
        writer.write(formats, sent * format_bytes);
    }
    writer.write_varint(total);
    queue(writer);
}

void ScriptedHost::cl_bytes(std::int32_t result, const void *values, std::uint32_t count)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(static_cast<std::int64_t>(result));
    writer.write_varint(count);
    writer.write(values, count);
    queue(writer);
}

void ScriptedHost::cl_enqueued(std::int32_t result, std::uint64_t event)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(static_cast<std::int64_t>(result));
    writer.write_pod(event);
    queue(writer);
}

void ScriptedHost::cl_read(std::int32_t result, const float *values, std::uint32_t count)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(static_cast<std::int64_t>(result));
    writer.write_varint(count * sizeof(float));
    writer.write(values, count * sizeof(float));
    queue(writer);
}

void ScriptedHost::cl_kernel_undescribed()
{
    cl_info(-48, nullptr, 0u);
}

void ScriptedHost::cl_info(std::int32_t result, const char *value, std::uint32_t size)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(static_cast<std::int64_t>(result));
    writer.write_varint(size);
    writer.write(value, size);
    writer.write_varint(size);
    queue(writer);
}

void ScriptedHost::vulkan_run()
{
    vk_handle(0, vk_instance);
    vk_count(0, two_physical_devices);
    vk_physical_devices(0, two_physical_devices);
    vk_handle(0, vk_device);
    vk_handle(0, vk_buffer);
    vk_memory_requirements(vk_memory_types);
    vk_handle(0, vk_memory);
    vk_result(0);
    nop();
}

void ScriptedHost::opencl_run_to_read(const float *sum)
{
    const std::uint64_t platforms[] = {cl_cpu_platform, cl_gpu_platform};
    const std::uint64_t device = cl_device;
    cl_ids(0, platforms, 2u, 2u);
    cl_ids(cl_device_not_found, nullptr, 0u, 0u);
    cl_ids(0, &device, 1u, 1u);
    cl_handle(cl_context, 0);
    cl_handle(cl_queue, 0);
    for (std::uint64_t index = 0u; index < 3u; index++) cl_handle(cl_buffer + index, 0);
    cl_handle(cl_program, 0);
    cl_result(0);
    cl_handle(cl_kernel, 0);
    for (std::uint32_t argument = 0u; argument < 3u; argument++) cl_result(0);
    cl_result(0);
    cl_result(0);
    cl_read(0, sum, cl_elements);
}

void ScriptedHost::opencl_releases()
{
    for (std::uint32_t index = 0u; index < cl_releases; index++) cl_result(0);
}

void ScriptedHost::opencl_run()
{
    float sum[cl_elements] = {};
    for (std::uint32_t index = 0u; index < cl_elements; index++) sum[index] = static_cast<float>(3u * index);
    opencl_run_to_read(sum);
    opencl_releases();
}

void ScriptedHost::healthy_context()
{
    nop();
    vk_handle(0, vk_instance);
    nop();
}

void ScriptedHost::host_visible_import_device()
{
    vk_handle(0, vk_instance);
    vk_count(0, two_physical_devices);
    vk_physical_devices(0, two_physical_devices);
    vk_handle(0, vk_device);
    vk_memory_properties(vk_host_memory_types, vk_host_visible_types);
}

void ScriptedHost::host_visible_import_side(std::uint32_t side)
{
    vk_handle(0, vk_buffer + side);
    vk_memory_requirements(vk_importable_types);
    blob_import(0u, vk_memory + side);
    vk_result(0);
}

void ScriptedHost::host_visible_import_copy()
{
    vk_handle(0, vk_command_pool);
    vk_handle(0, vk_command_buffer);
    vk_handle(0, vk_queue);
    vk_handle(0, vk_fence);
    vk_result(0);
}

void ScriptedHost::host_visible_import_releases()
{
    nop();
    nop();
    nop();
}

void ScriptedHost::host_visible_import_run()
{
    host_visible_import_device();
    host_visible_import_side(0u);
    host_visible_import_side(1u);
    host_visible_import_copy();
    host_visible_import_releases();
}

}
