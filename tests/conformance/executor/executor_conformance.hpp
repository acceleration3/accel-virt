/* Holds an executor library to src/device/abi/rvk_executor.h.
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

#include "device/abi/rvk_executor.h"

#include <cstddef>
#include <cstdint>

namespace av {

class ExecutorConformance {
public:
    static constexpr std::uint32_t max_findings = 64u;
    /* The versions that added the memory channel and the imported handle. The
     * header names only the newest, and a library of each is held to what its
     * own version promised. */
    static constexpr std::uint32_t memory_channel_version = 2u;
    static constexpr std::uint32_t imported_handle_version = 3u;
    static constexpr std::uint32_t options_version = 4u;

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

    /* Each group is one section of contracts/executor_abi.md. */
    void check_table();
    void check_sub_apis();
    void check_create_and_destroy();
    void check_execute(RvkExecutor *executor, std::uint32_t api);
    void check_reply_capacity(RvkExecutor *executor, std::uint32_t api);
    void check_memory_channel(RvkExecutor *executor);
    /* Version 3: an import of real memory answers a handle the executor's own
     * commands accept back. Needs a Vulkan device, so a real GPU. */
    void check_imported_handle(RvkExecutor *executor);
    void check_create_with_options();
    /* SC-002: something each API can actually be asked to do, rather than only
     * commands it refuses. A library that answers every malformed frame
     * correctly and runs nothing is not one the device can use. */
    void check_workload(RvkExecutor *executor, uint32_t sub_api);

    /* A host page, the size every import is a whole number of, and the id the
     * version 3 check imports one under. */
    static constexpr std::uint64_t page_bytes = 4096u;
    static constexpr std::uint64_t imported_blob = 0x45u;

    /* Runs the `length` bytes in frame_ as `command`, answering into reply_. */
    std::uint32_t send(RvkExecutor *executor, std::uint32_t command, std::size_t length);
    /* The VkResult an answer in reply_ starts with, and the handle after it -
     * 0 unless the result was success. */
    std::int32_t  answered_result() const;
    std::uint64_t answered_handle() const;
    /* An instance, its first physical device and a device on it, as a guest
     * makes them. False when the executor made no device. */
    bool make_device(RvkExecutor *executor, std::uint64_t &instance, std::uint64_t &device);
    /* A buffer a host allocation may back, and the memory types it accepts. */
    std::uint64_t make_buffer(RvkExecutor *executor, std::uint64_t device);
    std::uint32_t buffer_types(RvkExecutor *executor, std::uint64_t device, std::uint64_t buffer);
    /* Imports `page` as the first of `types` the executor takes, answering
     * the handle it names that memory by, or 0. */
    std::uint64_t import_page(RvkExecutor *executor, void *page, std::uint32_t types);
    void          destroy_handle(RvkExecutor *executor, std::uint32_t command, std::uint64_t parent,
                                 std::uint64_t handle);

    /* Whether `result` is one of the four the ABI defines. */
    bool defined_result(std::uint32_t result) const;
    /* A command id inside `api`'s range that no profile can carry. */
    std::uint32_t impossible_id(std::uint32_t api) const;


    /* The GPU asked for, and whether it is a real one. */
    std::uint8_t          gpu_[16];
    bool                  real_gpu_;
    void                 *library_;
    const RvkExecutorOps *ops_;
    const char           *findings_[max_findings];
    const char           *skip_reasons_[max_findings];
    std::uint32_t         found_;
    std::uint32_t         skips_;
    std::uint32_t         checked_;
    /* Commands that ran rather than being refused, which is what says the
     * library does something. */
    std::uint32_t         worked_;
    /* The frame being sent and its answer, for the checks that make a device. */
    std::uint8_t          frame_[1024];
    std::uint8_t          reply_[512];
    std::size_t           produced_;
};

}  // namespace av

#endif  // AV_EXECUTOR_CONFORMANCE_HPP
