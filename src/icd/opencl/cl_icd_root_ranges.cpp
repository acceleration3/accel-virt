#include "icd/opencl/cl_icd_root.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_buffer_rect.hpp"
#include "icd/opencl/cl_image_region.hpp"

namespace av::clicd {

cl_int ClIcdRoot::judge_buffer_range(const ClObject &mem, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("ClIcdRoot::judge_buffer_range");
    if (mem.extent().shape() == ClMemExtent::Shape::image) return CL_INVALID_MEM_OBJECT;
    return mem.extent().holds_range(offset, bytes) ? CL_SUCCESS : CL_INVALID_VALUE;
}

bool ClIcdRoot::same_backing(const ClObject &first, const ClObject &second)
{
    AV_SCOPE("ClIcdRoot::same_backing");
    const ClObject *const a = first.sliced_from() != nullptr ? first.sliced_from() : &first;
    const ClObject *const b = second.sliced_from() != nullptr ? second.sliced_from() : &second;
    return a == b;
}

bool ClIcdRoot::buffer_ranges_overlap(const ClObject &first, uint64_t first_offset, const ClObject &second,
                                      uint64_t second_offset, uint64_t bytes)
{
    AV_SCOPE("ClIcdRoot::buffer_ranges_overlap");
    if (!same_backing(first, second)) return false;
    const uint64_t a = first.extent().base() + first_offset;
    const uint64_t b = second.extent().base() + second_offset;
    return a < b + bytes && b < a + bytes;
}

cl_int ClIcdRoot::judge_rect_copy(const ClObject &source, const ClObject &target, const size_t *from_origin,
                                  const size_t *to_origin, const size_t *region, size_t from_row_pitch,
                                  size_t from_slice_pitch, size_t to_row_pitch, size_t to_slice_pitch)
{
    AV_SCOPE("ClIcdRoot::judge_rect_copy");
    if (source.extent().shape() == ClMemExtent::Shape::image || target.extent().shape() == ClMemExtent::Shape::image)
        return CL_INVALID_MEM_OBJECT;
    const ClBufferRect from_side(from_origin, from_origin, region, from_row_pitch, from_slice_pitch, 0u, 0u);
    const ClBufferRect to_side(to_origin, to_origin, region, to_row_pitch, to_slice_pitch, 0u, 0u);
    if (from_side.buffer_side().valid() == false || to_side.buffer_side().valid() == false) return CL_INVALID_VALUE;
    if (!from_side.buffer_side().slices_are_whole_rows() || !to_side.buffer_side().slices_are_whole_rows())
        return CL_INVALID_VALUE;
    uint64_t from_first = 0u;
    uint64_t from_end = 0u;
    uint64_t to_first = 0u;
    uint64_t to_end = 0u;
    if (!from_side.buffer_span(from_first, from_end) || !to_side.buffer_span(to_first, to_end)) return CL_INVALID_VALUE;
    if (source.extent().shape() == ClMemExtent::Shape::buffer &&
        (from_end > source.extent().bytes() || target.extent().shape() != ClMemExtent::Shape::buffer ||
         to_end > target.extent().bytes()))
        return CL_INVALID_VALUE;
    if (!same_backing(source, target)) return CL_SUCCESS;
    const uint64_t from_base = source.extent().base();
    const uint64_t to_base = target.extent().base();
    const ClImageRegion &a = from_side.buffer_side();
    const ClImageRegion &b = to_side.buffer_side();
    const size_t rows = a.rows() * a.slices();
    if (rows > 64u) {
        return from_base + from_first < to_base + to_end && to_base + to_first < from_base + from_end
                   ? CL_MEM_COPY_OVERLAP
                   : CL_SUCCESS;
    }
    for (size_t slice_a = 0u; slice_a < a.slices(); ++slice_a)
        for (size_t row_a = 0u; row_a < a.rows(); ++row_a) {
            const uint64_t a_low = from_base + from_first + a.row_offset(row_a, slice_a);
            const uint64_t a_high = a_low + a.row_bytes();
            for (size_t slice_b = 0u; slice_b < b.slices(); ++slice_b)
                for (size_t row_b = 0u; row_b < b.rows(); ++row_b) {
                    const uint64_t b_low = to_base + to_first + b.row_offset(row_b, slice_b);
                    if (a_low < b_low + b.row_bytes() && b_low < a_high) return CL_MEM_COPY_OVERLAP;
                }
        }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::judge_rect_transfer(const ClObject &mem, const ClBufferRect &rect)
{
    AV_SCOPE("ClIcdRoot::judge_rect_transfer");
    if (mem.extent().shape() == ClMemExtent::Shape::image) return CL_INVALID_MEM_OBJECT;
    uint64_t first = 0u;
    uint64_t end = 0u;
    if (!rect.buffer_span(first, end)) return CL_INVALID_VALUE;
    if (mem.extent().shape() == ClMemExtent::Shape::buffer && end > mem.extent().bytes()) return CL_INVALID_VALUE;
    return CL_SUCCESS;
}

cl_int ClIcdRoot::judge_fill(const ClObject &mem, size_t pattern_bytes, size_t offset, size_t bytes)
{
    AV_SCOPE("ClIcdRoot::judge_fill");
    const bool power = pattern_bytes != 0u && (pattern_bytes & (pattern_bytes - 1u)) == 0u && pattern_bytes <= 128u;
    if (!power || offset % pattern_bytes != 0u || bytes % pattern_bytes != 0u) return CL_INVALID_VALUE;
    return judge_buffer_range(mem, offset, bytes);
}

}
