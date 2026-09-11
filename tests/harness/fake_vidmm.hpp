/* VidMm's side of one physical adapter's segments: the two
 * DXGKQAITYPE_QUERYSEGMENT4 passes of CreatePhysicalAdapterSegments @ 14009de94
 * (dxgmms2.sys 10.0.26100.9444) and ValidateSegmentDescriptors @ 14009ec8c,
 * through FakeDxgadapter so every answer is held to dxgkrnl's status rule.
 * ADAPTER_RENDER::Initialize reaches it only for a render adapter. */
#ifndef AV_FAKE_VIDMM_HPP
#define AV_FAKE_VIDMM_HPP

#include "../../src/core/ddi/dxgk_query_segment_out4_record.hpp"
#include "../../src/core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "fake_dxgadapter.hpp"
#include "segment_rule.hpp"

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

    SegmentRule                       failed_rule() const;
    const DxgkQuerySegmentOut4Record &answer() const;
    DxgkSegmentDescriptor4Record      segment(std::uint32_t index) const;

private:
    static constexpr std::int32_t  status_unsuccessful = static_cast<std::int32_t>(0xC0000001u);
    static constexpr std::uint32_t highest_valid_flag = 0x3FFFFFu;
    enum : std::uint32_t {
        flag_aperture = 0x1u, flag_agp = 0x2u, flag_cpu_visible = 0x4u, flag_preserved_during_standby = 0x80u,
        flag_hibernate_preservation = 0x300u, flag_cpu_host_aperture = 0x2000u,
    };

    std::int32_t refuse(SegmentRule rule, std::int32_t status);

    FakeDxgadapter            &adapter_;
    DxgkQuerySegmentOut4Record answer_;
    std::uint8_t               descriptors_[max_segments * stride];
    SegmentRule                failed_;
};

}  // namespace av

#endif  // AV_FAKE_VIDMM_HPP
