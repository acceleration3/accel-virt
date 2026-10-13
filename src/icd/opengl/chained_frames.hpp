/* One window's frames as the compositor's surfaces rather than as pixels
 * (docs/shared-surfaces.md, step 5): the slots of the window's DXGI chain
 * (icd::WindowChains), each opened in this context over the slot texture's
 * memory (feature 053) as a texture of its own.
 *
 * A swap copies its frame into a slot as it is stored - bottom row first,
 * which the chain turns the right way up as it presents (WindowChains) - and
 * sends a glFinish one-way, so the batch the present thread waits for ends
 * only once the host's GPU has copied it; the thread then presents the slot.
 * A copy, not a blit: nothing of the application's state - its scissor, its
 * masks, sRGB writes - touches it. The slot's texture takes the window's own
 * colour format, so the stored values cross as they are, as the read-back the
 * chain replaces did. */
#ifndef AV_GLICD_CHAINED_FRAMES_HPP
#define AV_GLICD_CHAINED_FRAMES_HPP

#include "icd/common/window_chains.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::glicd {

class ChainedFrames {
public:
    static constexpr uint32_t slot_count = icd::WindowChains::max_slots;
    enum : uint32_t {
        gl_texture_2d = 0x0DE1u, gl_rgba8 = 0x8058u, gl_srgb8_alpha8 = 0x8C43u,
        /* GL_EXT_memory_object's GL_TEXTURE_TILING_EXT and GL_LINEAR_TILING_EXT. */
        gl_texture_tiling = 0x9580u, gl_linear_tiling = 0x9585u,
    };

    ChainedFrames();
    ChainedFrames(const ChainedFrames &) = delete;
    ChainedFrames &operator=(const ChainedFrames &) = delete;

    /* `window`'s chain at `width` x `height` on the adapter `luid`, opened in
     * this context through `wire`. False, with nothing held, for a chain that
     * cannot be had or a slot the device will not open here. */
    [[nodiscard]] bool open(ClientTransport &wire, icd::WindowChains &chains, void *window, uint64_t luid,
                            uint32_t width, uint32_t height, bool srgb);
    bool               opened() const;
    /* Opened, for `window` at `width` x `height`. */
    bool               fits(void *window, uint32_t width, uint32_t height) const;
    /* `source`, a texture of the opened size and format holding the frame,
     * copied into `slot`, with the GPU's work up to it finished before the
     * batch it goes in ends. */
    void               draw(ClientTransport &wire, uint32_t source, uint32_t slot);
    /* The slots' names deleted and their objects released through `wire` -
     * without one they went with the host's context - and the chain closed. */
    void               close(ClientTransport *wire, icd::WindowChains &chains);

    bool invariants_hold() const;

private:
    void delete_slots(ClientTransport &wire);
    void forget();

    void    *window_;
    uint32_t width_;
    uint32_t height_;
    uint64_t ids_[slot_count];
    uint64_t bytes_[slot_count];
    /* The host's names: the memory each slot was opened as, and the texture
     * over it. 0 for none. */
    uint32_t memories_[slot_count];
    uint32_t textures_[slot_count];
    bool     opened_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_CHAINED_FRAMES_HPP
