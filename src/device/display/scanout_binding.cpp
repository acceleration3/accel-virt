#include "device/display/scanout_binding.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "proto/virtio_gpu/gpu_rect.hpp"

namespace av {

ScanoutBinding::ScanoutBinding(Host &host, BlobTable &blobs, uint32_t head)
    : host_(host), blobs_(blobs), head_(head), blob_(0u), first_(host), second_(host),
      second_is_current_(false), shown_(false), frames_(0u)
{
    AV_SCOPE("ScanoutBinding::ScanoutBinding");
}

ScanoutBinding::~ScanoutBinding()
{
    AV_SCOPE("ScanoutBinding::~ScanoutBinding");
    hide();
}

GuestDmaBuf &ScanoutBinding::current()
{
    AV_SCOPE("ScanoutBinding::current");
    return second_is_current_ ? second_ : first_;
}

const GuestDmaBuf &ScanoutBinding::current() const
{
    AV_SCOPE("ScanoutBinding::current");
    return second_is_current_ ? second_ : first_;
}

GuestDmaBuf &ScanoutBinding::spare()
{
    AV_SCOPE("ScanoutBinding::spare");
    return second_is_current_ ? first_ : second_;
}

Status ScanoutBinding::show(uint32_t blob, const ScanoutFramebuffer &framebuffer)
{
    AV_SCOPE("ScanoutBinding::show");
    host_.require_main_loop("ScanoutBinding::show");
    const Status pinned = blobs_.pin(blob, framebuffer.end());
    if (pinned != Status::ok) return pinned;
    bool       kept = false;
    ScopeExit  unpin([this, blob, &kept] {
        if (!kept) blobs_.unpin(blob);
    });
    BlobWindow   pages;
    const Status windowed = blobs_.window(blob, pages);
    if (windowed != Status::ok) return windowed;
    const Status made = spare().make(pages, 0u, framebuffer.blob_size());
    if (made != Status::ok) return made;
    const Status shown = host_.show_scanout(head_, spare().handle(), framebuffer);
    if (shown != Status::ok) {
        spare().release();
        return shown;
    }
    if (shown_) {
        current().release();
        blobs_.unpin(blob_);
    }
    second_is_current_ = !second_is_current_;
    blob_ = blob;
    framebuffer_ = framebuffer;
    shown_ = true;
    frames_++;
    kept = true;
    return Status::ok;
}

void ScanoutBinding::hide()
{
    AV_SCOPE("ScanoutBinding::hide");
    host_.require_main_loop("ScanoutBinding::hide");
    if (!shown_) return;
    host_.hide_scanout(head_);
    current().release();
    blobs_.unpin(blob_);
    framebuffer_.forget();
    blob_ = 0u;
    shown_ = false;
}

Status ScanoutBinding::update(const GpuRect &flushed)
{
    AV_SCOPE("ScanoutBinding::update");
    host_.require_main_loop("ScanoutBinding::update");
    if (!shown_) return Status::invalid_state;
    if (!flushed.inside(framebuffer_.width(), framebuffer_.height())) return Status::invalid_parameter;
    uint32_t x = 0u;
    uint32_t y = 0u;
    uint32_t width = 0u;
    uint32_t height = 0u;
    if (!visible_part(flushed, x, y, width, height)) return Status::ok;
    host_.update_scanout(head_, x, y, width, height);
    return Status::ok;
}

bool ScanoutBinding::visible_part(const GpuRect &flushed, uint32_t &x, uint32_t &y, uint32_t &width,
                                  uint32_t &height) const
{
    AV_SCOPE("ScanoutBinding::visible_part");
    const GpuRect &picture = framebuffer_.rect();
    const uint32_t left = flushed.x() > picture.x() ? flushed.x() : picture.x();
    const uint32_t top = flushed.y() > picture.y() ? flushed.y() : picture.y();
    const uint32_t flushed_right = flushed.x() + flushed.width();
    const uint32_t picture_right = picture.x() + picture.width();
    const uint32_t flushed_bottom = flushed.y() + flushed.height();
    const uint32_t picture_bottom = picture.y() + picture.height();
    const uint32_t right = flushed_right < picture_right ? flushed_right : picture_right;
    const uint32_t bottom = flushed_bottom < picture_bottom ? flushed_bottom : picture_bottom;
    if (left >= right || top >= bottom) return false;
    x = left - picture.x();
    y = top - picture.y();
    width = right - left;
    height = bottom - top;
    return true;
}

bool ScanoutBinding::shows() const
{
    AV_SCOPE("ScanoutBinding::shows");
    return shown_;
}

uint32_t ScanoutBinding::head() const
{
    AV_SCOPE("ScanoutBinding::head");
    return head_;
}

uint32_t ScanoutBinding::blob() const
{
    AV_SCOPE("ScanoutBinding::blob");
    return blob_;
}

uint64_t ScanoutBinding::frames() const
{
    AV_SCOPE("ScanoutBinding::frames");
    return frames_;
}

const ScanoutFramebuffer &ScanoutBinding::framebuffer() const
{
    AV_SCOPE("ScanoutBinding::framebuffer");
    return framebuffer_;
}

void ScanoutBinding::report(StateWriter &writer) const
{
    AV_SCOPE("ScanoutBinding::report");
    writer.open("head", head_);
    writer.flag("shown", shown_);
    writer.number("blob", blob_);
    writer.number("width", framebuffer_.rect().width());
    writer.number("height", framebuffer_.rect().height());
    writer.number("stride", framebuffer_.stride());
    writer.number("offset", framebuffer_.offset());
    writer.hex("fourcc", framebuffer_.fourcc());
    writer.number("frames", frames_);
    writer.close();
}

bool ScanoutBinding::invariants_hold() const
{
    AV_SCOPE("ScanoutBinding::invariants_hold");
    if (!framebuffer_.invariants_hold() || !first_.invariants_hold() || !second_.invariants_hold())
        return false;
    if (first_.made() && second_.made()) return false;
    if (!shown_) return blob_ == 0u && !framebuffer_.adopted() && !first_.made() && !second_.made();
    return blob_ != 0u && framebuffer_.adopted() && current().made();
}

}  // namespace av
