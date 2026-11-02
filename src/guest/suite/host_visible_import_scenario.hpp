#ifndef AV_HOST_VISIBLE_IMPORT_SCENARIO_HPP
#define AV_HOST_VISIBLE_IMPORT_SCENARIO_HPP

#include "core/support/status.hpp"
#include "guest/suite/guest_pattern.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "guest/suite/vulkan_device_chain.hpp"
#include "guest/transport/blob_import.hpp"
#include "guest/transport/host_window_chunk.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class HostVisibleImportScenario {
public:
    static constexpr uint64_t chunk_bytes = uint64_t{64} << 20u;
    static constexpr uint32_t copy_bytes = uint32_t{1} << 20u;
    static constexpr uint64_t fence_slice_ns = 500000000u;
    static constexpr uint32_t fence_slices = 10u;

    HostVisibleImportScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);
    HostVisibleImportScenario(const HostVisibleImportScenario &) = delete;
    HostVisibleImportScenario &operator=(const HostVisibleImportScenario &) = delete;

    void run();

private:
    static constexpr uint32_t source = 0u;
    static constexpr uint32_t destination = 1u;
    static constexpr uint32_t sides = 2u;

    [[nodiscard]] Status read_memory_types();
    [[nodiscard]] Status make_buffer(uint32_t side);
    [[nodiscard]] Status import_chunk(uint32_t side);
    [[nodiscard]] Status copy();
    [[nodiscard]] Status record_copy();
    [[nodiscard]] Status submit_copy();
    [[nodiscard]] Status judge_copy(uint32_t filled_before);
    [[nodiscard]] Status destroy();

    ScenarioSteps                    steps_;
    ScenarioTransport                link_;
    VulkanDeviceChain                chain_;
    GuestPattern                     pattern_;
    HostWindowChunk                  chunks_[sides];
    BlobImport                       imports_[sides];
    VkBuffer                         buffers_[sides];
    VkPhysicalDeviceMemoryProperties memory_types_;
    VkCommandPool                    pool_;
    uint64_t                         command_buffer_;
    VkFence                          fence_;
};

}

#endif
