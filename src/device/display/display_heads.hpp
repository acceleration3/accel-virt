/* A device's display heads: which blob each is showing, and the host display
 * they are shown on. A head shows a blob as a display surface over the
 * device's own mapping of its pages, which every display backend reads, GL or
 * not (docs/host-memory.md). */
#ifndef AV_DISPLAY_HEADS_HPP
#define AV_DISPLAY_HEADS_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/display/scanout_binding.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class GpuRect;
class Host;
class ScanoutFramebuffer;
class StateWriter;
struct DisplayHeadsProbe;

class DisplayHeads {
public:
    /* VIRTIO_GPU_MAX_SCANOUTS. */
    static constexpr uint32_t max_heads = 16u;

    DisplayHeads(Host &host, BlobTable &blobs);
    ~DisplayHeads();
    DisplayHeads(const DisplayHeads &) = delete;
    DisplayHeads &operator=(const DisplayHeads &) = delete;

    /* Main loop. Head `head` shows `framebuffer` out of blob `blob`.
     * invalid_parameter for a head past max_heads, and the binding's answer
     * otherwise: not_supported when the display has no such head. */
    [[nodiscard]] Status show(uint32_t head, uint32_t blob, const ScanoutFramebuffer &framebuffer);
    /* Main loop. Head `head` shows nothing. invalid_parameter past max_heads;
     * ok for a head that was already showing nothing. */
    [[nodiscard]] Status hide(uint32_t head);
    /* Main loop. `flushed` of blob `blob` changed: every head showing it is
     * updated. not_found when no head shows it, and the first head's refusal
     * when the rectangle is not inside the framebuffer. */
    [[nodiscard]] Status flush(uint32_t blob, const GpuRect &flushed);
    /* Main loop. Every head shows nothing; a device reset. */
    void                 release_all();
    /* Whether any head is showing blob `blob`, which is what keeps it from
     * being unreferenced. */
    bool                 shows(uint32_t blob) const;
    uint32_t             showing() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    /* The first refusal, and the first frame shown, once per device (FR-006):
     * a display with no such head is a thing about this host, and a guest
     * that keeps asking must not turn the log into the reason the run is
     * slow. */
    void                  say_once(bool &said, const char *what);
    /* Head `head`, made on first use; `head` must be below max_heads. */
    ScanoutBinding       &binding(uint32_t head);
    /* Head `head` if it has ever been used, else null. */
    const ScanoutBinding *made(uint32_t head) const;

    Host                            &host_;
    BlobTable                       &blobs_;
    bool                             said_no_display_;
    bool                             said_showing_;
    ObjectStorage<ScanoutBinding>    heads_[max_heads];

    friend struct DisplayHeadsProbe;
};

}  // namespace av

#endif  // AV_DISPLAY_HEADS_HPP
