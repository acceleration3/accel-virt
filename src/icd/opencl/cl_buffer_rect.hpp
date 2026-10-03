/* The rectangle clEnqueueReadBufferRect and clEnqueueWriteBufferRect name: a
 * corner and pitches in the buffer, a corner and pitches in the application's
 * memory, and the one region both share, in bytes, rows and slices.
 *
 * Both sides are image regions of one-byte elements (ClImageRegion), so a
 * transfer is cut into commands the way an image's is. What this adds is the
 * API's own refusals and where the application's corner lies from its
 * pointer, which is where the bytes a transfer moves start. */
#ifndef AV_CLICD_CL_BUFFER_RECT_HPP
#define AV_CLICD_CL_BUFFER_RECT_HPP

#include "icd/opencl/cl_image_region.hpp"

#include <CL/cl.h>
#include <stddef.h>

namespace av::clicd {

class ClBufferRect {
public:
    /* The application's own arguments; a null corner or region is kept as
     * a refusal for check(), not read. */
    ClBufferRect(const size_t *buffer_origin, const size_t *host_origin, const size_t *region,
                 size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch,
                 size_t host_slice_pitch);

    /* CL_SUCCESS, or what the API answers: CL_INVALID_VALUE for a null corner
     * or region, an empty extent, a pitch too small for what it holds or a
     * slice that is not whole rows on either side, and a host corner beyond
     * what a pointer reaches; CL_OUT_OF_RESOURCES for a rectangle whose rows
     * are longer than one command can count. */
    cl_int check() const;
    const ClImageRegion &buffer_side() const;
    const ClImageRegion &application() const;
    /* From the application's pointer to the rectangle's first byte. */
    size_t host_offset() const;

private:
    static constexpr size_t most_countable = 0xFFFFFFFFu;

    ClImageRegion buffer_side_;
    ClImageRegion application_;
    size_t        host_offset_;
    bool          named_;
    bool          reachable_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_BUFFER_RECT_HPP
