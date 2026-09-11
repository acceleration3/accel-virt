/* Why FakeVidMm refused an adapter's segments - one value per check
 * CreatePhysicalAdapterSegments @ 14009de94 and ValidateSegmentDescriptors
 * @ 14009ec8c (dxgmms2.sys) apply. */
#ifndef AV_SEGMENT_RULE_HPP
#define AV_SEGMENT_RULE_HPP

namespace av {

enum class SegmentRule : unsigned {
    none,
    count_query_failed,
    no_segment,                   /* "Driver must provide at least one segment" */
    too_many_segments,            /* "Too many segments specified (%u). Maximum allowed is %u" */
    descriptor_query_failed,      /* "Failed to query segment descriptors" */
    too_many_uefi_ranges,         /* "Maximum allowed is 1" */
    uefi_ranges_in_two_segments,
    size_not_page_aligned,        /* "Segment %u size (%I64u) must be page aligned" */
    invalid_flags,                /* "Invalid flags specified for segment #%d" */
    cpu_host_aperture_with_aperture,
    paging_buffer_segment_out_of_range,
};

}  // namespace av

#endif  // AV_SEGMENT_RULE_HPP
