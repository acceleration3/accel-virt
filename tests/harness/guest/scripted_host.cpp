#include "harness/guest/scripted_host.hpp"

#include "harness/guest/mock_kmt.hpp"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

constexpr std::uint64_t memory_size = 8192u;
constexpr std::uint64_t memory_alignment = 4096u;
constexpr std::uint32_t two_physical_devices = 2u;
constexpr std::int32_t  cl_device_not_found = -1;

}  // namespace

ScriptedHost::ScriptedHost(MockKmt &kmt) : kmt_(kmt), cut_(uncut), answer_{} {}

void ScriptedHost::queue(const WireWriter &writer)
{
    const auto written = static_cast<std::uint32_t>(writer.offset());
    kmt_.queue_answer(answer_, cut_ < written ? cut_ : written);
    cut_ = uncut;
}

void ScriptedHost::cut_next(std::uint32_t bytes) { cut_ = bytes; }

void ScriptedHost::nop() { kmt_.queue_answer(nullptr, 0u); }

void ScriptedHost::transport_run()
{
    for (std::uint32_t wait = 0u; wait < transport_waits; wait++) nop();
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

void ScriptedHost::cl_handle(std::uint64_t handle, std::int32_t error)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_pod(handle);
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

void ScriptedHost::cl_read(std::int32_t result, const float *values, std::uint32_t count)
{
    WireWriter writer(answer_, sizeof answer_);
    writer.write_varint(static_cast<std::int64_t>(result));
    writer.write_varint(count * sizeof(float));
    writer.write(values, count * sizeof(float));
    queue(writer);
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

}  // namespace av
