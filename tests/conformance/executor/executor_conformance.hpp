/* Holds an executor library to src/device/abi/av_executor.h.
 *
 * The device must not care which library it loaded, and until now nothing
 * checked that: the whole contract was validated against the echo library's own
 * command ids and environment knobs, so a second implementation had nothing to
 * be held to. This is what FR-001 says this repository owes.
 *
 * It runs against every library, so it may assume nothing a library is allowed
 * to differ about - not a command id, not a GPU, not a sub-API. What it checks
 * is what the header says of all of them. */
#ifndef AV_EXECUTOR_CONFORMANCE_HPP
#define AV_EXECUTOR_CONFORMANCE_HPP

#include "device/abi/av_executor.h"
#include "proto/resources/host_memory_request.hpp"

#include <cstddef>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace av {

class ExecutorTransport;

class ExecutorConformance {
public:
    static constexpr std::uint32_t max_findings = 64u;

    ExecutorConformance();
    ExecutorConformance(const ExecutorConformance &) = delete;
    ExecutorConformance &operator=(const ExecutorConformance &) = delete;

    /* The GPU to ask for, as the 32 hex characters of its Vulkan device UUID.
     * Without one the suite uses a UUID nobody has, which is the right input
     * for "refuse a GPU that is not there" and the wrong one for everything
     * that needs an executor - so a run with no GPU says what it skipped. */
    [[nodiscard]] bool use_gpu(const char *hex);

    /* Loads `path` and runs every statement. False if anything failed; the
     * findings say what. A library that will not load is itself a finding. */
    bool run(const char *path);

    std::uint32_t findings() const;
    const char   *finding(std::uint32_t index) const;
    /* Statements checked, so a run that checks nothing cannot pass quietly. */
    std::uint32_t checked() const;
    /* What was not checked and why - a host with no GPU cannot be made to
     * create an executor, and saying so is better than reporting a pass. */
    std::uint32_t skipped() const;
    const char   *skip(std::uint32_t index) const;

private:
    void note(const char *finding);
    void skipped_because(const char *reason);
    /* Records `finding` unless `held`; either way one more statement checked. */
    void require(bool held, const char *finding);

    /* Each group is one section of the header's contract. */
    void check_table();
    void check_sub_apis();
    void check_create_and_destroy();
    void check_execute(AvExecutor *executor, std::uint32_t api);
    void check_reply_capacity(AvExecutor *executor, std::uint32_t api);
    void check_options();
    /* The options of a context in the suite's domain. */
    AvContextOptions in_domain(const AvContextOptions &options) const;
    /* SC-002: something each API can actually be asked to do, rather than only
     * commands it refuses. A library that answers every malformed frame
     * correctly and runs nothing is not one the device can use. */
    void check_workload(AvExecutor *executor, uint32_t sub_api);

    /* allocate_memory and free_memory, which need no executor: what is
     * refused, that each class answers usable memory or nothing, and that a
     * key is the domain's own. */
    void check_memory();
    void check_memory_refusals();
    void check_request_classes();
    void check_keys_per_domain();
    /* import_blob, release_blob and imported_handle on any executor: what is
     * refused whatever the API, and that an import ends where the contract
     * says it does. */
    void check_memory_channel(AvExecutor *executor, std::uint32_t sub_api);
    /* Memory allocated in another domain under `key`'s number is refused by
     * `executor`, which would take it from its own. */
    void check_foreign_key(AvExecutor *executor, HostMemoryRequest request);
    /* `executor`'s import `blob`, answered as `handle`, is nobody else's: a
     * second executor of `sub_api` names nothing by it and cannot release it. */
    void check_other_executor(AvExecutor *executor, std::uint32_t sub_api, std::uint64_t blob,
                              std::uint64_t handle);
    /* Version 20: memory allocated for one of the GPU's Vulkan types imports
     * into a guest's device as a VkDeviceMemory its own commands use, the
     * same bytes the descriptor maps. Needs a Vulkan device, so a real GPU. */
    void check_imported_handle(AvExecutor *executor);
    /* Every type the guest's device shows host-visible is answered, with the
     * caching its flags allow; a type it has not got, or more than its heaps
     * hold, is refused. */
    void check_vulkan_types(const VkPhysicalDeviceMemoryProperties &properties);
    /* `buffer` bound to an import of `type`, a GPU copy within it, and a fill
     * once the device let go of the key. */
    void check_gpu_round_trip(AvExecutor *executor, ExecutorTransport &wire, std::uint64_t device,
                              std::uint64_t buffer, std::uint32_t type);

