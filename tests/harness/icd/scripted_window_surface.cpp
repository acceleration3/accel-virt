#include "harness/icd/scripted_window_surface.hpp"

#include "harness/framework/test_registry.hpp"

#include <cstring>

namespace av {

ScriptedWindowSurface::ScriptedWindowSurface()
    : dcs_{}, dc_count_(0u), refuse_blit_(false), blits_(0u), blit_dc_(nullptr), blit_width_(0u), blit_height_(0u),
      blit_bytes_{} {}

void ScriptedWindowSurface::add_dc(void *dc, void *window, std::uint32_t width, std::uint32_t height)
{
    if (dc_count_ == max_dcs) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "more than %u DCs", max_dcs);
        return;
    }
    dcs_[dc_count_++] = Dc{dc, window, width, height, false};
}

void ScriptedWindowSurface::resize(void *window, std::uint32_t width, std::uint32_t height)
{
    for (std::uint32_t index = 0u; index < dc_count_; index++) {
        if (dcs_[index].window != window) continue;
        dcs_[index].width = width;
        dcs_[index].height = height;
    }
}

void ScriptedWindowSurface::close_window(void *window)
{
    for (std::uint32_t index = 0u; index < dc_count_; index++)
        if (dcs_[index].window == window) dcs_[index].closed = true;
}

void ScriptedWindowSurface::refuse_next_blit() { refuse_blit_ = true; }

ScriptedWindowSurface::Dc *ScriptedWindowSurface::find_dc(void *dc)
{
    for (std::uint32_t index = 0u; index < dc_count_; index++)
        if (dcs_[index].dc == dc && !dcs_[index].closed) return &dcs_[index];
    return nullptr;
}

void *ScriptedWindowSurface::window_of(void *dc)
{
    const Dc *const found = find_dc(dc);
    return found != nullptr ? found->window : nullptr;
}

bool ScriptedWindowSurface::client_size(void *window, std::uint32_t &width, std::uint32_t &height)
{
    for (std::uint32_t index = 0u; index < dc_count_; index++) {
        const Dc &dc = dcs_[index];
        if (window == nullptr || dc.window != window || dc.closed) continue;
        width = dc.width;
        height = dc.height;
        return true;
    }
    return false;
}

bool ScriptedWindowSurface::blit(void *dc, std::uint32_t width, std::uint32_t height,
                                 const std::uint8_t *bgra_bottom_up)
{
    const Dc *const found = find_dc(dc);
    if (found == nullptr || found->window == nullptr) return false;
    if (refuse_blit_) {
        refuse_blit_ = false;
        return false;
    }
    blits_++;
    blit_dc_ = dc;
    blit_width_ = width;
    blit_height_ = height;
    const std::uint64_t bytes = std::uint64_t{width} * height * 4u;
    std::memcpy(blit_bytes_, bgra_bottom_up, bytes < kept_bytes ? bytes : kept_bytes);
    return true;
}

std::uint32_t       ScriptedWindowSurface::blits() const { return blits_; }
void               *ScriptedWindowSurface::last_blit_dc() const { return blit_dc_; }
std::uint32_t       ScriptedWindowSurface::last_blit_width() const { return blit_width_; }
std::uint32_t       ScriptedWindowSurface::last_blit_height() const { return blit_height_; }
const std::uint8_t *ScriptedWindowSurface::last_blit_bytes() const { return blit_bytes_; }

}  // namespace av
