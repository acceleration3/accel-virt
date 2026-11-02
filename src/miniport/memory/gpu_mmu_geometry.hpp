#ifndef AV_GPU_MMU_GEOMETRY_HPP
#define AV_GPU_MMU_GEOMETRY_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class AdapterInfoQuery;

class GpuMmuGeometry {
public:
    static constexpr uint32_t level_count = 4u;
    static constexpr uint32_t entries_per_table = 512u;
    static constexpr uint32_t entry_bytes = 8u;

    explicit GpuMmuGeometry(Platform &platform);

    [[nodiscard]] Status report_caps(const AdapterInfoQuery &query) const;
    [[nodiscard]] Status report_level(const AdapterInfoQuery &query) const;
    uint64_t             root_table_bytes(uint32_t entries) const;
    bool                 root_reaches(uint64_t address, uint32_t root_entries) const;

private:
    enum : uint32_t {
        cache_coherent_memory_supported = 0x10u,
        system_memory_segment = 0u,
        update_through_cpu = 0u,
    };
    static constexpr uint32_t physical_adapter_index_bytes = 4u;
    static constexpr uint32_t page_shift = 12u;

    Platform &platform_;
    uint32_t  virtual_address_bits_;
    uint32_t  index_bits_;
};

}

#endif
