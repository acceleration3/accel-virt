/* The host behind the rvk scenarios, played over MockKmt: each answer queued
 * for the wait that will find it, encoded as the generated server dispatch
 * writes it - a Vulkan result as an int32 before its outputs, handles as
 * uint64, structures through vk_serialize.h; an OpenCL result or error as a
 * varint, an array as its count and elements. The handles are made-up numbers a
 * test can recognise; the guest only passes them back. */
#ifndef AV_SCRIPTED_HOST_HPP
#define AV_SCRIPTED_HOST_HPP

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
    /* A buffer that can live in memory types 3 and 5: the lowest is 3. */
    static constexpr std::uint32_t vk_memory_types = 0x28u;
    static constexpr std::uint64_t cl_cpu_platform = 0x6100u;
    static constexpr std::uint64_t cl_gpu_platform = 0x6200u;
    static constexpr std::uint64_t cl_device = 0x6300u;
    static constexpr std::uint64_t cl_context = 0x6400u;
    static constexpr std::uint64_t cl_queue = 0x6500u;
    /* The first of three buffers, numbered on from here. */
    static constexpr std::uint64_t cl_buffer = 0x6600u;
    static constexpr std::uint64_t cl_program = 0x6700u;
    static constexpr std::uint64_t cl_kernel = 0x6800u;
    static constexpr std::uint32_t cl_elements = 64u;
    /* Waits a whole successful run of each scenario takes. */
    static constexpr std::uint32_t vulkan_waits = 9u;
    static constexpr std::uint32_t opencl_waits = 24u;
    static constexpr std::uint32_t isolation_waits_before_fault = 3u;

    explicit ScriptedHost(MockKmt &kmt);
    ScriptedHost(const ScriptedHost &) = delete;
    ScriptedHost &operator=(const ScriptedHost &) = delete;

    /* The transport's no-op: nothing. */
    void nop();
    void vk_result(std::int32_t result);
    void vk_handle(std::int32_t result, std::uint64_t handle);
    /* How many physical devices, for the call that asks only that. */
    void vk_count(std::int32_t result, std::uint32_t count);
    /* `count` devices numbered from vk_physical_device. */
    void vk_physical_devices(std::int32_t result, std::uint32_t count);
    void vk_memory_requirements(std::uint32_t memory_types);
    void cl_result(std::int32_t result);
    void cl_handle(std::uint64_t handle, std::int32_t error);
    /* An ID query's answer: the result, the `sent` ids that fit the room the
     * guest gave, and the `total` the host has. */
    void cl_ids(std::int32_t result, const std::uint64_t *ids, std::uint32_t sent, std::uint32_t total);
    /* A blocking read's answer: the result and `count` floats. */
    void cl_read(std::int32_t result, const float *values, std::uint32_t count);

    /* Whole successful runs, every wait from a scenario's first to its last. */
    void vulkan_run();
    /* Through the read, whose floats are `sum`, before the releases. */
    void opencl_run_to_read(const float *sum);
    void opencl_releases();
    void opencl_run();
    /* The healthy context's no-op, instance and no-op. */
    void healthy_context();

private:
    static constexpr std::uint32_t answer_bytes = 512u;
    static constexpr std::uint32_t cl_releases = 7u;

    void queue(const WireWriter &writer);

    MockKmt     &kmt_;
    std::uint8_t answer_[answer_bytes];
};

}  // namespace av

#endif  // AV_SCRIPTED_HOST_HPP
