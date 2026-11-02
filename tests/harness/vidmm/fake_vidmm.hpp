#ifndef AV_FAKE_VIDMM_HPP
#define AV_FAKE_VIDMM_HPP

#include "miniport/ddi/dxgk_query_segment_out4_record.hpp"
#include "miniport/ddi/dxgk_segment_descriptor4_record.hpp"
#include "harness/dxgkrnl/fake_dxgadapter.hpp"
#include "harness/vidmm/segment_rule.hpp"

#include <cstdint>

namespace av {

class FakeVidMm {
public:
    static constexpr std::uint32_t max_segments = 32u;
    static constexpr std::uint32_t stride = 0x68u;

    explicit FakeVidMm(FakeDxgadapter &adapter);

    std::int32_t create_segments(void *context);
    SegmentRule validate(const std::uint8_t *descriptors, std::uint32_t count, std::uint32_t paging_segment) const;

    std::uint32_t                     segment_count() const;
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

    static constexpr std::uint32_t physical_virtual_addressing = 0x3u;

    std::int32_t refuse(SegmentRule rule, std::int32_t status);
    std::uint64_t total(std::uint32_t mask, std::uint32_t match) const;

    FakeDxgadapter            &adapter_;
    DxgkQuerySegmentOut4Record answer_;
    std::uint8_t               descriptors_[max_segments * stride];
    SegmentRule                failed_;
};

}

#endif
