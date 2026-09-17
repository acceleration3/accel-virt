/* The WGL and OpenGL the GL probe calls (feature 050, T038), on one window the
 * implementation owns: one method per call the probe makes, GL's types as
 * their fixed-width equivalents. In the guest it is the system opengl32.dll
 * and a window of the probe's own; on the full-stack rig, FakeOpengl32 over
 * the ICD; in the probe's unit tests, a scripted GL. Virtual for the reason
 * Platform is. */
#ifndef AV_GL_ENTRY_HPP
#define AV_GL_ENTRY_HPP

#include <stdint.h>

namespace av {

class GlEntry {
public:
    GlEntry(const GlEntry &) = delete;
    GlEntry &operator=(const GlEntry &) = delete;

    /* The window's client size. */
    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;

    /* ChoosePixelFormat for a double-buffered RGBA8 D24S8 window; 0 for none. */
    virtual int32_t  choose_pixel_format() = 0;
    virtual bool     set_pixel_format(int32_t format) = 0;
    /* wglCreateContext on the window's DC; null when refused. */
    virtual void    *create_context() = 0;
    /* wglCreateContextAttribsARB, sharing nothing; null when refused. */
    virtual void    *create_context_attribs(const int32_t *attributes) = 0;
    /* wglMakeCurrent on the window's DC; null releases. */
    virtual bool     make_current(void *context) = 0;
    virtual bool     delete_context(void *context) = 0;
    virtual bool     share_lists(void *first, void *second) = 0;
    virtual bool     swap_buffers() = 0;
    /* GetLastError, after a WGL call that failed. */
    virtual uint32_t last_error() const = 0;
    /* wglMakeCurrent of `context` on the window's DC from a second thread,
     * which releases it again when that succeeded: 0 then, GetLastError
     * there otherwise. */
    virtual uint32_t make_current_elsewhere(void *context) = 0;
    /* The ICD's probe-only rvkForceContextFault through wglGetProcAddress:
     * what it answers, or -1 when the ICD has no such hook. */
    virtual int32_t  force_fault() = 0;
    /* glGetGraphicsResetStatus through wglGetProcAddress; 0 without it. */
    virtual uint32_t reset_status() = 0;

    virtual const char *string(uint32_t name) = 0;
    virtual uint32_t    error() = 0;
    virtual void        viewport(int32_t x, int32_t y, int32_t width, int32_t height) = 0;
    virtual void        clear_colour(float red, float green, float blue, float alpha) = 0;
    virtual void        clear(uint32_t mask) = 0;
    virtual void        draw_arrays(uint32_t mode, int32_t first, int32_t count) = 0;
    virtual void        finish() = 0;
    virtual void        read_pixels(int32_t width, int32_t height, uint8_t *rgba) = 0;

    virtual uint32_t create_shader(uint32_t type) = 0;
    virtual void     shader_source(uint32_t shader, const char *source) = 0;
    virtual void     compile_shader(uint32_t shader) = 0;
    virtual int32_t  shader_parameter(uint32_t shader, uint32_t pname) = 0;
    virtual void     delete_shader(uint32_t shader) = 0;
    virtual uint32_t create_program() = 0;
    virtual void     attach_shader(uint32_t program, uint32_t shader) = 0;
    virtual void     link_program(uint32_t program) = 0;
    virtual int32_t  program_parameter(uint32_t program, uint32_t pname) = 0;
    virtual void     use_program(uint32_t program) = 0;
    virtual void     delete_program(uint32_t program) = 0;
    virtual uint32_t gen_vertex_array() = 0;
    virtual void     bind_vertex_array(uint32_t array) = 0;
    virtual void     delete_vertex_array(uint32_t array) = 0;

    virtual uint32_t gen_texture() = 0;
    virtual void     bind_texture(uint32_t texture) = 0;
    /* A 2D RGBA8 level 0 of `width` x `height` from `rgba`. */
    virtual void     texture_image(int32_t width, int32_t height, const uint8_t *rgba) = 0;
    /* The bound 2D texture's level 0 as RGBA bytes. */
    virtual void     read_texture(uint8_t *rgba) = 0;
    virtual void     delete_texture(uint32_t texture) = 0;

protected:
    GlEntry() = default;
    /* Non-virtual: nothing is destroyed through this pointer (CLAUDE.md, Adaptations). */
    ~GlEntry() = default;
};

}  // namespace av

#endif  // AV_GL_ENTRY_HPP
