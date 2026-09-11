#include "hardware_cursor.hpp"

#include "../proto/gpu_request_writer.hpp"
#include "cursor_image.hpp"
#include "ddi/dxgkarg_set_pointer_position_record.hpp"
#include "ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "display_sources.hpp"
#include "virtio_gpu_device.hpp"

namespace av {

HardwareCursor::HardwareCursor(Platform &platform, ObjectStorage<VirtioGpuDevice> &device)
    : platform_(platform), device_(device), backing_(nullptr), resource_created_(false), shape_set_(false),
      visible_(false), hot_x_(0u), hot_y_(0u), x_(0), y_(0) {}

HardwareCursor::~HardwareCursor() { release(); }

Status HardwareCursor::initialize()
{
    platform_.require_irql_at_most(Irql::passive, "HardwareCursor::initialize");
    if (backing_ != nullptr) return Status::invalid_state;
    backing_ = static_cast<uint8_t *>(platform_.allocate_contiguous(CursorImage::pixel_bytes));
    return backing_ != nullptr ? Status::ok : Status::no_memory;
}

void HardwareCursor::release()
{
    platform_.require_irql_at_most(Irql::passive, "HardwareCursor::release");
    if (backing_ != nullptr) platform_.release_contiguous(backing_, CursorImage::pixel_bytes);
    backing_ = nullptr;
    forget_device();
}

void HardwareCursor::forget_device()
{
    platform_.require_irql_at_most(Irql::passive, "HardwareCursor::forget_device");
    resource_created_ = false;
    shape_set_ = false;
    visible_ = false;
}

/* Every request here fits the buffer; one that did not would have length
 * zero, which the queue refuses, so the writer's own answer adds nothing. */
Status HardwareCursor::create_resource(VirtioGpuDevice &device)
{
    uint8_t          request[GpuRequestWriter::max_request_size];
    GpuRequestWriter writer(request, sizeof request);
    static_cast<void>(writer.create_2d(resource_id, GpuRequestWriter::format_b8g8r8a8, max_side, max_side));
    Status status = device.send_control(request, writer.length());
    if (status != Status::ok) return status;
    static_cast<void>(writer.attach_backing(resource_id, platform_.physical_address(backing_),
                                            static_cast<uint32_t>(CursorImage::pixel_bytes)));
    status = device.send_control(request, writer.length());
    resource_created_ = status == Status::ok;
    return status;
}

Status HardwareCursor::update(VirtioGpuDevice &device, uint32_t resource)
{
    uint8_t          request[GpuRequestWriter::max_request_size];
    GpuRequestWriter writer(request, sizeof request);
    static_cast<void>(writer.update_cursor(scanout, x_, y_, resource, hot_x_, hot_y_));
    return device.send_cursor(request, writer.length());
}

Status HardwareCursor::set_shape(const DxgkargSetPointerShapeRecord &shape)
{
    platform_.require_irql_at_most(Irql::passive, "HardwareCursor::set_shape");
    if (backing_ == nullptr || !device_.constructed()) return Status::invalid_state;
    if (shape.VidPnSourceId >= DisplaySources::source_count) return Status::invalid_parameter;
    Status status = CursorImage(platform_, shape).draw(backing_);
    if (status != Status::ok) return status;
    VirtioGpuDevice &device = device_.object();
    if (!resource_created_) status = create_resource(device);
    if (status != Status::ok) return status;
    uint8_t          request[GpuRequestWriter::max_request_size];
    GpuRequestWriter writer(request, sizeof request);
    static_cast<void>(writer.transfer_to_host_2d(resource_id, max_side, max_side));
    status = device.send_control(request, writer.length());
    if (status != Status::ok) return status;
    hot_x_ = shape.XHot;
    hot_y_ = shape.YHot;
    shape_set_ = true;
    return visible_ ? update(device, resource_id) : Status::ok;
}

/* A pointer made visible before any shape is shown when the shape arrives. */
Status HardwareCursor::set_position(const DxgkargSetPointerPositionRecord &position)
{
    platform_.require_irql_at_most(Irql::passive, "HardwareCursor::set_position");
    if (!device_.constructed()) return Status::invalid_state;
    if (position.VidPnSourceId >= DisplaySources::source_count) return Status::invalid_parameter;
    VirtioGpuDevice &device = device_.object();
    const bool was_visible = visible_;
    visible_ = (position.Flags & flag_visible) != 0u;
    if (!visible_) return update(device, 0u);
    x_ = position.X;
    y_ = position.Y;
    if (!shape_set_) return Status::ok;
    if (!was_visible) return update(device, resource_id);
    uint8_t          request[GpuRequestWriter::max_request_size];
    GpuRequestWriter writer(request, sizeof request);
    static_cast<void>(writer.move_cursor(scanout, x_, y_));
    return device.send_cursor(request, writer.length());
}

}  // namespace av
