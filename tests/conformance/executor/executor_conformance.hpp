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
    /* SC-002: something each API can actually be asked to do, rather than only
     * commands it refuses. A library that answers every malformed frame
     * correctly and runs nothing is not one the device can use. */
    void check_workload(RvkExecutor *executor, uint32_t sub_api);

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
};

}  // namespace av

#endif  // AV_EXECUTOR_CONFORMANCE_HPP
