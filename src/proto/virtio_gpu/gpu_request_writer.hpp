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

class GuestPageList;
class WireWriter;

class GpuRequestWriter {
public:
    static constexpr size_t   header_size = 24u;        /* struct virtio_gpu_ctrl_hdr */
    /* SET_SCANOUT_BLOB is the largest request that is not a page list: the
     * header, the rectangle, and four planes' strides and offsets. Every
     * request this size or under takes a narrow slot of the command queue,
     * which is what keeps the one wide slot for the page lists. */
    static constexpr size_t   max_request_size = 96u;
    static constexpr size_t   scanout_blob_request_size = max_request_size;
    static constexpr size_t   response_size = 24u;      /* a response the driver reads: its header */
    static constexpr uint32_t response_ok_nodata = 0x1100u;
    /* The OK range of virtio-gpu answers (virtio 1.2 5.7.6.7): anything below
     * is not an answer, anything from the error range is a refusal. */
    static constexpr uint32_t response_ok_first = 0x1100u;
    static constexpr uint32_t response_error_first = 0x1200u;
    /* The largest answer the driver reads: display info, one rectangle per
     * scanout after the header. */
    static constexpr size_t   max_response_size = 24u + 16u * 24u;
    static constexpr uint32_t format_b8g8r8a8 = 1u;     /* VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM */
    /* An attach_backing_pages request: the fixed part, then one memory entry
     * per run, so a caller can size the destination it hands the writer. */
    static constexpr size_t   backing_header_size = 32u;
    static constexpr size_t   memory_entry_size = 16u;
    /* The widest request this driver sends: a backing with one memory entry per
     * page of a 1920x1080 frame, which is the largest mode it offers and the
     * worst case when no two of its pages are adjacent. Rounded up to a page. */
    static constexpr size_t   max_backing_request_size = 0x9000u;

    GpuRequestWriter(uint8_t *bytes, size_t capacity);

    [[nodiscard]] Status create_2d(uint32_t resource_id, uint32_t format, uint32_t width, uint32_t height);
    /* One memory entry covering the whole backing. */
    [[nodiscard]] Status attach_backing(uint32_t resource_id, uint64_t address, uint32_t length);
    /* One memory entry per run of adjacent pages. A list with a page that was
     * never mapped is refused: the host would read the wrong memory. */
    [[nodiscard]] Status attach_backing_pages(uint32_t resource_id, const GuestPageList &pages);
    /* The whole resource, from offset 0. */
    [[nodiscard]] Status transfer_to_host_2d(uint32_t resource_id, uint32_t width, uint32_t height);
    /* What `scanout` shows, from the top left of `resource_id`; resource 0
     * shows nothing. */
    [[nodiscard]] Status set_scanout(uint32_t scanout, uint32_t resource_id, uint32_t width, uint32_t height);
    /* SET_SCANOUT_BLOB (virtio 1.2 5.7.6.10): the head shows the whole of the
     * framebuffer at the start of `resource_id`'s blob, one plane, B8G8R8A8.
     * A resource of 0 turns the head off, as SET_SCANOUT's does, and then the
     * geometry is zero too - which is what the device's own record demands of
     * a disable (specs/058, contracts/scanout_blob.md). */
    [[nodiscard]] Status set_scanout_blob(uint32_t scanout, uint32_t resource_id, uint32_t width,
                                          uint32_t height, uint32_t stride);
    /* The host reads the resource's pixels again, over that rectangle. */
    [[nodiscard]] Status resource_flush(uint32_t resource_id, uint32_t width, uint32_t height);
    /* The host may forget the resource; a mode change makes a new one. */
    [[nodiscard]] Status resource_unref(uint32_t resource_id);
    /* The resource lets go of its guest pages: the device refuses to attach
     * backing to a resource that already has some. */
    [[nodiscard]] Status detach_backing(uint32_t resource_id);
    /* rvk's own: the host fills `resource_id` from the host image published
     * under `key`, in place of a TRANSFER_TO_HOST_2D from the guest's pages. */
    [[nodiscard]] Status scanout_image(uint32_t resource_id, uint64_t key);
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
        set_scanout_blob_type = 0x010Du,
        resource_flush_type = 0x0104u,
        transfer_to_host_2d_type = 0x0105u,
        resource_attach_backing = 0x0106u,
        resource_detach_backing = 0x0107u,
        rvk_scanout_image = 0x02F0u,
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
