#ifndef AV_SCRIPTED_WINDOW_FRAME_HPP
#define AV_SCRIPTED_WINDOW_FRAME_HPP

#include "guest/probe/window_frame.hpp"

#include <cstdint>

namespace av {

class ScriptedWindowFrame final : public WindowFrame {
public:
    static constexpr std::uint32_t max_side = 160u;
    static constexpr std::uint32_t max_bytes = max_side * max_side * 4u;

    ScriptedWindowFrame();

    void         set_window(void *window, std::uint32_t width, std::uint32_t height);
    void         refuse_size();
    void         refuse_size_after(std::uint32_t count);
    void         refuse_read();
    void         refuse_resize();
    void         show(const std::uint8_t bgra[4]);
    void         show_frame(const std::uint8_t drawn[4], std::uint32_t width, std::uint32_t height,
                            const std::uint8_t cleared[4]);
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

}

#endif
