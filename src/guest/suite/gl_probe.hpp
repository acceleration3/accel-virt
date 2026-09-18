/* The OpenGL ICD's proof (feature 050, T038), over whatever GL it is handed.
 * Steps, as the report carries them under `opengl`: pixel_format (the format
 * chosen and set); context (a context current on the window); version (the
 * version as major*10+minor, at least 3.0 and at most 4.6, with GL_VERSION and
 * GL_RENDERER as notes); draw (a full-viewport triangle in solid.frag's colour
 * put on the window by SwapBuffers and read back: how many pixels were that
 * colour within 1); share (a texture made in one context and read in another
 * after wglShareLists: how many texels came back); busy (the context current
 * here refused to a second thread with ERROR_BUSY: the error); attribs (4.7
 * refused with ERROR_INVALID_VERSION_ARB and both profile bits with
 * ERROR_INVALID_PROFILE_ARB); lost (a robust context faulted through the
 * ICD's rvkForceContextFault answers GL_UNKNOWN_CONTEXT_RESET, then
 * GL_CONTEXT_LOST from glGetError once and nothing after: the reset status);
 * recreated (a context made after the loss passes draw: its pixels). A step
 * that cannot run past a failure ends the run there; every context made is
 * deleted. */
#ifndef AV_GL_PROBE_HPP
#define AV_GL_PROBE_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class ImageSink;

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
        error_busy = 170u, gl_context_lost = 0x0507u, gl_unknown_context_reset = 0x8255u,
        gl_context_profile_mask = 0x9126u, gl_compatibility_profile_bit = 0x2u,
    };
    /* What the interval step swaps, and the interval it asks for. */
    static constexpr uint32_t paced_frames = 60u;
    static constexpr int32_t  paced_interval = 1;
    /* solid.frag's vec4(0.25, 0.5, 0.75, 1.0) as UNORM bytes. */
    static constexpr uint8_t colour[4] = {64u, 128u, 191u, 255u};

    /* `images` keeps a picture of every render the probe judges. */
    GlProbe(Kmt &kmt, GuestReport &report, GlEntry &gl, ImageSink &images);
    GlProbe(const GlProbe &) = delete;
    GlProbe &operator=(const GlProbe &) = delete;

    void run();

private:
    [[nodiscard]] Status pixel_format_step();
    [[nodiscard]] Status context_step(void *&context);
    void                 version_step();
    /* The draw, reported under `name`: draw, or recreated. */
    void                 draw_step(const char *name);
    /* The frame now on the window, judged and saved under `name`. */
    void                 judge_frame(const char *name);
    void                 share_step(void *current);
    void                 busy_step(void *current);
    void                 attribs_step();
    /* Feature 051: what profile the host granted and whether the fixed-function
     * pipeline draws through it; how many blanks sixty paced swaps took; and a
     * window resized under a context that keeps presenting. */
    void                 compat_step();
    void                 interval_step();
    void                 resized_step();
    /* One vertical blank in performance-counter ticks, 0 where the adapter
     * answers none. */
    uint64_t             blank_ticks();
    uint64_t             counter() const;
    void                 lost_step();
    void                 recreated_step();
    /* The triangle's program, 0 when a shader or the link failed. */
    uint32_t             program();
    uint32_t             shader(uint32_t type, const char *source);

    Kmt          &kmt_;
    GuestReport  &report_;
    GlEntry      &gl_;
    ImageSink    &images_;
    ScenarioSteps steps_;
};

}  // namespace av

#endif  // AV_GL_PROBE_HPP
