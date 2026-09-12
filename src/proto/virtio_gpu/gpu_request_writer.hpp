/* virtio-gpu requests (virtio 1.2 5.7.6) the driver sends for its own
 * resources: a 2D resource, its backing, a transfer to the host, and the
 * cursor. Each request replaces what the destination held; a request that does
 * not fit writes nothing. Little-endian, no fence, no context. */
#ifndef AV_GPU_REQUEST_WRITER_HPP
#define AV_GPU_REQUEST_WRITER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireWriter;

class GpuRequestWriter {
public:
    static constexpr size_t   header_size = 24u;        /* struct virtio_gpu_ctrl_hdr */
    static constexpr size_t   max_request_size = 64u;
    static constexpr size_t   response_size = 24u;      /* a response the driver reads: its header */
    static constexpr uint32_t response_ok_nodata = 0x1100u;
    static constexpr uint32_t format_b8g8r8a8 = 1u;     /* VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM */

    GpuRequestWriter(uint8_t *bytes, size_t capacity);

    [[nodiscard]] Status create_2d(uint32_t resource_id, uint32_t format, uint32_t width, uint32_t height);
    /* One memory entry covering the whole backing. */
    [[nodiscard]] Status attach_backing(uint32_t resource_id, uint64_t address, uint32_t length);
    /* The whole resource, from offset 0. */
    [[nodiscard]] Status transfer_to_host_2d(uint32_t resource_id, uint32_t width, uint32_t height);
    /* What `scanout` shows, from the top left of `resource_id`; resource 0
     * shows nothing. */
    [[nodiscard]] Status set_scanout(uint32_t scanout, uint32_t resource_id, uint32_t width, uint32_t height);
    /* The host reads the resource's pixels again, over that rectangle. */
    [[nodiscard]] Status resource_flush(uint32_t resource_id, uint32_t width, uint32_t height);
    /* The host may forget the resource; a mode change makes a new one. */
    [[nodiscard]] Status resource_unref(uint32_t resource_id);
    /* Asks for every scanout's rectangle; the answer is a DisplayInfoResponse. */
    [[nodiscard]] Status get_display_info();
    /* Resource 0 hides the cursor. */
    [[nodiscard]] Status update_cursor(uint32_t scanout, int32_t x, int32_t y, uint32_t resource_id, uint32_t hot_x,
                                       uint32_t hot_y);
    [[nodiscard]] Status move_cursor(uint32_t scanout, int32_t x, int32_t y);
    /* Bytes of the last request written; zero after one that did not fit. */
    size_t               length() const;

private:
    enum : uint32_t {
        resource_create_2d = 0x0101u,
        get_display_info_type = 0x0100u,
        resource_unref_type = 0x0102u,
        set_scanout_type = 0x0103u,
        resource_flush_type = 0x0104u,
        transfer_to_host_2d_type = 0x0105u,
        resource_attach_backing = 0x0106u,
        update_cursor_type = 0x0300u,
        move_cursor_type = 0x0301u,
    };
    static constexpr size_t cursor_request_size = 56u;

    /* Starts a request of `size` bytes with its header; false when it cannot fit. */
    bool begin(WireWriter &writer, uint32_t type, size_t size);

    uint8_t *bytes_;
    size_t   capacity_;
    size_t   length_;
};

}  // namespace av

#endif  // AV_GPU_REQUEST_WRITER_HPP
