#ifndef AV_GL_PROBE_HPP
#define AV_GL_PROBE_HPP

#include "core/support/status.hpp"
#include "guest/probe/vblank_clock.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class ImageSink;

class GlEntry;
class GuestReport;
class Kmt;

class GlProbe {
public:
    static constexpr uint32_t max_extent = 64u;
    static constexpr uint32_t share_extent = 2u;
    enum : uint32_t {
        gl_triangles = 0x0004u, gl_color_buffer_bit = 0x4000u, gl_renderer = 0x1F01u, gl_version = 0x1F02u,
        gl_fragment_shader = 0x8B30u, gl_vertex_shader = 0x8B31u, gl_compile_status = 0x8B81u,
        gl_link_status = 0x8B82u, error_invalid_version_arb = 0x2095u, error_invalid_profile_arb = 0x2096u,
        error_busy = 170u, gl_context_lost = 0x0507u, gl_guilty_context_reset = 0x8253u,
        gl_context_profile_mask = 0x9126u, gl_compatibility_profile_bit = 0x2u,
    };
    static constexpr uint32_t paced_frames = 60u;
    static constexpr int32_t  paced_interval = 1;
    static constexpr uint8_t colour[4] = {64u, 128u, 191u, 255u};

    GlProbe(Kmt &kmt, GuestReport &report, GlEntry &gl, ImageSink &images);
    GlProbe(const GlProbe &) = delete;
    GlProbe &operator=(const GlProbe &) = delete;

    void run();

private:
    [[nodiscard]] Status pixel_format_step();
    [[nodiscard]] Status context_step(void *&context);
    void                 version_step();
    void                 draw_step(const char *name);
    void                 judge_frame(const char *name);
    void                 share_step(void *current);
    void                 busy_step(void *current);
    void                 attribs_step();
    void                 compat_step();
    void                 interval_step();
    void                 resized_step();
    void                 lost_step();
    void                 recreated_step();
    uint32_t             program();
    uint32_t             shader(uint32_t type, const char *source);

    VblankClock   clock_;
    GuestReport  &report_;
    GlEntry      &gl_;
    ImageSink    &images_;
    ScenarioSteps steps_;
};

}

#endif
