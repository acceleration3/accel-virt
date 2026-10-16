/* One window's chain slots opened as shared memory through one connection
 * (docs/shared-surfaces.md, step 5): the window's DXGI chain asked of the
 * WindowChains at a size and layout, and each of its slots opened by the
 * object id it is shared as (feature 053), as memory the API then makes its
 * image or texture over. What the Vulkan swapchain (ChainedImage, one slot)
 * and the OpenGL window framebuffers (ChainedFrames, two) both do; the import
 * over the memory, the slot count and the row order are theirs.
 *
 * An API's objects over the memory go before the memory is released: OpenGL
 * deletes its textures first, Vulkan destroys its image. */
#ifndef AV_ICD_CHAIN_SLOTS_HPP
#define AV_ICD_CHAIN_SLOTS_HPP

#include "icd/common/window_chains.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::icd {

class ChainSlots {
public:
    static constexpr uint32_t max_slots = WindowChains::max_slots;

    ChainSlots();
    ChainSlots(const ChainSlots &) = delete;
    ChainSlots &operator=(const ChainSlots &) = delete;

    /* `window`'s chain of `chains` on the adapter `luid` at `width` x
     * `height`, `count` slots holding frames as `layout` says, each opened
     * through `wire` into `into` (OpenedObjects: a Vulkan device, 0 for
     * OpenGL). A chain held for another window is closed first; this window's is made
     * again by the chains. False, with nothing held and no chain left for the
     * window, for a chain that cannot be had or a slot that will not open -
     * what was opened is released - and for a window of no area. Every slot
     * held is released through `wire` before the open. */
    [[nodiscard]] bool open(ClientTransport &wire, WindowChains &chains, void *window, uint64_t luid, uint32_t width,
                            uint32_t height, SlotLayout layout, uint32_t count, uint64_t into);
    bool               opened() const;
    /* Opened, for `window` at `width` x `height`. */
    bool               fits(void *window, uint32_t width, uint32_t height) const;
    uint32_t           count() const;
    uint32_t           width() const;
    uint32_t           height() const;
    /* The host's handle `slot` was opened as; 0 for a slot not opened. */
    uint64_t           memory(uint32_t slot) const;
    /* Slot `slot` on the window: false when the chain will not present it or
     * nothing is opened. */
    [[nodiscard]] bool present(uint32_t slot);
    /* Each slot opened released through `wire`, the window's chain left as it
     * is: a retired Vulkan swapchain's window is its successor's. */
    void               release(ClientTransport &wire);
    /* The window's chain closed and everything forgotten: without release()
     * first, the slots went with the host's context. */
    void               close();

    bool invariants_hold() const;

private:
    void forget_slots();

    /* The chains the window's chain was opened through; null with none. */
    WindowChains *chains_;
    void         *window_;
    uint32_t      width_;
    uint32_t      height_;
    uint32_t      count_;
    uint64_t      ids_[max_slots];
    uint64_t      bytes_[max_slots];
    uint64_t      memories_[max_slots];
    bool          opened_;
};

}  // namespace av::icd

#endif  // AV_ICD_CHAIN_SLOTS_HPP
