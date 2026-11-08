#ifndef AV_SCRIPTED_HOST_HPP
#define AV_SCRIPTED_HOST_HPP

#include "guest/suite/submit_benchmark.hpp"
#include "proto/generated/vulkan/vk_handle_translation.h"

#include <cstdint>

namespace av {

class MockKmt;
class WireWriter;

class ScriptedHost {
public:
    static constexpr std::uint64_t vk_instance = 0x1100u;
    static constexpr std::uint64_t vk_physical_device = 0x2200u;
    static constexpr std::uint64_t vk_device = 0x3300u;
    static constexpr std::uint64_t vk_buffer = 0x4400u;
    static constexpr std::uint64_t vk_memory = 0x5500u;
    static constexpr std::uint32_t vk_memory_types = 0x28u;
    static constexpr std::uint64_t vk_command_pool = 0x7100u;
    static constexpr std::uint64_t vk_command_buffer = 0x7200u;
    static constexpr std::uint64_t vk_queue = 0x7300u;
    static constexpr std::uint64_t vk_fence = 0x7400u;
    static constexpr std::uint32_t vk_host_memory_types = 4u;
    static constexpr std::uint32_t vk_host_visible_types = 0x6u;
    static constexpr std::uint32_t vk_importable_types = 0x7u;
    static constexpr std::uint64_t cl_cpu_platform = 0x6100u;
    static constexpr std::uint64_t cl_gpu_platform = 0x6200u;
    static constexpr std::uint64_t cl_device = 0x6300u;
    static constexpr std::uint64_t cl_context = 0x6400u;
    static constexpr std::uint64_t cl_queue = 0x6500u;
    static constexpr std::uint64_t cl_buffer = 0x6600u;
    static constexpr std::uint64_t cl_program = 0x6700u;
    static constexpr std::uint64_t cl_kernel = 0x6800u;
    static constexpr std::uint32_t cl_elements = 64u;
    static constexpr std::uint32_t transport_waits = 32u;
    static constexpr std::uint32_t vulkan_waits = 9u;
    static constexpr std::uint32_t opencl_waits = 24u;
    static constexpr std::uint32_t isolation_waits_before_fault = 3u;
    static constexpr std::uint32_t submit_waits = 1u + 2u * SubmitBenchmark::rounds;
    static constexpr std::uint32_t host_visible_import_waits = 21u;
    static constexpr std::uint32_t host_visible_import_copy_wait = 18u;

    explicit ScriptedHost(MockKmt &kmt);
    ScriptedHost(const ScriptedHost &) = delete;
    ScriptedHost &operator=(const ScriptedHost &) = delete;

    void nop();
    void d3d_device(std::uint32_t hresult, std::uint32_t level);
    void d3d_finish(std::uint32_t hresult);
    void host_loss(std::uint32_t status);
    void transport_run();
    void submit_run();
    void vk_result(std::int32_t result);
    void vk_handle(std::int32_t result, std::uint64_t handle);
    void vk_count(std::int32_t result, std::uint32_t count);
    void vk_fd(std::int32_t result, int fd);
    void vk_external_buffer_properties(std::uint32_t features, std::uint32_t export_from,
                                       std::uint32_t compatible);
    void vk_external_semaphore_properties(std::uint32_t features, std::uint32_t export_from,
                                          std::uint32_t compatible);
    void vk_physical_devices(std::int32_t result, std::uint32_t count);
    void vk_memory_requirements(std::uint32_t memory_types);
    void vk_memory_properties(std::uint32_t count, std::uint32_t host_visible);
    void vk_memory_properties_with_usage(std::uint32_t count, std::uint32_t host_visible, std::uint64_t usage);
    void vk_lendable_memory_properties(std::uint32_t count, std::uint32_t lendable);
    void blob_import(std::uint32_t status, std::uint64_t memory);
    void arena_import(std::uint64_t memory, std::uint32_t segments);
    void shared_object(std::uint32_t status, std::uint64_t wire_handle);
    void vk_queue_families(std::uint32_t count);
    void cl_result(std::int32_t result);
    void cl_gl_object(std::int32_t result, std::uint32_t type, std::uint32_t name);
    void cl_handle(std::uint64_t handle, std::int32_t error);
    void cl_from_binary(std::uint64_t handle, std::int32_t status, std::int32_t error);
    void cl_count(std::int32_t result, std::uint32_t count);
    void cl_mapped(std::uint64_t address, std::uint64_t event, std::int32_t error);
    void cl_ids(std::int32_t result, const std::uint64_t *ids, std::uint32_t sent, std::uint32_t total);
    void cl_read(std::int32_t result, const float *values, std::uint32_t count);
    void cl_bytes(std::int32_t result, const void *values, std::uint32_t count);
    void cl_enqueued(std::int32_t result, std::uint64_t event);
    void cl_formats(std::int32_t result, const void *formats, std::uint32_t sent, std::uint32_t total);
    void cl_info(std::int32_t result, const char *value, std::uint32_t size);
    void gl_string(const char *text);
    void gl_varint(std::uint64_t value);
    void gl_bytes(const std::uint8_t *bytes, std::uint32_t size);
    void gl_integer(std::int32_t value);
    void gl_resource_property(std::int32_t value);
    void gl_make(const char *renderer, const char *version);
    void gl_make_profile(const char *renderer, const char *version, std::int32_t major, std::int32_t minor,
                         std::int32_t mask);
    void gl_policy(std::int32_t listed);
    void gl_slices(std::uint32_t supported = 0u);
    void gl_made(const char *renderer, const char *version);
    void gl_window(std::uint32_t framebuffers, std::uint32_t renderbuffers, std::uint32_t first = 100u);
    static constexpr std::uint32_t gl_make_waits = 13u;
    static constexpr std::uint32_t gl_policy_waits = 2u;
    static constexpr std::uint32_t gl_slices_waits = 1u;

    void cut_next(std::uint32_t bytes);

    void vulkan_run();
    void opencl_run_to_read(const float *sum);
    void opencl_releases();
    void opencl_run();
    void healthy_context();
    void host_visible_import_device();
    void host_visible_import_side(std::uint32_t side);
    void host_visible_import_copy();
    void host_visible_import_releases();
    void host_visible_import_run();

private:
    static constexpr std::uint32_t answer_bytes = 1024u;
    static constexpr std::uint32_t cl_releases = 7u;

    static constexpr std::uint32_t uncut = ~std::uint32_t{0};

    void queue(const WireWriter &writer);

    MockKmt      &kmt_;
    std::uint32_t cut_;
    std::uint8_t  answer_[answer_bytes];
    vkproto::HandleIdentity handles_;
};

}

#endif
