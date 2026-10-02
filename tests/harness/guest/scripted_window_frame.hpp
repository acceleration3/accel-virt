/* The probe's window as a test plays it (feature 046): a client size the test
 * changes, the pixels the "desktop" shows - written by the scripted Vulkan's
 * present, as the driver's would be - and a refusal at any of the three
 * window calls. */
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
    /* Every pixel of the client area set to `bgra`, as a present does. */
    void         show(const std::uint8_t bgra[4]);
    /* The last pixel left as it was: a frame that did not arrive whole. */
    void         show_but_the_last(const std::uint8_t bgra[4]);
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
};

}  // namespace av

#endif  // AV_SCRIPTED_WINDOW_FRAME_HPP
