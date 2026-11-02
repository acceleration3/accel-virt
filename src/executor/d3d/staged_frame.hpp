/* One surface as one present left it, copied by the API that drew it into
 * memory the CPU reads, in that API's own command order
 * (docs/shared-surfaces.md, "Readers"). Each host API stages through its own
 * kind - Direct3D 9 an offscreen surface, Direct3D 11 a staging texture,
 * Direct3D 12 a readback buffer - from a pool of its own (FramePool);
 * SharedSurfaces keeps them by surface and point until the present engine has
 * read them, and gives each back to its pool.
 *
 * What every API's frame does alike is here: the claim that has it out, its
 * size, what a read fills in, and the verdict on a frame made on a device
 * that was lost since. What differs - mapping the copy once the GPU has made
 * it, and unmapping it - is virtual: each API's is a real implementation, and
 * the mock one makes SharedSurfaces testable without a GPU.
 *
 * A frame whose API stages in a Vulkan image can also be copied by the GPU
 * straight into memory the host lent (copy): a hook only those APIs override,
 * the others declining so the present reads their copy instead. */
#ifndef AV_STAGED_FRAME_HPP
#define AV_STAGED_FRAME_HPP

#include "device/abi/av_executor.h"
#include "executor/d3d/frame_claim.hpp"

#include <stdint.h>

namespace av {

class StagedFrame {
public:
    enum class Reading { mapped, copying, failed };
    /* copied: the bytes are in the lent memory. declined: this frame cannot be
     * copied that way, and read still gives it. timed_out: the GPU never
     * finished, or the frame is not readable at all. */
    enum class Copying { copied, declined, timed_out };

    StagedFrame(const StagedFrame &) = delete;
    StagedFrame &operator=(const StagedFrame &) = delete;

    /* Free to out for a present (FramePool); false when it was out. */
    [[nodiscard]] bool claim();
    bool               out() const;
    /* Mapped into `view` once the GPU has copied it, and copying until then;
     * failed when it never will be - a frame forsaken among them. Any thread,
     * never two at once, and never with the lock of SharedSurfaces held: it
     * may take its API's own. */
    Reading            read(AvFrameView &view);
    /* `target`'s regions of the frame copied on the GPU into the lent memory
     * it names, done within `wait_ms`. Any thread, never two at once, and
     * never with the lock of SharedSurfaces held, as read. */
    Copying            copy(const AvFrameCopy &target, uint32_t wait_ms);
    /* Unmapped if it was, and back in its pool for another frame. Any thread,
     * never with the lock of SharedSurfaces held. */
    void               release();
    /* Made on a device that was lost: unreadable from now on, until its pool
     * makes it again. Any thread: the one renewing the device. */
    void               forsake();

protected:
    StagedFrame();
    /* Not virtual: a frame is given back to its pool, never destroyed through
     * this (CLAUDE.md, Adaptations). */
    ~StagedFrame() = default;

    /* The size of the image the frame now holds, which a read answers. */
    void     size(uint32_t width, uint32_t height);
    uint32_t width() const;
    uint32_t height() const;
    bool     forsaken() const;
    /* Made again on the device now open: readable once more. */
    void     readmit();

    /* The copy's pixels and pitch once the GPU has made it; copying until
     * then, failed when it never will. */
    virtual Reading map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch) = 0;
    /* The copy unmapped, if map_when_copied mapped it. */
    virtual void    unmap() = 0;
    /* The copy for copy(); declines unless an API overrides it. */
    virtual Copying copy_into(const AvFrameCopy &target, uint32_t wait_ms);

private:
    FrameClaim claim_;
    uint32_t   width_;
    uint32_t   height_;
    /* Written by the thread that renews the device, read by the engine's. */
    bool       forsaken_;
};

}  // namespace av

#endif  // AV_STAGED_FRAME_HPP
