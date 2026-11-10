#ifndef AV_MEMORY_EXPORTER_HPP
#define AV_MEMORY_EXPORTER_HPP

#include "core/support/status.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class ExportedMemory;

class MemoryExporter {
public:
    MemoryExporter(const MemoryExporter &) = delete;
    MemoryExporter &operator=(const MemoryExporter &) = delete;

    virtual void     memory_properties(VkPhysicalDeviceMemoryProperties &answer) = 0;
    virtual bool     exportable(uint32_t type) = 0;
    virtual bool     imports_host_visible(uint32_t type) = 0;
    virtual bool     imports_alike(uint32_t type) = 0;
    [[nodiscard]] virtual Status allocate(uint32_t type, uint64_t size, ExportedMemory &memory) = 0;
    virtual void                 free(const ExportedMemory &memory) = 0;

protected:
    MemoryExporter() = default;
    ~MemoryExporter() = default;
};

}

#endif
