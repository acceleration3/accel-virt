#include "core/display/scanout_blob_source.hpp"

#include "core/display/display_mode.hpp"
#include "core/platform/log_budget.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"
#include "proto/virtio_gpu/rvk_request_writer.hpp"

namespace av {

ScanoutBlobSource::ScanoutBlobSource(Platform &platform, LogBudget &budget,
                                     ObjectStorage<VirtioGpuDevice> &device, uint32_t source)
    : platform_(platform), budget_(budget), device_(device), source_(source), width_(0u), height_(0u),
      blob_address_(0u), blob_pages_(0u), reported_address_(0u), reported_(false), create_request_(nullptr),
      create_request_bytes_(0u), created_(false), scanning_out_(false)
      {
          AV_SCOPE("ScanoutBlobSource::ScanoutBlobSource");
      }

ScanoutBlobSource::~ScanoutBlobSource()
{
    AV_SCOPE("ScanoutBlobSource::~ScanoutBlobSource");
    if (create_request_ != nullptr) platform_.release(create_request_);
}

bool ScanoutBlobSource::needs_pages(uint64_t address) const
{
    AV_SCOPE("ScanoutBlobSource::needs_pages");
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutBlobSource::needs_pages");
    return address != 0u && device_.constructed() && width_ != 0u;
}

uint32_t ScanoutBlobSource::page_count() const
{
    AV_SCOPE("ScanoutBlobSource::page_count");
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutBlobSource::page_count");
    const uint64_t bytes = uint64_t{width_} * height_ * bytes_per_pixel;
    return static_cast<uint32_t>((bytes + GuestPageList::page_bytes - 1u) / GuestPageList::page_bytes);
}

Status ScanoutBlobSource::reserve()
{
    AV_SCOPE("ScanoutBlobSource::reserve");
    const size_t needed = RvkRequestWriter::guest_blob_fixed_size +
                          size_t{page_count()} * RvkRequestWriter::guest_blob_entry_size;
    if (needed <= create_request_bytes_) return Status::ok;
    if (create_request_ != nullptr) platform_.release(create_request_);
    create_request_ = static_cast<uint8_t *>(platform_.allocate(needed, Pool::non_paged));
    create_request_bytes_ = create_request_ != nullptr ? needed : 0u;
    return create_request_ != nullptr ? Status::ok : Status::no_memory;
}

/* The one request whose writer answer is not voided: a page list the driver
 * could not complete is a refusal, not a buffer that was too small. */
Status ScanoutBlobSource::create(VirtioGpuDevice &device, const GuestPageList &pages)
{
    AV_SCOPE("ScanoutBlobSource::create");
    RvkRequestWriter writer(create_request_, create_request_bytes_);
    const Status     written = writer.guest_blob_pages(first_resource_id + source_, pages);
    if (written != Status::ok) return written;
    return device.send_control(create_request_, writer.length());
}

Status ScanoutBlobSource::show(VirtioGpuDevice &device, uint32_t resource)
{
    AV_SCOPE("ScanoutBlobSource::show");
    uint8_t          bytes[GpuRequestWriter::scanout_blob_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    const uint32_t   width = resource != 0u ? width_ : 0u;
    const uint32_t   height = resource != 0u ? height_ : 0u;
    static_cast<void>(writer.set_scanout_blob(source_, resource, width, height,
                                              width * bytes_per_pixel));
    return device.send_control(bytes, writer.length());
}

/* No transfer, which is the whole point of this backing: the host's display
 * samples the guest's own pages, so a flush is the only thing a frame costs. */
Status ScanoutBlobSource::flush(VirtioGpuDevice &device)
{
    AV_SCOPE("ScanoutBlobSource::flush");
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    static_cast<void>(writer.resource_flush(first_resource_id + source_, width_, height_));
    return device.send_control(bytes, writer.length());
}

Status ScanoutBlobSource::unref()
{
    AV_SCOPE("ScanoutBlobSource::unref");
    if (!device_.constructed()) return Status::invalid_state;
    VirtioGpuDevice &device = device_.object();
    if (scanning_out_) {
        const Status hidden = show(device, 0u);
        if (hidden != Status::ok) return hidden;
        scanning_out_ = false;
    }
    blob_address_ = 0u;
    blob_pages_ = 0u;
    uint8_t          bytes[GpuRequestWriter::max_request_size] = {};
    GpuRequestWriter writer(bytes, sizeof bytes);
    static_cast<void>(writer.resource_unref(first_resource_id + source_));
    const Status freed = device.send_control(bytes, writer.length());
    created_ = false;
    return freed;
}

void ScanoutBlobSource::release()
{
    AV_SCOPE("ScanoutBlobSource::release");
    platform_.require_irql_at_most(Irql::passive, "ScanoutBlobSource::release");
    InvariantScope<ScanoutBlobSource> guard(platform_, *this, "release");
    if (created_) static_cast<void>(unref());
    width_ = 0u;
    height_ = 0u;
}

/* Nothing is sent here. A blob is made over the primary's pages and the mode
 * commit knows no primary; what the mode settles is how big the blob will be,
 * which is what the request buffer is sized from. */
Status ScanoutBlobSource::configure(const DisplayMode &mode)
{
    AV_SCOPE("ScanoutBlobSource::configure");
    platform_.require_irql_at_most(Irql::passive, "ScanoutBlobSource::configure");
    InvariantScope<ScanoutBlobSource> guard(platform_, *this, "configure");
    if (!device_.constructed()) return Status::invalid_state;
    if (width_ == mode.width() && height_ == mode.height()) return Status::ok;
    const Status freed = created_ ? unref() : Status::ok;
    if (freed != Status::ok) return freed;
    width_ = mode.width();
    height_ = mode.height();
    const Status room = reserve();
    if (room != Status::ok) {
        width_ = 0u;
        height_ = 0u;
    }
    return room;
}

Status ScanoutBlobSource::disable()
{
    AV_SCOPE("ScanoutBlobSource::disable");
    platform_.require_irql_at_most(Irql::passive, "ScanoutBlobSource::disable");
    InvariantScope<ScanoutBlobSource> guard(platform_, *this, "disable");
    if (!created_) return Status::ok;
    return unref();
}

void ScanoutBlobSource::report(uint64_t address, Status status)
{
    AV_SCOPE("ScanoutBlobSource::report");
    if (status == Status::ok) return;
    if (reported_ && address == reported_address_) return;
    reported_address_ = address;
    reported_ = true;
    if (!budget_.allows()) return;
    char       bytes[128];
    TextBuffer line(bytes, sizeof bytes);
    line.append("rvkgpu: scanout blob ");
    line.append_decimal(source_);
    line.append(" address 0x");
    line.append_hex(address, 16u);
    line.append(" status ");
    line.append_decimal(static_cast<uint64_t>(status));
    platform_.log(line.text());
}

void ScanoutBlobSource::forget_device()
{
    AV_SCOPE("ScanoutBlobSource::forget_device");
    platform_.require_irql_at_most(Irql::passive, "ScanoutBlobSource::forget_device");
    created_ = false;
    scanning_out_ = false;
    blob_address_ = 0u;
    blob_pages_ = 0u;
    width_ = 0u;
    height_ = 0u;
}

/* The address moves when dxgkrnl flips to another primary: the blob names the
 * pages of one allocation, so a new address is a new blob and the old one is
 * given back before it is made. */
Status ScanoutBlobSource::refresh(uint64_t address, const GuestPageList &pages, bool presented)
{
    AV_SCOPE("ScanoutBlobSource::refresh");
    platform_.require_irql_at_most(Irql::dispatch, "ScanoutBlobSource::refresh");
    InvariantScope<ScanoutBlobSource> guard(platform_, *this, "refresh");
    if (width_ == 0u || !device_.constructed()) return Status::ok;
    if (address == 0u) {
        report(address, Status::invalid_state);
        return Status::ok;
    }
    VirtioGpuDevice &device = device_.object();
    const uint64_t   fingerprint = pages.fingerprint();
    const bool       moved = pages.count() != 0u && fingerprint != blob_pages_;
    if (address != blob_address_ || moved) {
        if (created_) {
            const Status freed = unref();
            if (freed != Status::ok) return freed;
        }
        const Status made = create(device, pages);
        report(address, made);
        if (made != Status::ok) return made;
        created_ = true;
        const Status shown = show(device, first_resource_id + source_);
        if (shown != Status::ok) return shown;
        blob_address_ = address;
        blob_pages_ = fingerprint;
        scanning_out_ = true;
        return flush(device);
    }
    return presented ? flush(device) : Status::ok;
}

bool ScanoutBlobSource::invariants_hold() const
{
    AV_SCOPE("ScanoutBlobSource::invariants_hold");
    /* An address only with the blob that was made over it, nothing scanned out
     * that was not made, and nothing scanned out of no address. */
    const bool addressed = created_ || blob_address_ == 0u;
    return addressed && (created_ || !scanning_out_) && (!scanning_out_ || blob_address_ != 0u);
}

}  // namespace av
