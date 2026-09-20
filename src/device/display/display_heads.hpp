/* A device's display heads: which blob each is showing, and the host display
 * they are shown on.
 *
 * Whether the host can show a dma-buf at all is asked once and remembered,
 * because QEMU registers its display backends after every device has realized
 * (system/vl.c: qemu_init_displays runs after qmp_x_exit_preconfig) and it is
 * the backend that gives a console its GL context, so the question has no
 * useful answer until the guest is running. Until it is asked the heads show
 * nothing and the capset carries no scanout bit. */
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

    /* Main loop. Whether any head can show a dma-buf, asked of the host the
     * first time and remembered. */
    bool                 takes_dmabuf();
    /* Main loop. Head `head` shows `framebuffer` out of blob `blob`.
     * invalid_parameter for a head past max_heads, not_supported when the
     * display will not show a dma-buf or has no such head, and the binding's
     * answer otherwise. */
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
    enum class Display : uint32_t { unasked, takes_dmabuf, refuses_dmabuf };

    /* Head `head`, made on first use; `head` must be below max_heads. */
    /* The first refusal of each kind, once per device (FR-006): a display that
     * will not show a dma-buf and a blob whose memory cannot become one are
     * both things about this host, and a guest that keeps asking must not turn
     * the log into the reason the run is slow. */
    void                  say_once(bool &said, const char *what);
    ScanoutBinding       &binding(uint32_t head);
    /* Head `head` if it has ever been used, else null. */
    const ScanoutBinding *made(uint32_t head) const;

    Host                            &host_;
    BlobTable                       &blobs_;
    Display                          display_;
    bool                             said_no_display_;
    bool                             said_no_descriptor_;
    bool                             said_showing_;
    ObjectStorage<ScanoutBinding>    heads_[max_heads];

    friend struct DisplayHeadsProbe;
};

}  // namespace av

#endif  // AV_DISPLAY_HEADS_HPP
