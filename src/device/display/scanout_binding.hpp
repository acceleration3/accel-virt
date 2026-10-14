/* One display head: the blob it shows and the framebuffer inside that blob,
 * shown as a display surface over the device's own mapping of the blob's
 * pages (BlobTable::view), so every display backend shows it and no frame is
 * copied on the way. The blob is pinned while it is shown, which is what
 * keeps that mapping valid and what makes RESOURCE_UNREF of a blob on screen
 * a refusal rather than a surface over freed pages. A head changes what it
 * shows without going dark: the new blob is pinned before the host is told,
 * and the old one let go only once it has taken the new one, so a refused
 * rebind leaves the head showing exactly what it showed. */
#ifndef AV_SCANOUT_BINDING_HPP
#define AV_SCANOUT_BINDING_HPP

#include "core/support/status.hpp"
#include "device/display/scanout_framebuffer.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class GpuRect;
class Host;
class StateWriter;
struct DisplayHeadsProbe;
struct ScanoutBindingProbe;

class ScanoutBinding {
public:
    ScanoutBinding(Host &host, BlobTable &blobs, uint32_t head);
    ~ScanoutBinding();
    ScanoutBinding(const ScanoutBinding &) = delete;
    ScanoutBinding &operator=(const ScanoutBinding &) = delete;

    /* Main loop. This head shows `framebuffer` out of blob `blob`. The blob's
     * answer when it cannot be pinned or its pages mapped as one range,
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
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    /* The part of `flushed` the head is showing, in the picture's own
     * coordinates; false when none of it is. */
    bool visible_part(const GpuRect &flushed, uint32_t &x, uint32_t &y, uint32_t &width,
                      uint32_t &height) const;

    Host              &host_;
    BlobTable         &blobs_;
    uint32_t           head_;
    uint32_t           blob_;
    ScanoutFramebuffer framebuffer_;
    bool               shown_;
    uint64_t           frames_;

    friend struct DisplayHeadsProbe;
    friend struct ScanoutBindingProbe;
struct ScanoutBindingProbe;
};

}  // namespace av

#endif  // AV_SCANOUT_BINDING_HPP
