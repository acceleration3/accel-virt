#include "proto/virtio_gpu/gpu_request_writer.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

constexpr size_t create_2d_size = 40u;
constexpr size_t attach_backing_size = 48u;
constexpr size_t transfer_size = 56u;
constexpr size_t resource_unref_size = 32u;
constexpr size_t set_scanout_size = 48u;

constexpr size_t resource_flush_size = 48u;

}  // namespace

GpuRequestWriter::GpuRequestWriter(uint8_t *bytes, size_t capacity)
    : bytes_(bytes), capacity_(bytes != nullptr ? capacity : 0u), length_(0u)
    {
        AV_SCOPE("GpuRequestWriter::GpuRequestWriter");
    }

bool GpuRequestWriter::begin(WireWriter &writer, uint32_t type, size_t size)
{
    AV_SCOPE("GpuRequestWriter::begin");
    length_ = size <= capacity_ ? size : 0u;
    if (length_ == 0u) return false;
    writer.write_u32(type);
    writer.write_zeros(header_size - 4u);
    return true;
}

Status GpuRequestWriter::create_2d(uint32_t resource_id, uint32_t format, uint32_t width, uint32_t height)
{
    AV_SCOPE("GpuRequestWriter::create_2d");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_create_2d, create_2d_size)) return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_u32(format);
    writer.write_u32(width);
    writer.write_u32(height);
    return Status::ok;
}

Status GpuRequestWriter::attach_backing(uint32_t resource_id, uint64_t address, uint32_t length)
{
    AV_SCOPE("GpuRequestWriter::attach_backing");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_attach_backing, attach_backing_size)) return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_u32(1u);
    writer.write_u64(address);
    writer.write_u32(length);
    writer.write_zeros(4u);
    return Status::ok;
}

Status GpuRequestWriter::attach_backing_pages(uint32_t resource_id, const GuestPageList &pages)
{
    AV_SCOPE("GpuRequestWriter::attach_backing_pages");
    length_ = 0u;
    if (!pages.complete()) return Status::invalid_parameter;
    WireWriter     writer(bytes_, capacity_);
    const uint32_t runs = pages.runs();
    if (!begin(writer, resource_attach_backing, backing_header_size + size_t{runs} * memory_entry_size))
        return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_u32(runs);
    for (uint32_t page = 0u; page < pages.count();) {
        const uint32_t run = pages.run_length(page);
        writer.write_u64(pages.frame(page));
        writer.write_u32(static_cast<uint32_t>(run * GuestPageList::page_bytes));
        writer.write_zeros(4u);
        page += run;
    }
    return Status::ok;
}

Status GpuRequestWriter::transfer_to_host_2d(uint32_t resource_id, uint32_t width, uint32_t height)
{
    AV_SCOPE("GpuRequestWriter::transfer_to_host_2d");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, transfer_to_host_2d_type, transfer_size)) return Status::too_large;
    writer.write_zeros(8u);
    writer.write_u32(width);
    writer.write_u32(height);
    writer.write_u64(0u);
    writer.write_u32(resource_id);
    writer.write_zeros(4u);
    return Status::ok;
}

Status GpuRequestWriter::resource_unref(uint32_t resource_id)
{
    AV_SCOPE("GpuRequestWriter::resource_unref");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_unref_type, resource_unref_size)) return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_zeros(4u);
    return Status::ok;
}

/* struct virtio_gpu_resource_detach_backing: the resource and padding, as unref's. */
Status GpuRequestWriter::detach_backing(uint32_t resource_id)
{
    AV_SCOPE("GpuRequestWriter::detach_backing");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_detach_backing, resource_unref_size)) return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_zeros(4u);
    return Status::ok;
}

/* struct virtio_gpu_set_scanout: the rectangle, then the scanout and the
 * resource whose pixels it shows. */
Status GpuRequestWriter::set_scanout(uint32_t scanout, uint32_t resource_id, uint32_t width, uint32_t height)
{
    AV_SCOPE("GpuRequestWriter::set_scanout");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, set_scanout_type, set_scanout_size)) return Status::too_large;
    writer.write_zeros(8u);
    writer.write_u32(width);
    writer.write_u32(height);
    writer.write_u32(scanout);
    writer.write_u32(resource_id);
    return Status::ok;
}

/* The rectangle is the whole framebuffer, which is what a primary's scanout
 * always shows; a blob of 0 is the disable and carries none of it. */
Status GpuRequestWriter::set_scanout_blob(uint32_t scanout, uint32_t resource_id, uint32_t width,
                                          uint32_t height, uint32_t stride)
{
    AV_SCOPE("GpuRequestWriter::set_scanout_blob");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, set_scanout_blob_type, scanout_blob_request_size)) return Status::too_large;
    writer.write_zeros(8u);
    writer.write_u32(width);
    writer.write_u32(height);
    writer.write_u32(scanout);
    writer.write_u32(resource_id);
    writer.write_u32(width);
    writer.write_u32(height);
    writer.write_u32(resource_id != 0u ? format_b8g8r8a8 : 0u);
    writer.write_zeros(4u);
    writer.write_u32(stride);
    writer.write_zeros(12u);
    writer.write_zeros(16u);
    return Status::ok;
}

Status GpuRequestWriter::resource_flush(uint32_t resource_id, uint32_t width, uint32_t height)
{
    AV_SCOPE("GpuRequestWriter::resource_flush");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_flush_type, resource_flush_size)) return Status::too_large;
    writer.write_zeros(8u);
    writer.write_u32(width);
    writer.write_u32(height);
    writer.write_u32(resource_id);
    writer.write_zeros(4u);
    return Status::ok;
}

Status GpuRequestWriter::get_display_info()
{
    AV_SCOPE("GpuRequestWriter::get_display_info");
    WireWriter writer(bytes_, capacity_);
    return begin(writer, get_display_info_type, header_size) ? Status::ok : Status::too_large;
}

/* struct virtio_gpu_update_cursor after its header: the position, then the
 * fields only an update reads. */
Status GpuRequestWriter::update_cursor(uint32_t scanout, int32_t x, int32_t y, uint32_t resource_id, uint32_t hot_x,
                                       uint32_t hot_y)
{
    AV_SCOPE("GpuRequestWriter::update_cursor");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, update_cursor_type, cursor_request_size)) return Status::too_large;
    writer.write_u32(scanout);
    writer.write_u32(static_cast<uint32_t>(x));
    writer.write_u32(static_cast<uint32_t>(y));
    writer.write_zeros(4u);
    writer.write_u32(resource_id);
    writer.write_u32(hot_x);
    writer.write_u32(hot_y);
    writer.write_zeros(4u);
    return Status::ok;
}

/* The same layout as an update, whose last fields a move leaves zero. */
Status GpuRequestWriter::move_cursor(uint32_t scanout, int32_t x, int32_t y)
{
    AV_SCOPE("GpuRequestWriter::move_cursor");
    const Status status = update_cursor(scanout, x, y, 0u, 0u, 0u);
    if (status == Status::ok) WireWriter(bytes_, sizeof(uint32_t)).write_u32(move_cursor_type);
    return status;
}

size_t GpuRequestWriter::length() const
{
    AV_SCOPE("GpuRequestWriter::length");
    return length_;
}

}  // namespace av
