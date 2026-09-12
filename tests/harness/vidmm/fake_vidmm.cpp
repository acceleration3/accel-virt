#include "harness/vidmm/fake_vidmm.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::int32_t query_segment4 = 11;   /* DXGKQAITYPE_QUERYSEGMENT4 */
constexpr std::uint64_t page_mask = 0xFFFu;

}  // namespace

FakeVidMm::FakeVidMm(FakeDxgadapter &adapter)
    : adapter_(adapter), answer_{}, descriptors_{}, failed_(SegmentRule::none) {}

std::int32_t FakeVidMm::refuse(SegmentRule rule, std::int32_t status)
{
    failed_ = rule;
    return status;
}

std::int32_t FakeVidMm::create_segments(void *context)
{
    failed_ = SegmentRule::none;
    std::memset(&answer_, 0, sizeof answer_);
    std::memset(descriptors_, 0, sizeof descriptors_);
    std::uint32_t physical_adapter = 0u;
    std::int32_t status = adapter_.query(context, query_segment4, &physical_adapter, sizeof physical_adapter,
                                         &answer_, sizeof answer_);
    if (status < 0) return refuse(SegmentRule::count_query_failed, status);
    if (answer_.NbSegment == 0u) return refuse(SegmentRule::no_segment, status_unsuccessful);
    if (answer_.NbSegment > max_segments) return refuse(SegmentRule::too_many_segments, status_unsuccessful);
    const std::uint32_t count = answer_.NbSegment;
    answer_.pSegmentDescriptor = descriptors_;
    answer_.SegmentDescriptorStride = stride;
    status = adapter_.query(context, query_segment4, &physical_adapter, sizeof physical_adapter, &answer_,
                            sizeof answer_);
    if (status < 0) return refuse(SegmentRule::descriptor_query_failed, status);
    const SegmentRule rule = validate(descriptors_, count, answer_.PagingBufferSegmentId);
    if (rule != SegmentRule::none) return refuse(rule, FakeDxgadapter::status_invalid_parameter);
    return 0;
}

SegmentRule FakeVidMm::validate(const std::uint8_t *descriptors, std::uint32_t count,
                                std::uint32_t paging_segment) const
{
    bool uefi_seen = false;
    for (std::uint32_t index = 0u; index < count; index++) {
        DxgkSegmentDescriptor4Record descriptor = {};
        std::memcpy(&descriptor, descriptors + index * stride, sizeof descriptor);
        if (descriptor.NumUEFIFrameBufferRanges > 1u) return SegmentRule::too_many_uefi_ranges;
        if (descriptor.NumUEFIFrameBufferRanges == 1u && uefi_seen) return SegmentRule::uefi_ranges_in_two_segments;
        uefi_seen = uefi_seen || descriptor.NumUEFIFrameBufferRanges == 1u;
        if ((descriptor.Size & page_mask) != 0u) return SegmentRule::size_not_page_aligned;
        const std::uint32_t flags = descriptor.Flags;
        const std::uint32_t hibernate = flags & flag_hibernate_preservation;
        const bool standby = (flags & flag_preserved_during_standby) != 0u;
        if (flags > highest_valid_flag || (flags & flag_agp) != 0u) return SegmentRule::invalid_flags;
        if (standby ? hibernate == flag_hibernate_preservation : hibernate != 0u) return SegmentRule::invalid_flags;
        if ((flags & flag_cpu_host_aperture) != 0u && (flags & (flag_aperture | flag_cpu_visible)) != 0u)
            return SegmentRule::cpu_host_aperture_with_aperture;
    }
    /* Segment identifiers are one-based, so the paging buffer's must name a real
     * segment: VidMmInitDmaPool turns it into a segment set and indexes the
     * adapter's segment pointers by bit position, so zero reaches a segment
     * that was never created and the guest bugchecks rather than refusing. */
    if (paging_segment == 0u || paging_segment > count)
        return SegmentRule::paging_buffer_segment_out_of_range;
    return SegmentRule::none;
}

std::uint32_t FakeVidMm::segment_count() const
{
    const bool implicit = (adapter_.physical_adapter_caps().Flags & physical_virtual_addressing) != 0u;
    return answer_.NbSegment + (implicit ? 1u : 0u);
}

SegmentRule                       FakeVidMm::failed_rule() const { return failed_; }
const DxgkQuerySegmentOut4Record &FakeVidMm::answer() const { return answer_; }

DxgkSegmentDescriptor4Record FakeVidMm::segment(std::uint32_t index) const
{
    DxgkSegmentDescriptor4Record descriptor = {};
    if (index < max_segments) std::memcpy(&descriptor, descriptors_ + index * stride, sizeof descriptor);
    return descriptor;
}

}  // namespace av