    /* allocate_memory, with `memory` reset first: its answer. */
    std::uint32_t allocate(std::uint32_t domain, std::uint64_t key, std::uint64_t size, HostMemoryRequest request,
                           AvHostMemory &memory);
    /* What allocate_memory answered for `size` bytes is memory the device can
     * map: a descriptor at least that long, whole pages, a caching the ABI
     * defines, and two mappings of it that see each other's writes. Closes
     * nothing. */
    bool check_usable(const AvHostMemory &memory, std::uint64_t size);
    /* The answered descriptor closed and the key let go. */
    void let_go(std::uint32_t domain, std::uint64_t key, AvHostMemory &memory);
    /* A second domain on the suite's GPU, or 0 with the reason recorded. */
    std::uint32_t second_domain(const char *unchecked);

    /* An instance, its first physical device and a device on it, as a guest
     * makes them. False when the executor made no device. */
    bool          make_device(ExecutorTransport &wire, std::uint64_t &instance, std::uint64_t &physical,
                              std::uint64_t &device);
    std::uint64_t make_buffer(ExecutorTransport &wire, std::uint64_t device);
    /* The guest's view of a handle the wire answered. */
    template <typename Handle>
    Handle as(std::uint64_t handle) const
    {
        return reinterpret_cast<Handle>(handle);
    }

    /* Whether `result` is one of the four the ABI defines. */
    bool defined_result(std::uint32_t result) const;
    /* A command id inside `api`'s range that no profile can carry. */
    std::uint32_t impossible_id(std::uint32_t api) const;

    /* A host page, the size every allocation is a whole number of. */
    static constexpr std::uint64_t page_bytes = 4096u;
    /* The keys and blob ids the checks use, each its own so that one check's
     * leftovers cannot pass another. The device chooses both; nothing about
     * their values is the library's business. */
    static constexpr std::uint64_t refused_key = 0x7001u;
    static constexpr std::uint64_t class_key = 0x7002u;
    static constexpr std::uint64_t domain_key = 0x7003u;
    static constexpr std::uint64_t channel_key = 0x7004u;
    static constexpr std::uint64_t foreign_key = 0x7005u;
    static constexpr std::uint64_t type_key = 0x7006u;
    static constexpr std::uint64_t imported_key = 0x7007u;
    static constexpr std::uint64_t never_allocated_key = 0x7FFFu;
    static constexpr std::uint64_t channel_blob = 0x41u;
    static constexpr std::uint64_t foreign_blob = 0x42u;
    static constexpr std::uint64_t imported_blob = 0x45u;
    static constexpr std::uint64_t never_imported_blob = 0xFFFFFFFFu;

    /* The GPU asked for, and whether it is a real one. */
    std::uint8_t          gpu_[16];
    bool                  real_gpu_;
    /* The domain the suite asked for on that GPU, as a device would; 0 when
     * the library has no such GPU. */
    std::uint32_t         domain_;
    void                 *library_;
    const AvExecutorOps *ops_;
    const char           *findings_[max_findings];
    const char           *skip_reasons_[max_findings];
    std::uint32_t         found_;
    std::uint32_t         skips_;
    std::uint32_t         checked_;
    /* Commands that ran rather than being refused, which is what says the
     * library does something. */
    std::uint32_t         worked_;
};

}  // namespace av

#endif  // AV_EXECUTOR_CONFORMANCE_HPP
