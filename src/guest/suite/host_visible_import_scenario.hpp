/* A guest process's memory as the host GPU's memory (feature 045, research.md
 * §7, the live experiment): two 64 MiB chunks the driver gives a Vulkan
 * context - of its host window, or of guest RAM where the window cannot hold
 * one - each imported with IMPORT_BLOB as a host-visible memory type, back two
 * buffers on the host's device. The guest writes a
 * GuestPattern into the first chunk's view and clears the second; the host
 * copies the first buffer into the second with vkCmdCopyBuffer in a submitted
 * command buffer, its fence waited in slices a TDR never sees; and the guest
 * reads the second chunk's view. So the host GPU has read pages the guest wrote
 * and written pages the guest reads, with no copy in between.
 *
 * Steps: the context's; instance, physical_device, device; memory_types with
 * how many; for each side (src_, dst_) chunk with the resource, pages, buffer,
 * import with the memory type the host took, bind_buffer; then
 * command_pool, command_buffer, record, queue, fence, submit, wait with the
 * slices waited, copied with the bytes that match, guest_saw_host_write with
 * the destination's bytes that were not zero before the copy; release. Every
 * object made is released in reverse whatever failed. */
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
    /* Whole pages at the start of each chunk, as IMPORT_BLOB requires. */
    static constexpr uint32_t copy_bytes = uint32_t{1} << 20u;
    /* No single host wait may come near TdrDelay's two seconds, so the fence is
     * waited this long at a time, at most this many times. */
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
    [[nodiscard]] Status make_chunk(uint32_t side);
    [[nodiscard]] Status make_buffer(uint32_t side);
    [[nodiscard]] Status import_chunk(uint32_t side);
    [[nodiscard]] Status copy();
    [[nodiscard]] Status record_copy();
    [[nodiscard]] Status submit_copy();
    /* The fence waited, then the destination judged against what it held
     * before: the last of the three steps' status. */
    [[nodiscard]] Status judge_copy(uint32_t filled_before);
    /* The first failure of the releases and of the no-op that proves the
     * destroys sent before them ran. */
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

}  // namespace av

#endif  // AV_HOST_VISIBLE_IMPORT_SCENARIO_HPP
