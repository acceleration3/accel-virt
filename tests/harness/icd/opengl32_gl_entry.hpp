/* The GL probe's GlEntry over FakeOpengl32 (feature 050, T038): WGL through
 * its exports, GL 1.1 through the calling thread's proc table, the rest
 * through DrvGetProcAddress, as an application on the system opengl32 reaches
 * them; GetLastError from the thread slot the ICD sets it in. The window is
 * one DC the caller declared. ChoosePixelFormat is GDI's, which the full-stack
 * rig does not have: it answers the ICD's first format, the double-buffered
 * RGBA8 D24S8 one GDI would match. */
#ifndef AV_OPENGL32_GL_ENTRY_HPP
#define AV_OPENGL32_GL_ENTRY_HPP

#include "guest/probe/gl_entry.hpp"

#include <cstdint>

namespace av {

class FakeOpengl32;
class ScriptedThreadSlot;

class Opengl32GlEntry final : public GlEntry {
public:
    Opengl32GlEntry(FakeOpengl32 &opengl32, ScriptedThreadSlot &threads, void *dc, std::uint32_t width,
                    std::uint32_t height);
    Opengl32GlEntry(const Opengl32GlEntry &) = delete;
    Opengl32GlEntry &operator=(const Opengl32GlEntry &) = delete;

    std::uint32_t width() const override;
    std::uint32_t height() const override;
    std::int32_t  choose_pixel_format() override;
    bool          set_pixel_format(std::int32_t format) override;
    void         *create_context() override;
    void         *create_context_attribs(const std::int32_t *attributes) override;
    bool          make_current(void *context) override;
    bool          delete_context(void *context) override;
    bool          share_lists(void *first, void *second) override;
    bool          swap_buffers() override;
    std::uint32_t last_error() const override;
    std::uint32_t make_current_elsewhere(void *context) override;
    std::int32_t  force_fault() override;
    std::uint32_t reset_status() override;
    std::int32_t  integer(std::uint32_t pname) override;
    bool          set_swap_interval(std::int32_t interval) override;
    std::int32_t  swap_interval() override;
    bool          resize(std::uint32_t width, std::uint32_t height) override;
    bool          legacy_triangle() override;

    const char   *string(std::uint32_t name) override;
    std::uint32_t error() override;
    void          viewport(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) override;
    void          clear_colour(float red, float green, float blue, float alpha) override;
    void          clear(std::uint32_t mask) override;
    void          draw_arrays(std::uint32_t mode, std::int32_t first, std::int32_t count) override;
    void          finish() override;
    void          read_pixels(std::int32_t width, std::int32_t height, std::uint8_t *rgba) override;

    std::uint32_t create_shader(std::uint32_t type) override;
    void          shader_source(std::uint32_t shader, const char *source) override;
    void          compile_shader(std::uint32_t shader) override;
    std::int32_t  shader_parameter(std::uint32_t shader, std::uint32_t pname) override;
    void          delete_shader(std::uint32_t shader) override;
    std::uint32_t create_program() override;
    void          attach_shader(std::uint32_t program, std::uint32_t shader) override;
    void          link_program(std::uint32_t program) override;
    std::int32_t  program_parameter(std::uint32_t program, std::uint32_t pname) override;
    void          use_program(std::uint32_t program) override;
    void          delete_program(std::uint32_t program) override;
    std::uint32_t gen_vertex_array() override;
    void          bind_vertex_array(std::uint32_t array) override;
    void          delete_vertex_array(std::uint32_t array) override;

    std::uint32_t gen_texture() override;
    void          bind_texture(std::uint32_t texture) override;
    void          texture_image(std::int32_t width, std::int32_t height, const std::uint8_t *rgba) override;
    void          read_texture(std::uint8_t *rgba) override;
    void          delete_texture(std::uint32_t texture) override;

private:
    /* ERROR_PROC_NOT_FOUND. */
    static constexpr std::uint32_t proc_not_found = 127u;
    static constexpr std::uint32_t elsewhere = 7u;

    template <typename Function>
    Function named(const char *name) const;

    FakeOpengl32       &opengl32_;
    ScriptedThreadSlot &threads_;
    void               *dc_;
    std::uint32_t       width_;
    std::uint32_t       height_;
};

}  // namespace av

#endif  // AV_OPENGL32_GL_ENTRY_HPP
