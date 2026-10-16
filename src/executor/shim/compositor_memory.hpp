/* A host GPU's memory as its compositing context allocates it
 * (docs/host-memory.md): exportable VkDeviceMemory on the CompositorDevice,
 * exported once as a dma-buf and mapped through vkMapMemory. The real
 * MemoryExporter. Translation only. */
#ifndef AV_COMPOSITOR_MEMORY_HPP
#define AV_COMPOSITOR_MEMORY_HPP

#include "executor/memory_exporter.hpp"

#include <stdint.h>

namespace av {

class CompositorDevice;
class ExportedMemory;

class CompositorMemory final : public MemoryExporter {
public:
    explicit CompositorMemory(CompositorDevice &device);

    void     memory_properties(VkPhysicalDeviceMemoryProperties &answer) override;
    /* Proved once per type: a page of it allocated, exported, and mapped
     * through the export, which is what QEMU does with every allocation. A
     * driver can claim a type exports and then refuse it (NVIDIA's dma-buf
     * of host-visible memory), so the claim alone is not taken. */
    bool     exportable(uint32_t type) override;
    /* Proved as exportable is, with the driver asked which types it would
     * import the page's dma-buf as. */
    bool     imports_host_visible(uint32_t type) override;
    [[nodiscard]] Status allocate(uint32_t type, uint64_t size, ExportedMemory &memory) override;
    void     free(const ExportedMemory &memory) override;

private:
    static constexpr uint64_t probe_bytes = 0x10000u;

    CompositorDevice &device_;
};

}  // namespace av

#endif  // AV_COMPOSITOR_MEMORY_HPP
