/* The probe's window as a test plays it (feature 046): a client size the test
 * changes, the pixels the "desktop" shows - written by the scripted Vulkan's
 * present, as the driver's would be - and a refusal at any of the three
 * window calls. Resized, it shows its last frame stretched over the new size
 * until the next present, as DWM shows a flip chain's window (measured on
 * avgpu and on the Basic Render Driver alike). */
#ifndef AV_SCRIPTED_WINDOW_FRAME_HPP
#define AV_SCRIPTED_WINDOW_FRAME_HPP

#include "guest/probe/window_frame.hpp"

#include <cstdint>

namespace av {

class ScriptedWindowFrame final : public WindowFrame {
public:
    /* Larger than the largest frame a probe keeps, so a test can play a window
     * bigger than the present work has room for. */
    static constexpr std::uint32_t max_side = 160u;
    static constexpr std::uint32_t max_bytes = max_side * max_side * 4u;

    ScriptedWindowFrame();

    /* The window this plays, and its client size. */
    void         set_window(void *window, std::uint32_t width, std::uint32_t height);
    void         refuse_size();
    /* The size is answered `count` times and refused after that: a window torn
     * down between two of a caller's questions about it. */
    void         refuse_size_after(std::uint32_t count);
    void         refuse_read();
    void         refuse_resize();
    /* Every pixel of the client area set to `bgra`. */
    void         show(const std::uint8_t bgra[4]);
    /* A presented frame: `drawn` over `width` x `height` at the top left of
     * the client area, `cleared` past it. */
    void         show_frame(const std::uint8_t drawn[4], std::uint32_t width, std::uint32_t height,
                            const std::uint8_t cleared[4]);
    /* The last pixel black: a frame that did not arrive whole. */
    void         lose_the_last_pixel();
    std::uint32_t reads() const;
    std::uint32_t resizes() const;

    bool size(void *window, std::uint32_t &width, std::uint32_t &height) override;
    bool read(void *window, std::uint32_t width, std::uint32_t height, std::uint8_t *bgra) override;
    bool resize(void *window, std::uint32_t width, std::uint32_t height) override;

private:
    void         *window_;
    std::uint32_t width_;
    std::uint32_t height_;
    bool          refuse_size_;
    bool          limited_sizes_;
    std::uint32_t size_limit_;
    std::uint32_t sizes_;
    bool          refuse_read_;
    bool          refuse_resize_;
    std::uint32_t reads_;
    std::uint32_t resizes_;
    std::uint8_t  pixels_[max_bytes];
    std::uint8_t  shown_[max_bytes];
};

}  // namespace av

#endif  // AV_SCRIPTED_WINDOW_FRAME_HPP
