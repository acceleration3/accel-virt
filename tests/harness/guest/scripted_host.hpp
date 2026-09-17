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
    static constexpr std::uint64_t vk_command_pool = 0x7100u;
    static constexpr std::uint64_t vk_command_buffer = 0x7200u;
    static constexpr std::uint64_t vk_queue = 0x7300u;
    static constexpr std::uint64_t vk_fence = 0x7400u;
    /* The host's memory types for an import: four, of which 1 and 2 are host
     * visible, and a buffer that can live in 0 to 2 - so the first host-visible
     * type it can live in is 1. The device memory each import answers is
     * vk_memory numbered on by side. */
    static constexpr std::uint32_t vk_host_memory_types = 4u;
    static constexpr std::uint32_t vk_host_visible_types = 0x6u;
    static constexpr std::uint32_t vk_importable_types = 0x7u;
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
    static constexpr std::uint32_t transport_waits = 32u;   /* the first no-op and 31 warm ones */
    static constexpr std::uint32_t vulkan_waits = 9u;
    static constexpr std::uint32_t opencl_waits = 24u;
    static constexpr std::uint32_t isolation_waits_before_fault = 3u;
    /* The import scenario's waits, and the one of them that waits for the fence
     * of the submitted copy - where a test plays the host GPU's copy. */
    static constexpr std::uint32_t host_visible_import_waits = 21u;
    static constexpr std::uint32_t host_visible_import_copy_wait = 18u;

    explicit ScriptedHost(MockKmt &kmt);
    ScriptedHost(const ScriptedHost &) = delete;
    ScriptedHost &operator=(const ScriptedHost &) = delete;

    /* The transport's no-op: nothing. */
    void nop();
    /* A whole transport scenario's no-ops. */
    void transport_run();
    void vk_result(std::int32_t result);
    void vk_handle(std::int32_t result, std::uint64_t handle);
    /* How many physical devices, for the call that asks only that. */
    void vk_count(std::int32_t result, std::uint32_t count);
    /* `count` devices numbered from vk_physical_device. */
    void vk_physical_devices(std::int32_t result, std::uint32_t count);
    void vk_memory_requirements(std::uint32_t memory_types);
    /* `count` memory types, those whose bit is set in `host_visible` host
     * visible, as a void command answers: a zero, then the structure. */
    void vk_memory_properties(std::uint32_t count, std::uint32_t host_visible);
    /* IMPORT_BLOB's answer: the host's status, a reserved word and the device
     * memory. */
    void blob_import(std::uint32_t status, std::uint64_t memory);
    /* `count` queue families, each with `count` graphics queues: a void
     * command's zero, the count, and the structures. */
    void vk_queue_families(std::uint32_t count);
    void cl_result(std::int32_t result);
    void cl_handle(std::uint64_t handle, std::int32_t error);
    /* An ID query's answer: the result, the `sent` ids that fit the room the
     * guest gave, and the `total` the host has. */
    void cl_ids(std::int32_t result, const std::uint64_t *ids, std::uint32_t sent, std::uint32_t total);
    /* A blocking read's answer: the result and `count` floats. */
    void cl_read(std::int32_t result, const float *values, std::uint32_t count);
    /* An info query's answer: the result, `size` bytes of `value`, and the
     * size again as the size it reports. */
    void cl_info(std::int32_t result, const char *value, std::uint32_t size);
    /* An OpenGL string query's answer: present, the length, the bytes. */
    void gl_string(const char *text);
    /* An OpenGL scalar return - an error, a location, an index. */
    void gl_varint(std::uint64_t value);
    /* An OpenGL query's filled buffer: its length, then its bytes. */
    void gl_bytes(const std::uint8_t *bytes, std::uint32_t size);
    /* glGetIntegerv's answer of one value. */
    void gl_integer(std::int32_t value);
    /* A resource property query's answer: one value written, and `value`. */
    void gl_resource_property(std::int32_t value);
    /* Everything a GL context's make waits for: the blob import, the vendor,
     * renderer, version and shading language strings, and the three list
     * lengths. */
    void gl_make(const char *renderer, const char *version);
    /* What the first make-current of a context on a window waits for: the
     * names of its framebuffer objects (two when multisampled), its colour
     * texture and its renderbuffers (a multisampled colour one, then the
     * depth one), numbered from `first`. */
    void gl_window(std::uint32_t framebuffers, std::uint32_t renderbuffers, std::uint32_t first = 100u);
    /* Waits a GL make takes. */
    static constexpr std::uint32_t gl_make_waits = 8u;

    /* The next answer queued stops after its first `bytes`, as a host that
     * wrote a short reply leaves it. */
    void cut_next(std::uint32_t bytes);

    /* Whole successful runs, every wait from a scenario's first to its last. */
    void vulkan_run();
    /* Through the read, whose floats are `sum`, before the releases. */
    void opencl_run_to_read(const float *sum);
    void opencl_releases();
    void opencl_run();
    /* The healthy context's no-op, instance and no-op. */
    void healthy_context();
    /* Through the instance, physical devices, device and memory types; then
     * one side's buffer, requirements, import and bind. */
    void host_visible_import_device();
    void host_visible_import_side(std::uint32_t side);
    /* Pool, command buffer, queue, fence and the fence wait; then both
     * releases and the no-op. */
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
};

}  // namespace av

#endif  // AV_SCRIPTED_HOST_HPP
