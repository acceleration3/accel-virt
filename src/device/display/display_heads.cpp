#include "device/display/display_heads.hpp"

#include "core/trace/function_scope.hpp"
#include "device/display/scanout_framebuffer.hpp"
#include "device/host/host.hpp"
#include "proto/virtio_gpu/gpu_rect.hpp"

namespace av {

DisplayHeads::DisplayHeads(Host &host, BlobTable &blobs)
    : host_(host), blobs_(blobs), display_(Display::unasked)
{
    AV_SCOPE("DisplayHeads::DisplayHeads");
}

DisplayHeads::~DisplayHeads()
{
    AV_SCOPE("DisplayHeads::~DisplayHeads");
    release_all();
}

bool DisplayHeads::takes_dmabuf()
{
    AV_SCOPE("DisplayHeads::takes_dmabuf");
    host_.require_main_loop("DisplayHeads::takes_dmabuf");
    if (display_ == Display::unasked)
        display_ = host_.display_takes_dmabuf(0u) ? Display::takes_dmabuf : Display::refuses_dmabuf;
    return display_ == Display::takes_dmabuf;
}

/* A binding acquires nothing at construction, so the only way construct
 * refuses is a slot already made, which is what the guard asks. */
ScanoutBinding &DisplayHeads::binding(uint32_t head)
{
    AV_SCOPE("DisplayHeads::binding");
    if (!heads_[head].constructed()) static_cast<void>(heads_[head].construct(host_, blobs_, head));
    return heads_[head].object();
}

const ScanoutBinding *DisplayHeads::made(uint32_t head) const
{
    AV_SCOPE("DisplayHeads::made");
    return heads_[head].constructed() ? &heads_[head].object() : nullptr;
}

Status DisplayHeads::show(uint32_t head, uint32_t blob, const ScanoutFramebuffer &framebuffer)
{
    AV_SCOPE("DisplayHeads::show");
    host_.require_main_loop("DisplayHeads::show");
    if (head >= max_heads) return Status::invalid_parameter;
    if (!takes_dmabuf()) return Status::not_supported;
    if (!host_.display_takes_dmabuf(head)) return Status::not_supported;
    return binding(head).show(blob, framebuffer);
}

Status DisplayHeads::hide(uint32_t head)
{
    AV_SCOPE("DisplayHeads::hide");
    host_.require_main_loop("DisplayHeads::hide");
    if (head >= max_heads) return Status::invalid_parameter;
    if (heads_[head].constructed()) heads_[head].object().hide();
    return Status::ok;
}

Status DisplayHeads::flush(uint32_t blob, const GpuRect &flushed)
{
    AV_SCOPE("DisplayHeads::flush");
    host_.require_main_loop("DisplayHeads::flush");
    Status answered = Status::not_found;
    for (ObjectStorage<ScanoutBinding> &slot : heads_) {
        if (!slot.constructed() || !slot.object().shows() || slot.object().blob() != blob) continue;
        const Status updated = slot.object().update(flushed);
        if (updated != Status::ok) return updated;
        answered = Status::ok;
    }
    return answered;
}

void DisplayHeads::release_all()
{
    AV_SCOPE("DisplayHeads::release_all");
    for (ObjectStorage<ScanoutBinding> &slot : heads_) slot.destroy();
}

bool DisplayHeads::shows(uint32_t blob) const
{
    AV_SCOPE("DisplayHeads::shows");
    for (const ObjectStorage<ScanoutBinding> &slot : heads_)
        if (slot.constructed() && slot.object().shows() && slot.object().blob() == blob) return true;
    return false;
}

uint32_t DisplayHeads::showing() const
{
    AV_SCOPE("DisplayHeads::showing");
    uint32_t heads = 0u;
    for (const ObjectStorage<ScanoutBinding> &slot : heads_)
        if (slot.constructed() && slot.object().shows()) heads++;
    return heads;
}

void DisplayHeads::report(StateWriter &writer) const
{
    AV_SCOPE("DisplayHeads::report");
    for (uint32_t head = 0u; head < max_heads; head++) {
        const ScanoutBinding *const made_head = made(head);
        if (made_head != nullptr) made_head->report(writer);
    }
}

bool DisplayHeads::invariants_hold() const
{
    AV_SCOPE("DisplayHeads::invariants_hold");
    for (uint32_t head = 0u; head < max_heads; head++) {
        const ScanoutBinding *const made_head = made(head);
        if (made_head == nullptr) continue;
        if (!made_head->invariants_hold() || made_head->head() != head) return false;
        if (made_head->shows() && display_ != Display::takes_dmabuf) return false;
    }
    return true;
}

}  // namespace av
