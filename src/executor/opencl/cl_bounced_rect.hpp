/* A guest's buffer-rectangle read into its context's arena, read into private
 * memory first and copied in a row at a time.
 *
 * The arena is GPU memory the host's memory source exported and mapped
 * (docs/host-memory.md). Intel's OpenCL reads a rectangle with its blitter
 * straight into the destination, which it must first pin as a userptr, and no
 * userptr pins a mapping of a dma-buf: it answers CL_OUT_OF_RESOURCES, measured
 * on an Iris Xe and its compute runtime. A whole-span read and an image read it
 * copies on the CPU, and those it takes as they are. The rows are copied and
 * not the span, since the gaps between them are the guest's. */
#ifndef AV_DEVICE_EXECUTOR_CL_BOUNCED_RECT_HPP
#define AV_DEVICE_EXECUTOR_CL_BOUNCED_RECT_HPP

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClBouncedRect {
public:
    ClBouncedRect();
    ~ClBouncedRect();
    ClBouncedRect(const ClBouncedRect &) = delete;
    ClBouncedRect &operator=(const ClBouncedRect &) = delete;

    /* Whether `answer` is the host refusing a read's destination rather than
     * the read: a refusal is remembered, and every read into the arena after it
     * is bounced without asking again. */
    [[nodiscard]] bool refusal(cl_int answer);
    /* A refusal was seen. */
    bool               bounces() const;
    /* clEnqueueReadBufferRect, blocking, into private memory at the same host
     * origin and pitches, then each of the region's rows into `ptr`: what the
     * host answered, CL_INVALID_VALUE for a region or pitch that names no
     * rows, or one whose reach wraps, and CL_OUT_OF_HOST_MEMORY when the
     * private memory cannot be had. */
    [[nodiscard]] cl_int read(cl_command_queue queue, cl_mem buffer, const size_t *buffer_origin,
                              const size_t *host_origin, const size_t *region, size_t buffer_row_pitch,
                              size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void *ptr,
                              cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);

private:
    /* At least `bytes` of private memory, kept for the next read. */
    [[nodiscard]] bool reserve(size_t bytes);

    uint8_t *bytes_;
    size_t   held_;
    bool     refused_;
};

}  // namespace av

#endif  // AV_DEVICE_EXECUTOR_CL_BOUNCED_RECT_HPP
