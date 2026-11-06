#ifndef AV_SCRIPTED_WINDOW_SURFACE_HPP
#define AV_SCRIPTED_WINDOW_SURFACE_HPP

#include "icd/common/window_surface.hpp"

#include <cstdint>

namespace av {

class ScriptedWindowSurface final : public icd::WindowSurface {
public:
    static constexpr std::uint32_t max_dcs = 1100u;
    static constexpr std::uint32_t kept_bytes = 1u << 16u;

    ScriptedWindowSurface();

    void add_dc(void *dc, void *window, std::uint32_t width, std::uint32_t height);
    void resize(void *window, std::uint32_t width, std::uint32_t height);
    void close_window(void *window);
    void refuse_next_blit();
    void resize_when_a_blit_is_refused(void *window, std::uint32_t width, std::uint32_t height);

    void *window_of(void *dc) override;
    bool  client_size(void *window, std::uint32_t &width, std::uint32_t &height) override;
    bool  blit(void *dc, std::uint32_t width, std::uint32_t height, const std::uint8_t *bgra_bottom_up) override;
    bool  blit_window(void *window, std::uint32_t width, std::uint32_t height,
                      const std::uint8_t *bgra_bottom_up) override;
    bool  blit_window_top_down(void *window, std::uint32_t width, std::uint32_t height,
                               const std::uint8_t *bgra_top_down) override;

    std::uint32_t       blits() const;
    void               *last_blit_dc() const;
    std::uint32_t       last_blit_width() const;
    std::uint32_t       last_blit_height() const;
    const std::uint8_t *last_blit_bytes() const;
    bool                last_blit_top_down() const;

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
    void         *resize_window_;
    std::uint32_t resize_width_;
    std::uint32_t resize_height_;
    std::uint32_t blits_;
    void         *blit_dc_;
    std::uint32_t blit_width_;
    std::uint32_t blit_height_;
    std::uint8_t  blit_bytes_[kept_bytes];
    bool          top_down_;
};

}

#endif
