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
    bool     exportable(uint32_t type) override;
    bool     imports_host_visible(uint32_t type) override;
    [[nodiscard]] Status allocate(uint32_t type, uint64_t size, ExportedMemory &memory) override;
    void     free(const ExportedMemory &memory) override;

private:
    static constexpr uint64_t probe_bytes = 0x10000u;

    CompositorDevice &device_;
};

}

#endif
