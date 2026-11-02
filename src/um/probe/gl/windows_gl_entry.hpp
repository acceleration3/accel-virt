#ifndef AV_UM_WINDOWS_GL_ENTRY_HPP
#define AV_UM_WINDOWS_GL_ENTRY_HPP

#include "guest/probe/gl_entry.hpp"

#include <stdint.h>

namespace av {

class ProbeWindow;

class WindowsGlEntry final : public GlEntry {
public:
    explicit WindowsGlEntry(ProbeWindow &window);
    WindowsGlEntry(const WindowsGlEntry &) = delete;
    WindowsGlEntry &operator=(const WindowsGlEntry &) = delete;

    uint32_t width() const override;
    uint32_t height() const override;
    int32_t  choose_pixel_format() override;
    bool     set_pixel_format(int32_t format) override;
    void    *create_context() override;
    void    *create_context_attribs(const int32_t *attributes) override;
    bool     make_current(void *context) override;
    bool     delete_context(void *context) override;
    bool     share_lists(void *first, void *second) override;
    bool     swap_buffers() override;
    uint32_t last_error() const override;
    uint32_t make_current_elsewhere(void *context) override;
    int32_t  force_fault() override;
    uint32_t reset_status() override;

    int32_t     integer(uint32_t pname) override;
    bool        set_swap_interval(int32_t interval) override;
    int32_t     swap_interval() override;
    bool        resize(uint32_t width, uint32_t height) override;
    bool        legacy_triangle() override;
    const char *string(uint32_t name) override;
    uint32_t    error() override;
    void        viewport(int32_t x, int32_t y, int32_t width, int32_t height) override;
    void        clear_colour(float red, float green, float blue, float alpha) override;
    void        clear(uint32_t mask) override;
    void        draw_arrays(uint32_t mode, int32_t first, int32_t count) override;
    void        finish() override;
    void        read_pixels(int32_t width, int32_t height, uint8_t *rgba) override;

    uint32_t create_shader(uint32_t type) override;
    void     shader_source(uint32_t shader, const char *source) override;
    void     compile_shader(uint32_t shader) override;
    int32_t  shader_parameter(uint32_t shader, uint32_t pname) override;
    void     delete_shader(uint32_t shader) override;
    uint32_t create_program() override;
    void     attach_shader(uint32_t program, uint32_t shader) override;
    void     link_program(uint32_t program) override;
    int32_t  program_parameter(uint32_t program, uint32_t pname) override;
    void     use_program(uint32_t program) override;
    void     delete_program(uint32_t program) override;
    uint32_t gen_vertex_array() override;
    void     bind_vertex_array(uint32_t array) override;
    void     delete_vertex_array(uint32_t array) override;

    uint32_t gen_texture() override;
    void     bind_texture(uint32_t texture) override;
    void     texture_image(int32_t width, int32_t height, const uint8_t *rgba) override;
    void     read_texture(uint8_t *rgba) override;
    void     delete_texture(uint32_t texture) override;

private:
    template <typename Function>
    Function named(const char *name) const;

    ProbeWindow &window_;
};

}

#endif
