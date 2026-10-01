/* One surface as one present left it, copied by the API that drew it into
 * memory the CPU reads, in that API's own command order
 * (docs/shared-surfaces.md, "Readers"). Each host API stages through its own
 * kind - Direct3D 11 a staging texture, Direct3D 12 a readback buffer - from a
 * pool of its own; SharedSurfaces keeps them by surface and point until the
 * present engine has read them, and gives each back to its pool.
 *
 * Virtual because each API's is a real implementation, and the mock one is
 * what makes SharedSurfaces testable without a GPU. */
#ifndef AV_STAGED_FRAME_HPP
#define AV_STAGED_FRAME_HPP

#include "device/abi/rvk_executor.h"

namespace av {

class StagedFrame {
public:
    enum class Reading { mapped, copying, failed };

    StagedFrame(const StagedFrame &) = delete;
    StagedFrame &operator=(const StagedFrame &) = delete;

    /* Mapped into `view` once the GPU has copied it, and copying until then;
     * failed when it never will be. Any thread, never two at once, and never
     * with the lock of SharedSurfaces held: it may take its API's own. */
    virtual Reading read(RvkFrameView &view) = 0;
    /* Unmapped if it was, and back in its pool for another frame. Any thread,
     * never with the lock of SharedSurfaces held. */
    virtual void    release() = 0;

protected:
    StagedFrame() = default;
    /* Not virtual: a frame is given back to its pool, never destroyed through
     * this (CLAUDE.md, Adaptations). */
    ~StagedFrame() = default;
};

}  // namespace av

#endif  // AV_STAGED_FRAME_HPP
