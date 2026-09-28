/* VidMm's side of one physical adapter's segments: the two
 * DXGKQAITYPE_QUERYSEGMENT4 passes of CreatePhysicalAdapterSegments @ 14009de94
 * (dxgmms2.sys 10.0.26100.9444) and ValidateSegmentDescriptors @ 14009ec8c,
 * through FakeDxgadapter so every answer is held to dxgkrnl's status rule.
 * ADAPTER_RENDER::Initialize reaches it only for a render adapter. */
#ifndef AV_FAKE_VIDMM_HPP
#define AV_FAKE_VIDMM_HPP

#include "core/ddi/dxgk_query_segment_out4_record.hpp"
#include "core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "harness/dxgkrnl/fake_dxgadapter.hpp"
#include "harness/vidmm/segment_rule.hpp"

#include <cstdint>

namespace av {

class FakeVidMm {
public:
    static constexpr std::uint32_t max_segments = 32u;
    static constexpr std::uint32_t stride = 0x68u;

    explicit FakeVidMm(FakeDxgadapter &adapter);

    /* Answers the NTSTATUS VidMm fails adapter start with. */
    std::int32_t create_segments(void *context);
    /* The descriptor checks alone, over `count` descriptors `stride` apart. */
    SegmentRule validate(const std::uint8_t *descriptors, std::uint32_t count, std::uint32_t paging_segment) const;

    /* The segments VidMm knows: the driver's, and for a GPU MMU or IoMmu
     * physical adapter the system memory segment it appends
     * (CreatePhysicalAdapterSegments). */
    std::uint32_t                     segment_count() const;
    /* VIDMM_GLOBAL::GetTotalSegmentSize @ 14010636c over the driver's
     * segments, each counting the smaller of its size and commit limit: one
     * neither Aperture nor ReservedSysMem is dedicated - video memory, or
     * system memory when PopulatedFromSystemMemory - and an aperture is
     * shared. The segment VidMm appends is ReservedSysMem
     * (InitializePhysicalAdapterSegments @ 140116728 flags it 0x1180 and up),
     * so it is never dedicated. These are DXGI's DedicatedVideoMemory and
     * DedicatedSystemMemory, and what dxdiag shows. */
    std::uint64_t                     dedicated_video_memory() const;
    std::uint64_t                     dedicated_system_memory() const;
    std::uint64_t                     aperture_memory() const;
    SegmentRule                       failed_rule() const;
    const DxgkQuerySegmentOut4Record &answer() const;
    DxgkSegmentDescriptor4Record      segment(std::uint32_t index) const;

private:
    static constexpr std::int32_t  status_unsuccessful = static_cast<std::int32_t>(0xC0000001u);
    static constexpr std::uint32_t highest_valid_flag = 0x3FFFFFu;
    enum : std::uint32_t {
        flag_aperture = 0x1u, flag_agp = 0x2u, flag_cpu_visible = 0x4u, flag_populated_from_system_memory = 0x40u,
        flag_preserved_during_standby = 0x80u, flag_hibernate_preservation = 0x300u, flag_reserved_system_memory = 0x1000u,
        flag_cpu_host_aperture = 0x2000u,
    };

    static constexpr std::uint32_t physical_virtual_addressing = 0x3u;   /* IoMmuSupported | GpuMmuSupported */

    std::int32_t refuse(SegmentRule rule, std::int32_t status);
    /* The segments whose flags under `mask` are `match`. */
    std::uint64_t total(std::uint32_t mask, std::uint32_t match) const;

    FakeDxgadapter            &adapter_;
    DxgkQuerySegmentOut4Record answer_;
    std::uint8_t               descriptors_[max_segments * stride];
    SegmentRule                failed_;
};

}  // namespace av

#endif  // AV_FAKE_VIDMM_HPP
