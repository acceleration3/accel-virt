/* One display head: the blob it shows, the framebuffer inside that blob, and
 * the dma-buf the host display samples it through.
 *
 * The descriptor covers the whole blob rather than the framebuffer, because a
 * dma-buf is made of whole pages and a framebuffer inside a blob is not
 * page-aligned in general; the framebuffer's offset is the plane's offset
 * instead. The blob is pinned while it is shown, which is what makes the
 * descriptor safe and what makes RESOURCE_UNREF of a blob on screen a
 * refusal rather than a dangling import.
 *
 * There are two descriptor slots because a head changes what it shows without
 * going dark: the new descriptor is made and the new pin taken before the host
 * is told, and the old ones are let go only once it has taken them, so a
 * refused rebind leaves the head showing exactly what it showed. */
#ifndef AV_SCANOUT_BINDING_HPP
#define AV_SCANOUT_BINDING_HPP

#include "core/support/status.hpp"
#include "device/display/scanout_framebuffer.hpp"
#include "device/memory/guest_dma_buf.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class GpuRect;
class Host;
class StateWriter;
struct DisplayHeadsProbe;

class ScanoutBinding {
public:
    ScanoutBinding(Host &host, BlobTable &blobs, uint32_t head);
    ~ScanoutBinding();
    ScanoutBinding(const ScanoutBinding &) = delete;
    ScanoutBinding &operator=(const ScanoutBinding &) = delete;

    /* Main loop. This head shows `framebuffer` out of blob `blob`. The blob's
     * answer when it cannot be pinned or its pages cannot become a dma-buf,
     * not_supported when the display would not take it; anything but ok leaves
     * the head showing what it showed. */
    [[nodiscard]] Status show(uint32_t blob, const ScanoutFramebuffer &framebuffer);
    /* Main loop. Shows nothing; safe on a head already showing nothing. */
    void                 hide();
    /* Main loop. `flushed`, a rectangle of the framebuffer, reached the head:
     * the part of it the head is showing is updated. invalid_state when the
     * head shows nothing, invalid_parameter for a rectangle that is not inside
     * the framebuffer. A rectangle inside the framebuffer but outside the
     * picture updates nothing and answers ok. */
    [[nodiscard]] Status update(const GpuRect &flushed);

    bool     shows() const;
    uint32_t head() const;
    uint32_t blob() const;
    /* Frames this head has been shown, which is what a state dump reports. */
    uint64_t frames() const;
    const ScanoutFramebuffer &framebuffer() const;
    void                      report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    GuestDmaBuf       &current();
    const GuestDmaBuf &current() const;
    GuestDmaBuf       &spare();
    /* The part of `flushed` the head is showing, in the picture's own
     * coordinates; false when none of it is. */
    bool visible_part(const GpuRect &flushed, uint32_t &x, uint32_t &y, uint32_t &width,
                      uint32_t &height) const;

    Host              &host_;
    BlobTable         &blobs_;
    uint32_t           head_;
    uint32_t           blob_;
    ScanoutFramebuffer framebuffer_;
    GuestDmaBuf        first_;
    GuestDmaBuf        second_;
    bool               second_is_current_;
    bool               shown_;
    uint64_t           frames_;

    friend struct DisplayHeadsProbe;
};

}  // namespace av

#endif  // AV_SCANOUT_BINDING_HPP
