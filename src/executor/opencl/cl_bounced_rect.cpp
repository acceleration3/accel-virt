#include "executor/opencl/cl_bounced_rect.hpp"

#include "core/trace/function_scope.hpp"

#include <sys/mman.h>

namespace av {

namespace {

constexpr size_t page_bytes = 4096u;

}  // namespace

ClBouncedRect::ClBouncedRect() : bytes_(nullptr), held_(0u), refused_(false)
{
    AV_SCOPE("ClBouncedRect::ClBouncedRect");
}

ClBouncedRect::~ClBouncedRect()
{
    AV_SCOPE("ClBouncedRect::~ClBouncedRect");
    if (bytes_ != nullptr) static_cast<void>(munmap(bytes_, held_));
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

bool ClBouncedRect::reserve(size_t bytes)
{
    AV_SCOPE("ClBouncedRect::reserve");
    if (bytes <= held_) return true;
    if (bytes > SIZE_MAX - page_bytes) return false;
    const size_t whole = (bytes + page_bytes - 1u) / page_bytes * page_bytes;
    void *const  mapped = mmap(nullptr, whole, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) return false;
    if (bytes_ != nullptr) static_cast<void>(munmap(bytes_, held_));
    bytes_ = static_cast<uint8_t *>(mapped);
    held_ = whole;
    return true;
}

/* The host lays the rows out in the private memory exactly as it would have in
 * `ptr`, so a row's offset is the same in both. */
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
    /* With the slice at least its rows, the region's last row cannot wrap
     * where its last slice did not. */
    size_t down = 0u, across = 0u, first = 0u, reach = 0u;
    if (row < region[0] || slice / row < region[1] || __builtin_mul_overflow(host_origin[1], row, &across) ||
        __builtin_mul_overflow(host_origin[2], slice, &down) || __builtin_add_overflow(across, down, &first) ||
        __builtin_add_overflow(first, host_origin[0], &first) ||
        __builtin_mul_overflow(region[2] - 1u, slice, &down) || __builtin_add_overflow(first, down, &reach) ||
        __builtin_add_overflow(reach, (region[1] - 1u) * row, &reach) ||
        __builtin_add_overflow(reach, region[0], &reach))
        return CL_INVALID_VALUE;
    if (!reserve(reach)) return CL_OUT_OF_HOST_MEMORY;
    const cl_int read = ::clEnqueueReadBufferRect(queue, buffer, CL_TRUE, buffer_origin, host_origin, region,
                                                  buffer_row_pitch, buffer_slice_pitch, row, slice, bytes_,
                                                  num_events_in_wait_list, event_wait_list, event);
    if (read != CL_SUCCESS) return read;
    uint8_t *const to = static_cast<uint8_t *>(ptr);
    for (size_t z = 0u; z < region[2]; z++)
        for (size_t y = 0u; y < region[1]; y++) {
            const size_t at = first + z * slice + y * row;
            __builtin_memcpy(to + at, bytes_ + at, region[0]);
        }
    return CL_SUCCESS;
}

}  // namespace av
