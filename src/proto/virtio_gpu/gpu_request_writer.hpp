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
    static constexpr size_t   header_size = 24u;
    static constexpr size_t   max_request_size = 96u;
    static constexpr size_t   scanout_blob_request_size = max_request_size;
    static constexpr size_t   response_size = 24u;
    static constexpr uint32_t response_ok_nodata = 0x1100u;
    static constexpr uint32_t response_ok_first = 0x1100u;
    static constexpr uint32_t response_error_first = 0x1200u;
    static constexpr size_t   max_response_size = 24u + 16u * 24u;
    static constexpr uint32_t format_b8g8r8a8 = 1u;
    static constexpr size_t   backing_header_size = 32u;
    static constexpr size_t   memory_entry_size = 16u;
    static constexpr size_t   max_backing_request_size = 0x9000u;

    GpuRequestWriter(uint8_t *bytes, size_t capacity);

    [[nodiscard]] Status create_2d(uint32_t resource_id, uint32_t format, uint32_t width, uint32_t height);
    [[nodiscard]] Status attach_backing(uint32_t resource_id, uint64_t address, uint32_t length);
    [[nodiscard]] Status attach_backing_pages(uint32_t resource_id, const GuestPageList &pages);
    [[nodiscard]] Status transfer_to_host_2d(uint32_t resource_id, uint32_t width, uint32_t height);
    [[nodiscard]] Status set_scanout(uint32_t scanout, uint32_t resource_id, uint32_t width, uint32_t height);
    [[nodiscard]] Status set_scanout_blob(uint32_t scanout, uint32_t resource_id, uint32_t width,
                                          uint32_t height, uint32_t stride);
    [[nodiscard]] Status resource_flush(uint32_t resource_id, uint32_t width, uint32_t height);
    [[nodiscard]] Status resource_unref(uint32_t resource_id);
    [[nodiscard]] Status detach_backing(uint32_t resource_id);
    [[nodiscard]] Status scanout_image(uint32_t resource_id, uint64_t key, uint32_t frame);
    [[nodiscard]] Status get_display_info();
    [[nodiscard]] Status update_cursor(uint32_t scanout, int32_t x, int32_t y, uint32_t resource_id, uint32_t hot_x,
                                       uint32_t hot_y);
    [[nodiscard]] Status move_cursor(uint32_t scanout, int32_t x, int32_t y, uint32_t resource_id);
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
        av_scanout_image = 0x02F0u,
        update_cursor_type = 0x0300u,
        move_cursor_type = 0x0301u,
    };
    static constexpr size_t cursor_request_size = 56u;

    bool begin(WireWriter &writer, uint32_t type, size_t size);

    uint8_t *bytes_;
    size_t   capacity_;
    size_t   length_;
};

}

#endif
