/* The GPU MMU half of adapter start for physical adapter 0: the GPUMMUCAPS and
 * PAGETABLELEVELDESC queries and node metadata checks ADAPTER_RENDER::Initialize
 * @ 1401b3d4c makes, and VidMm's InitializeGpuVaState @ 1400972cc over the
 * answers once segments exist. Both run only for an adapter whose physical
 * adapter caps report GpuMmuSupported. */
#ifndef AV_FAKE_GPU_MMU_HPP
#define AV_FAKE_GPU_MMU_HPP

#include "miniport/ddi/dxgk_gpu_mmu_caps_record.hpp"
#include "miniport/ddi/dxgk_page_table_level_desc_record.hpp"
#include "harness/dxgkrnl/fake_dxgadapter.hpp"
#include "harness/vidmm/gpu_mmu_rule.hpp"

#include <cstdint>

namespace av {

class FakeGpuMmu {
public:
    static constexpr std::uint32_t max_levels = 6u;
    static constexpr std::int32_t  status_device_configuration_error = static_cast<std::int32_t>(0xC0000182u);
    static constexpr std::int32_t  status_no_memory = static_cast<std::int32_t>(0xC0000017u);

    explicit FakeGpuMmu(FakeDxgadapter &adapter);

    /* Answers the NTSTATUS adapter start fails with. */
    std::int32_t query(void *context);
    /* Over `segments`, the count VidMm knows including its implicit one. */
    std::int32_t initialize_va_state(std::uint32_t segments);

    bool                                 supported() const;
    GpuMmuRule                           failed_rule() const;
    const DxgkGpuMmuCapsRecord          &caps() const;
    const DxgkPageTableLevelDescRecord  &level(std::uint32_t index) const;

private:
    static constexpr std::uint32_t physical_gpu_mmu = 0x2u;
    static constexpr std::uint32_t memory_gpu_mmu = 0x40u;
    static constexpr std::uint32_t node_reserved_flags = 0xFFE0u;
    static constexpr std::uint32_t page_shift = 12u;
    static constexpr std::uint32_t highest_update_mode = 2u;

    std::int32_t refuse(GpuMmuRule rule, std::int32_t status);
    std::int32_t query_levels(void *context);
    std::int32_t check_nodes(void *context);
    GpuMmuRule   check_directories() const;

    FakeDxgadapter              &adapter_;
    DxgkGpuMmuCapsRecord         caps_;
    DxgkPageTableLevelDescRecord levels_[max_levels];
    GpuMmuRule                   failed_;
};

}  // namespace av

#endif  // AV_FAKE_GPU_MMU_HPP
