#ifndef AV_SEGMENT_RULE_HPP
#define AV_SEGMENT_RULE_HPP

namespace av {

enum class SegmentRule : unsigned {
    none,
    count_query_failed,
    no_segment,
    too_many_segments,
    descriptor_query_failed,
    too_many_uefi_ranges,
    uefi_ranges_in_two_segments,
    size_not_page_aligned,
    invalid_flags,
    cpu_host_aperture_with_aperture,
    paging_buffer_segment_out_of_range,
};

}

#endif
