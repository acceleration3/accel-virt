/* The OpenGL ICD's proof (feature 050, T038), over whatever GL it is handed.
 * Steps, as the report carries them under `opengl`: pixel_format (the format
 * chosen and set); context (a context current on the window); version (the
 * version as major*10+minor, at least 3.0 and at most 4.6, with GL_VERSION and
 * GL_RENDERER as notes); draw (a full-viewport triangle in solid.frag's colour
 * put on the window by SwapBuffers and read back: how many pixels were that
 * colour within 1); share (a texture made in one context and read in another
 * after wglShareLists: how many texels came back); attribs (4.7 refused with
 * ERROR_INVALID_VERSION_ARB and both profile bits with
 * ERROR_INVALID_PROFILE_ARB). A step that cannot run past a failure ends the
 * run there; every context made is deleted. */
#ifndef AV_GL_PROBE_HPP
#define AV_GL_PROBE_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class GlEntry;
class GuestReport;
class Kmt;

class GlProbe {
public:
    /* The largest window whose pixels the probe reads back. */
    static constexpr uint32_t max_extent = 64u;
    static constexpr uint32_t share_extent = 2u;
    enum : uint32_t {
        gl_triangles = 0x0004u, gl_color_buffer_bit = 0x4000u, gl_renderer = 0x1F01u, gl_version = 0x1F02u,
        gl_fragment_shader = 0x8B30u, gl_vertex_shader = 0x8B31u, gl_compile_status = 0x8B81u,
        gl_link_status = 0x8B82u, error_invalid_version_arb = 0x2095u, error_invalid_profile_arb = 0x2096u,
    };
    /* solid.frag's vec4(0.25, 0.5, 0.75, 1.0) as UNORM bytes. */
    static constexpr uint8_t colour[4] = {64u, 128u, 191u, 255u};

    GlProbe(Kmt &kmt, GuestReport &report, GlEntry &gl);
    GlProbe(const GlProbe &) = delete;
    GlProbe &operator=(const GlProbe &) = delete;

    void run();

private:
    [[nodiscard]] Status pixel_format_step();
    [[nodiscard]] Status context_step(void *&context);
    void                 version_step();
    void                 draw_step();
    void                 share_step(void *current);
    void                 attribs_step();
    /* The triangle's program, 0 when a shader or the link failed. */
    uint32_t             program();
    uint32_t             shader(uint32_t type, const char *source);

    GuestReport  &report_;
    GlEntry      &gl_;
    ScenarioSteps steps_;
};

}  // namespace av

#endif  // AV_GL_PROBE_HPP
