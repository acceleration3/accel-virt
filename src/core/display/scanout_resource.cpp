#include "core/display/scanout_resource.hpp"

#include "core/display/display_mode.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"

namespace av {

ScanoutResource::ScanoutResource(Platform &platform, ObjectStorage<VirtioGpuDevice> &device, uint32_t source)
    : platform_(platform), device_(device), source_(source), width_(0u), height_(0u), attached_address_(0u),
      created_(false), scanning_out_(false) {}

/* Every request below fits a max_request_size buffer, so the writer cannot
 * refuse one and its answer is voided as the cursor's is; only the queue can
 * refuse, and that answer is returned. */
Status ScanoutResource::create(VirtioGpuDevice &device)
{
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    static_cast<void>(writer.create_2d(first_resource_id + source_, GpuRequestWriter::format_b8g8r8a8, width_,
                                       height_));
    return device.send_control(bytes, writer.length());
}

Status ScanoutResource::attach(VirtioGpuDevice &device, uint64_t address)
{
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    static_cast<void>(writer.attach_backing(first_resource_id + source_, address,
                                            width_ * height_ * bytes_per_pixel));
    return device.send_control(bytes, writer.length());
}

Status ScanoutResource::show(VirtioGpuDevice &device, uint32_t resource)
{
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    const uint32_t   width = resource != 0u ? width_ : 0u;
    const uint32_t   height = resource != 0u ? height_ : 0u;
    static_cast<void>(writer.set_scanout(source_, resource, width, height));
    return device.send_control(bytes, writer.length());
}

/* TRANSFER_TO_HOST_2D carries the guest's pixels to the host's copy of the
 * resource; RESOURCE_FLUSH is what puts them on the screen. */
Status ScanoutResource::flush(VirtioGpuDevice &device)
{
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    static_cast<void>(writer.transfer_to_host_2d(first_resource_id + source_, width_, height_));
    const Status transferred = device.send_control(bytes, writer.length());
    if (transferred != Status::ok) return transferred;
    static_cast<void>(writer.resource_flush(first_resource_id + source_, width_, height_));
    return device.send_control(bytes, writer.length());
}

Status ScanoutResource::unref()
{
    if (!device_.constructed()) return Status::invalid_state;
    VirtioGpuDevice &device = device_.object();
    if (scanning_out_) {
        const Status hidden = show(device, 0u);
        if (hidden != Status::ok) return hidden;
        scanning_out_ = false;
    }
    attached_address_ = 0u;
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    static_cast<void>(writer.resource_unref(first_resource_id + source_));
    const Status freed = device.send_control(bytes, writer.length());
    created_ = false;
    width_ = 0u;
    height_ = 0u;
    return freed;
}

void ScanoutResource::release()
{
    platform_.require_irql_at_most(Irql::passive, "ScanoutResource::release");
    InvariantScope<ScanoutResource> guard(platform_, *this, "release");
    if (created_) static_cast<void>(unref());
}

Status ScanoutResource::configure(const DisplayMode &mode)
{
    platform_.require_irql_at_most(Irql::passive, "ScanoutResource::configure");
    InvariantScope<ScanoutResource> guard(platform_, *this, "configure");
    if (!device_.constructed()) return Status::invalid_state;
    if (created_ && width_ == mode.width() && height_ == mode.height()) return Status::ok;
    const Status freed = created_ ? unref() : Status::ok;
    if (freed != Status::ok) return freed;
    width_ = mode.width();
    height_ = mode.height();
    const Status made = create(device_.object());
    created_ = made == Status::ok;
    if (!created_) {
        /* No resource, so no size: a refused create leaves nothing behind. */
        width_ = 0u;
        height_ = 0u;
    }
    return made;
}

Status ScanoutResource::disable()
{
    platform_.require_irql_at_most(Irql::passive, "ScanoutResource::disable");
    InvariantScope<ScanoutResource> guard(platform_, *this, "disable");
    if (!scanning_out_) return Status::ok;
    const Status hidden = show(device_.object(), 0u);
    scanning_out_ = false;
    attached_address_ = 0u;
    return hidden;
}

bool ScanoutResource::invariants_hold() const
{
    const bool sized = created_ == (width_ != 0u && height_ != 0u);
    return sized && (created_ || !scanning_out_) && (scanning_out_ || attached_address_ == 0u);
}

void ScanoutResource::forget_device()
{
    platform_.require_irql_at_most(Irql::passive, "ScanoutResource::forget_device");
    created_ = false;
    scanning_out_ = false;
    attached_address_ = 0u;
    width_ = 0u;
    height_ = 0u;
}

Status ScanoutResource::refresh(uint64_t address, bool presented)
{
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutResource::refresh");
    InvariantScope<ScanoutResource> guard(platform_, *this, "refresh");
    if (!created_ || address == 0u || !device_.constructed()) return Status::ok;
    VirtioGpuDevice &device = device_.object();
    if (address != attached_address_) {
        const Status attached = attach(device, address);
        if (attached != Status::ok) return attached;
        const Status shown = show(device, first_resource_id + source_);
        if (shown != Status::ok) return shown;
        attached_address_ = address;
        scanning_out_ = true;
        return flush(device);
    }
    return presented ? flush(device) : Status::ok;
}

}  // namespace av
