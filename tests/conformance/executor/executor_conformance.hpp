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

    [[nodiscard]] bool use_gpu(const char *hex);

    bool run(const char *path);

    std::uint32_t findings() const;
    const char   *finding(std::uint32_t index) const;
    std::uint32_t checked() const;
    std::uint32_t skipped() const;
    const char   *skip(std::uint32_t index) const;

private:
    void note(const char *finding);
    void skipped_because(const char *reason);
    void require(bool held, const char *finding);

    void check_table();
    void check_sub_apis();
    void check_create_and_destroy();
    void check_execute(AvExecutor *executor, std::uint32_t api);
    void check_reply_capacity(AvExecutor *executor, std::uint32_t api);
    void check_options();
    AvContextOptions in_domain(const AvContextOptions &options) const;
    void check_workload(AvExecutor *executor, uint32_t sub_api);

    void check_memory();
    void check_memory_refusals();
    void check_request_classes();
    void check_keys_per_domain();
    void check_memory_channel(AvExecutor *executor, std::uint32_t sub_api);
    void check_foreign_key(AvExecutor *executor, HostMemoryRequest request);
    void check_other_executor(AvExecutor *executor, std::uint32_t sub_api, std::uint64_t blob,
                              std::uint64_t handle);
    void check_imported_handle(AvExecutor *executor);
    void check_vulkan_types(const VkPhysicalDeviceMemoryProperties &properties);
    void check_gpu_round_trip(AvExecutor *executor, ExecutorTransport &wire, std::uint64_t device,
                              std::uint64_t buffer, std::uint32_t type);

    std::uint32_t allocate(std::uint32_t domain, std::uint64_t key, std::uint64_t size, HostMemoryRequest request,
                           AvHostMemory &memory);
    bool check_usable(const AvHostMemory &memory, std::uint64_t size);
    void let_go(std::uint32_t domain, std::uint64_t key, AvHostMemory &memory);
    std::uint32_t second_domain(const char *unchecked);

    bool          make_device(ExecutorTransport &wire, std::uint64_t &instance, std::uint64_t &physical,
                              std::uint64_t &device);
    std::uint64_t make_buffer(ExecutorTransport &wire, std::uint64_t device);
    template <typename Handle>
    Handle as(std::uint64_t handle) const
    {
        return reinterpret_cast<Handle>(handle);
    }

    bool defined_result(std::uint32_t result) const;
    std::uint32_t impossible_id(std::uint32_t api) const;

    static constexpr std::uint64_t page_bytes = 4096u;
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

    std::uint8_t          gpu_[16];
    bool                  real_gpu_;
    std::uint32_t         domain_;
    void                 *library_;
    const AvExecutorOps *ops_;
    const char           *findings_[max_findings];
    const char           *skip_reasons_[max_findings];
    std::uint32_t         found_;
    std::uint32_t         skips_;
    std::uint32_t         checked_;
    std::uint32_t         worked_;
};

}

#endif
