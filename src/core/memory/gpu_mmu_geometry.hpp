/* The GPU page tables VidMm builds for this adapter: x86-64's shape, four
 * levels of 512 8-byte entries over 48 bits (level 0 the leaf), every table in
 * system memory, updated by the driver through the CPU mapping. The device
 * walks them by guest physical address (docs/protocol.md). */
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

    /* DXGKQAITYPE_GPUMMUCAPS for physical adapter 0. */
    [[nodiscard]] Status report_caps(const AdapterInfoQuery &query) const;
    /* DXGKQAITYPE_PAGETABLELEVELDESC for one level of physical adapter 0. */
    [[nodiscard]] Status report_level(const AdapterInfoQuery &query) const;
    /* DxgkDdiGetRootPageTableSize. */
    uint64_t             root_table_bytes(uint32_t entries) const;
    /* Whether `address` is inside the address space and indexes one of a
     * root table's first `root_entries` entries. */
    bool                 root_reaches(uint64_t address, uint32_t root_entries) const;

private:
    enum : uint32_t {
        cache_coherent_memory_supported = 0x10u,   /* DXGK_GPUMMUCAPS */
        system_memory_segment = 0u,                /* PageTableSegmentId */
        update_through_cpu = 0u,                   /* DXGK_PAGETABLEUPDATE_CPU_VIRTUAL */
    };
    static constexpr uint32_t physical_adapter_index_bytes = 4u;
    static constexpr uint32_t page_shift = 12u;

    Platform &platform_;
    uint32_t  virtual_address_bits_;
    uint32_t  index_bits_;
};

}  // namespace av

#endif  // AV_GPU_MMU_GEOMETRY_HPP
