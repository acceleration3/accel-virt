#include "core/display/scanout_resource.hpp"

#include "core/display/display_mode.hpp"
#include "core/platform/log_budget.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/text_buffer.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"

namespace av {

ScanoutResource::ScanoutResource(Platform &platform, LogBudget &budget, ObjectStorage<VirtioGpuDevice> &device,
                                 uint32_t source)
    : platform_(platform), budget_(budget), device_(device), source_(source), width_(0u), height_(0u),
      attached_address_(0u), reported_address_(0u), reported_(false), backing_request_(nullptr), backing_request_bytes_(0u), created_(false), scanning_out_(false) {}

ScanoutResource::~ScanoutResource()
{
    if (backing_request_ != nullptr) platform_.release(backing_request_);
}

bool ScanoutResource::needs_pages(uint64_t address) const
{
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutResource::needs_pages");
    return created_ && address != 0u && address != attached_address_ && device_.constructed();
}

uint32_t ScanoutResource::page_count() const
{
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutResource::page_count");
    const uint64_t bytes = uint64_t{width_} * height_ * bytes_per_pixel;
    return static_cast<uint32_t>((bytes + GuestPageList::page_bytes - 1u) / GuestPageList::page_bytes);
}

Status ScanoutResource::reserve()
{
    const size_t needed = GpuRequestWriter::backing_header_size +
                          size_t{page_count()} * GpuRequestWriter::memory_entry_size;
    if (needed <= backing_request_bytes_) return Status::ok;
    if (backing_request_ != nullptr) platform_.release(backing_request_);
    backing_request_ = static_cast<uint8_t *>(platform_.allocate(needed, Pool::non_paged));
    backing_request_bytes_ = backing_request_ != nullptr ? needed : 0u;
    return backing_request_ != nullptr ? Status::ok : Status::no_memory;
}

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

/* The one request whose writer answer is not voided: a page list the driver
 * could not complete is a refusal, not a buffer that was too small. */
Status ScanoutResource::attach(VirtioGpuDevice &device, const GuestPageList &pages)
{
    GpuRequestWriter writer(backing_request_, backing_request_bytes_);
    const Status     written = writer.attach_backing_pages(first_resource_id + source_, pages);
    if (written != Status::ok) return written;
    return device.send_control(backing_request_, writer.length());
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
    const Status room = reserve();
    if (room != Status::ok) {
        width_ = 0u;
        height_ = 0u;
        return room;
    }
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

void ScanoutResource::report(uint64_t address, const GuestPageList &pages, Status status)
{
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

Status ScanoutResource::refresh(uint64_t address, const GuestPageList &pages, bool presented)
{
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutResource::refresh");
    InvariantScope<ScanoutResource> guard(platform_, *this, "refresh");
    if (!created_ || !device_.constructed()) return Status::ok;
    /* dxgkrnl has given the source no primary to show yet. */
    if (address == 0u) {
        report(address, pages, Status::invalid_state);
        return Status::ok;
    }
    VirtioGpuDevice &device = device_.object();
    if (address != attached_address_) {
        const Status attached = attach(device, pages);
        report(address, pages, attached);
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
