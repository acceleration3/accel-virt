/* The windows whose frames reach the compositor as host surfaces rather than
 * as pixels (docs/shared-surfaces.md, step 5).
 *
 * Each window gets a DXGI flip chain on the Direct3D 11 device of the adapter
 * its frames are drawn on, and a few shared textures of that device - slots.
 * An ICD opens a slot by its object id (feature 053) and has the host copy a
 * frame into it as its API stores one (SlotLayout); presenting the slot puts
 * it in the chain's back buffer the right way up, on the host, and presents
 * that, which DWM composes from the host's own texture. No frame crosses guest
 * memory, and nothing is blitted through GDI.
 *
 * The Windows shim answers through d3d11 and dxgi; a test declares its chains.
 * open and close run on an application's thread, present on OpenGL's present
 * thread or on the presenting Vulkan application's own, never for the same
 * window at once: a caller settles its frames first. */
#ifndef AV_ICD_WINDOW_CHAINS_HPP
#define AV_ICD_WINDOW_CHAINS_HPP

#include <stdint.h>

namespace av::icd {

/* How a slot holds a frame: OpenGL's rows start at the bottom, a Vulkan
 * image's at the top, and a Vulkan image may be blue first. A slot takes the
 * frame's own channel order, so a frame is copied in rather than converted. */
enum class SlotLayout : uint32_t {
    rgba_bottom_up,
    rgba_top_down,
    bgra_top_down,
};

class WindowChains {
public:
    static constexpr uint32_t max_slots = 2u;

    WindowChains(const WindowChains &) = delete;
    WindowChains &operator=(const WindowChains &) = delete;

    /* `window`'s chain on the adapter `luid` names, of `width` x `height`
     * slots holding frames as `layout` says, made again when the size or the
     * layout changed: the object id and the bytes each of its `slots` is
     * shared as, in `ids` and `bytes`. False where no chain can be made - no
     * such adapter, no Direct3D 11 on it, a window DXGI refuses, more than
     * max_slots - with no chain kept for the window, and the caller shows its
     * frames as before. */
    virtual bool open(void *window, uint64_t luid, uint32_t width, uint32_t height, SlotLayout layout,
                      uint32_t slots, uint64_t *ids, uint64_t *bytes) = 0;
    /* Slot `slot` of `window`'s chain on the window, the right way up,
     * returning once the draw out of it is done, so a frame may be copied into
     * it again. False for a window with no chain or a slot it does not have. */
    virtual bool present(void *window, uint32_t slot) = 0;
    /* `window`'s chain and its slots gone; nothing for a window with none. */
    virtual void close(void *window) = 0;

protected:
    WindowChains() = default;
    ~WindowChains() = default;
};

}  // namespace av::icd

#endif  // AV_ICD_WINDOW_CHAINS_HPP
