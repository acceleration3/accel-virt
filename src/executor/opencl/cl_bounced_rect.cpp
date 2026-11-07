#include "executor/opencl/cl_bounced_rect.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/pitched_extent.hpp"

namespace av {

ClBouncedRect::ClBouncedRect() : staging_(), refused_(false)
{
    AV_SCOPE("ClBouncedRect::ClBouncedRect");
}

bool ClBouncedRect::refusal(cl_int answer)
{
    AV_SCOPE("ClBouncedRect::refusal");
    const bool refused = answer == CL_OUT_OF_RESOURCES || answer == CL_OUT_OF_HOST_MEMORY ||
                         answer == CL_MEM_OBJECT_ALLOCATION_FAILURE;
    refused_ = refused_ || refused;
    return refused;
}

bool ClBouncedRect::bounces() const
{
    AV_SCOPE("ClBouncedRect::bounces");
    return refused_;
}

cl_int ClBouncedRect::read(cl_command_queue queue, cl_mem buffer, const size_t *buffer_origin,
                           const size_t *host_origin, const size_t *region, size_t buffer_row_pitch,
                           size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void *ptr,
                           cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("ClBouncedRect::read");
    if (host_origin == nullptr || region == nullptr || ptr == nullptr || region[0] == 0u || region[1] == 0u ||
        region[2] == 0u)
        return CL_INVALID_VALUE;
    const size_t row = host_row_pitch != 0u ? host_row_pitch : region[0];
    size_t       slice = host_slice_pitch;
    if (slice == 0u && __builtin_mul_overflow(region[1], row, &slice)) return CL_INVALID_VALUE;
    if (row < region[0] || slice / row < region[1]) return CL_INVALID_VALUE;
    const PitchedExtent extent(region[0], region[1], region[2], row, slice);
    uint64_t            first = 0u, reach = 0u;
    if (!extent.offset_of(host_origin[1], host_origin[2], first) ||
        __builtin_add_overflow(first, host_origin[0], &first) || !extent.end(first, reach))
        return CL_INVALID_VALUE;
    if (!staging_.reserve(static_cast<size_t>(reach))) return CL_OUT_OF_HOST_MEMORY;
    const cl_int read = ::clEnqueueReadBufferRect(queue, buffer, CL_TRUE, buffer_origin, host_origin, region,
                                                  buffer_row_pitch, buffer_slice_pitch, row, slice, staging_.bytes(),
                                                  num_events_in_wait_list, event_wait_list, event);
    if (read != CL_SUCCESS) return read;
    uint8_t *const to = static_cast<uint8_t *>(ptr);
    for (size_t z = 0u; z < region[2]; z++)
        for (size_t y = 0u; y < region[1]; y++) {
            const size_t at = static_cast<size_t>(first) + z * slice + y * row;
            __builtin_memcpy(to + at, staging_.bytes() + at, region[0]);
        }
    return CL_SUCCESS;
}

void ClBouncedRect::use_host_ram(ByteLedger *ledger)
{
    AV_SCOPE("ClBouncedRect::use_host_ram");
    staging_.bind(ledger);
}

void ClBouncedRect::release_staging()
{
    AV_SCOPE("ClBouncedRect::release_staging");
    staging_.release();
}

}
