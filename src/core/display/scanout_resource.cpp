#include "core/display/scanout_resource.hpp"

#include "core/display/display_mode.hpp"
#include "core/platform/log_budget.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"

namespace av {

ScanoutResource::ScanoutResource(Platform &platform, LogBudget &budget, ObjectStorage<VirtioGpuDevice> &device,
                                 uint32_t source)
    : platform_(platform), budget_(budget), device_(device), source_(source), width_(0u), height_(0u),
      attached_address_(0u), attached_pages_(0u), key_(0u), posted_frame_(0u), reported_address_(0u), reported_(false),
      backing_request_(platform, GpuRequestWriter::backing_header_size, GpuRequestWriter::memory_entry_size),
      created_(false), scanning_out_(false), backed_(false)
      {
          AV_SCOPE("ScanoutResource::ScanoutResource");
      }

bool ScanoutResource::needs_pages(uint64_t address) const
{
    AV_SCOPE("ScanoutResource::needs_pages");
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutResource::needs_pages");
    return created_ && address != 0u && device_.constructed();
}

bool ScanoutResource::keyed() const
{
    AV_SCOPE("ScanoutResource::keyed");
    return key_ != 0u;
}

bool ScanoutResource::posts_frames() const
{
    AV_SCOPE("ScanoutResource::posts_frames");
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutResource::posts_frames");
    return key_ != 0u && scanning_out_;
}

bool ScanoutResource::scanning_out() const
{
    AV_SCOPE("ScanoutResource::scanning_out");
    return scanning_out_;
}

uint32_t ScanoutResource::page_count() const
{
    AV_SCOPE("ScanoutResource::page_count");
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutResource::page_count");
    return DisplayMode(width_, height_).frame_pages();
}

/* Every request below fits a max_request_size buffer, so the writer cannot
 * refuse one and its answer is voided as the cursor's is; only the queue can
 * refuse, and that answer is returned. */
Status ScanoutResource::create(VirtioGpuDevice &device)
{
    AV_SCOPE("ScanoutResource::create");
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    static_cast<void>(writer.create_2d(first_resource_id + source_, GpuRequestWriter::format_b8g8r8a8, width_,
                                       height_));
    return device.send_control(bytes, writer.length());
}

/* The one request whose writer answer is not voided: a page list the driver
 * could not complete is a refusal, not a buffer that was too small. */
Status ScanoutResource::attach(VirtioGpuDevice &device, const GuestPageList &pages)
{
    AV_SCOPE("ScanoutResource::attach");
    GpuRequestWriter writer(backing_request_.start(), backing_request_.size());
    const Status     written = writer.attach_backing_pages(first_resource_id + source_, pages);
    if (written != Status::ok) return written;
    const Status attached = device.send_control(backing_request_.start(), writer.length());
    backed_ = attached == Status::ok;
    return attached;
}

Status ScanoutResource::detach(VirtioGpuDevice &device)
{
    AV_SCOPE("ScanoutResource::detach");
    if (!backed_) return Status::ok;
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    static_cast<void>(writer.detach_backing(first_resource_id + source_));
    const Status detached = device.send_control(bytes, writer.length());
    if (detached == Status::ok) backed_ = false;
    return detached;
}

Status ScanoutResource::show(VirtioGpuDevice &device, uint32_t resource)
{
    AV_SCOPE("ScanoutResource::show");
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    const uint32_t   width = resource != 0u ? width_ : 0u;
    const uint32_t   height = resource != 0u ? height_ : 0u;
    static_cast<void>(writer.set_scanout(source_, resource, width, height));
    return device.send_control(bytes, writer.length());
}

/* TRANSFER_TO_HOST_2D carries the guest's pixels to the host's copy of the
 * resource; RESOURCE_FLUSH is what puts them on the screen. */
Status ScanoutResource::flush(VirtioGpuDevice &device, uint32_t frame)
{
    AV_SCOPE("ScanoutResource::flush");
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    if (key_ != 0u) static_cast<void>(writer.scanout_image(first_resource_id + source_, key_, frame));
    else static_cast<void>(writer.transfer_to_host_2d(first_resource_id + source_, width_, height_));
    const Status transferred = device.send_control(bytes, writer.length());
    if (transferred != Status::ok) return transferred;
    static_cast<void>(writer.resource_flush(first_resource_id + source_, width_, height_));
    return device.send_control(bytes, writer.length());
}

Status ScanoutResource::unref()
{
    AV_SCOPE("ScanoutResource::unref");
    if (!device_.constructed()) return Status::invalid_state;
    VirtioGpuDevice &device = device_.object();
    if (scanning_out_) {
        const Status hidden = show(device, 0u);
        if (hidden != Status::ok) return hidden;
        scanning_out_ = false;
    }
    attached_address_ = 0u;
    attached_pages_ = 0u;
    backed_ = false;
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
    AV_SCOPE("ScanoutResource::release");
    platform_.require_irql_at_most(Irql::passive, "ScanoutResource::release");
    InvariantScope<ScanoutResource> guard(platform_, *this, "release");
    if (created_) static_cast<void>(unref());
}

Status ScanoutResource::prepare(const DisplayMode &mode)
{
    AV_SCOPE("ScanoutResource::prepare");
    platform_.require_irql_at_most(Irql::passive, "ScanoutResource::prepare");
    return backing_request_.prepare(mode.frame_pages());
}

Status ScanoutResource::configure(const DisplayMode &mode)
{
    AV_SCOPE("ScanoutResource::configure");
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutResource::configure");
    InvariantScope<ScanoutResource> guard(platform_, *this, "configure");
    if (!device_.constructed()) return Status::invalid_state;
    if (created_ && width_ == mode.width() && height_ == mode.height()) return Status::ok;
    const Status freed = created_ ? unref() : Status::ok;
    if (freed != Status::ok) return freed;
    const Status room = backing_request_.take(mode.frame_pages());
    if (room != Status::ok) return room;
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
    AV_SCOPE("ScanoutResource::disable");
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutResource::disable");
    InvariantScope<ScanoutResource> guard(platform_, *this, "disable");
    if (!scanning_out_) return Status::ok;
    const Status hidden = show(device_.object(), 0u);
    scanning_out_ = false;
    attached_address_ = 0u;
    return hidden;
}

void ScanoutResource::report(uint64_t address, const GuestPageList &pages, Status status)
{
    AV_SCOPE("ScanoutResource::report");
    if (status == Status::ok && pages.count() == page_count()) return;
    if (reported_ && address == reported_address_) return;
    reported_address_ = address;
    reported_ = true;
    if (!budget_.allows()) return;
    char       bytes[128];
    TextBuffer line(bytes, sizeof bytes);
    line.append("rvkgpu: scanout ");
    line.append_decimal(source_);
    line.append(" address 0x");
    line.append_hex(address, 16u);
    line.append(" pages ");
    line.append_decimal(pages.count());
    line.append(" runs ");
    line.append_decimal(pages.runs());
    line.append(" needs ");
    line.append_decimal(page_count());
    line.append(" status ");
    line.append_decimal(static_cast<uint64_t>(status));
    platform_.log(line.text());
}

bool ScanoutResource::invariants_hold() const
{
    AV_SCOPE("ScanoutResource::invariants_hold");
    const bool sized = created_ == (width_ != 0u && height_ != 0u);
    return sized && (created_ || !scanning_out_) && (scanning_out_ || attached_address_ == 0u);
}

void ScanoutResource::forget_device()
{
    AV_SCOPE("ScanoutResource::forget_device");
    platform_.require_irql_at_most(Irql::passive, "ScanoutResource::forget_device");
    created_ = false;
    scanning_out_ = false;
    backed_ = false;
    attached_address_ = 0u;
    attached_pages_ = 0u;
    width_ = 0u;
    height_ = 0u;
}

Status ScanoutResource::refresh(uint64_t address, const GuestPageList &pages, bool presented, uint64_t key,
                                uint32_t frame)
{
    AV_SCOPE("ScanoutResource::refresh");
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutResource::refresh");
    InvariantScope<ScanoutResource> guard(platform_, *this, "refresh");
    if (!created_ || !device_.constructed()) return Status::ok;
    /* dxgkrnl has given the source no primary to show yet. */
    if (address == 0u) {
        report(address, pages, Status::invalid_state);
        return Status::ok;
    }
    VirtioGpuDevice &device = device_.object();
    key_ = key;
    /* A keyed image is read from the host's own copy, never the pages: once
     * shown, a flip between keyed images is a new key and a flush, not a
     * detach and attach of a thousand pages on QEMU's main loop. */
    if (key != 0u && scanning_out_) {
        attached_address_ = address;
        if (frame != posted_frame_) {
            posted_frame_ = frame;
            return flush(device, frame);
        }
        return presented ? flush(device, 0u) : Status::ok;
    }
    const uint64_t   fingerprint = pages.fingerprint();
    const bool       moved = pages.count() != 0u && fingerprint != attached_pages_;
    if (address != attached_address_ || moved) {
        const Status detached = detach(device);
        if (detached != Status::ok) return detached;
        const Status attached = attach(device, pages);
        report(address, pages, attached);
        if (attached != Status::ok) return attached;
        const Status shown = show(device, first_resource_id + source_);
        if (shown != Status::ok) return shown;
        attached_address_ = address;
        attached_pages_ = fingerprint;
        scanning_out_ = true;
        posted_frame_ = key != 0u ? frame : 0u;
        return flush(device, posted_frame_);
    }
    return presented ? flush(device, 0u) : Status::ok;
}

}  // namespace av
