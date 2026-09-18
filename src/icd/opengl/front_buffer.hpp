/* The last frame presented on one window, as the front buffer (feature 051).
 *
 * This driver's swap reads the back buffer and blits it, so the pixels it put
 * on the window are still in the guest when the application asks for them -
 * and GL_FRONT is the only name for a buffer the host does not have, since the
 * window is a framebuffer object there. A read of it is answered from here,
 * with no round trip, for the format and type a present already produced; any
 * other read is not this buffer's to answer and goes to the host as before. */
#ifndef AV_GLICD_FRONT_BUFFER_HPP
#define AV_GLICD_FRONT_BUFFER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::glicd {

struct FrontBufferProbe;

class FrontBuffer {
public:
    /* What a present leaves behind, and what a read may ask for. */
    enum : uint32_t { gl_rgba = 0x1908u, gl_bgra = 0x80E1u, gl_unsigned_byte = 0x1401u };
    static constexpr uint32_t bytes_per_pixel = 4u;

    FrontBuffer();

    /* The frame a present put on the window: `width` x `height` BGRA pixels,
     * the bottom row first, held by the presenter for as long as this. */
    void presented(const uint8_t *bgra_bottom_up, uint32_t width, uint32_t height);
    /* Nothing has been presented, or the buffer it was in is gone. */
    void forget();
    bool presented() const;
    uint32_t width() const;
    uint32_t height() const;

    /* Answers `width` x `height` pixels from (`x`, `y`), the bottom row first
     * as GL reads, into `pixels`. False when nothing has been presented, when
     * the rectangle leaves the frame, when the format or type is one a present
     * did not produce, or when there is nowhere to write - and then nothing is
     * written and the caller asks the host. */
    [[nodiscard]] bool read(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type,
                            void *pixels) const;

    bool invariants_hold() const;

private:
    const uint8_t *pixels_;
    uint32_t       width_;
    uint32_t       height_;

    friend struct FrontBufferProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_FRONT_BUFFER_HPP
