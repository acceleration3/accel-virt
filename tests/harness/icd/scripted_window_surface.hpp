/* The OpenGL ICD's windows in a test: DCs the test declares, each on a window
 * or on none (a memory DC), client sizes the test changes, windows the test
 * closes, and every blit kept - its size, its count and its first bytes - so a
 * test judges the frame the ICD put on a window. */
#ifndef AV_SCRIPTED_WINDOW_SURFACE_HPP
#define AV_SCRIPTED_WINDOW_SURFACE_HPP

#include "icd/opengl/window_surface.hpp"

#include <cstdint>

namespace av {

class ScriptedWindowSurface final : public glicd::WindowSurface {
public:
    static constexpr std::uint32_t max_dcs = 300u;
    static constexpr std::uint32_t kept_bytes = 1u << 16u;

    ScriptedWindowSurface();

    /* `dc` draws on `window` (null: a memory DC) of `width` x `height`. */
    void add_dc(void *dc, void *window, std::uint32_t width, std::uint32_t height);
    void resize(void *window, std::uint32_t width, std::uint32_t height);
    void close_window(void *window);
    void refuse_next_blit();

    void *window_of(void *dc) override;
    bool  client_size(void *window, std::uint32_t &width, std::uint32_t &height) override;
    bool  blit(void *dc, std::uint32_t width, std::uint32_t height, const std::uint8_t *bgra_bottom_up) override;

    std::uint32_t       blits() const;
    void               *last_blit_dc() const;
    std::uint32_t       last_blit_width() const;
    std::uint32_t       last_blit_height() const;
    /* The last blit's first kept_bytes bytes. */
    const std::uint8_t *last_blit_bytes() const;

private:
    struct Dc {
        void         *dc;
        void         *window;
        std::uint32_t width;
        std::uint32_t height;
        bool          closed;
    };

    Dc *find_dc(void *dc);

    Dc            dcs_[max_dcs];
    std::uint32_t dc_count_;
    bool          refuse_blit_;
    std::uint32_t blits_;
    void         *blit_dc_;
    std::uint32_t blit_width_;
    std::uint32_t blit_height_;
    std::uint8_t  blit_bytes_[kept_bytes];
};

}  // namespace av

#endif  // AV_SCRIPTED_WINDOW_SURFACE_HPP
