/* The OpenGL the GL probe's unit tests play (feature 050, T038), behind
 * GlEntry: contexts that share textures once wglShareLists joined them,
 * shaders that compile and programs that link unless told not to, a draw
 * that fills the back buffer with solid.frag's colour when a linked program
 * and a vertex array are bound, and a swap that keeps it for the read. Any
 * call in Call can be made to fail on its Nth use, and every object made is
 * counted, so a test asserts nothing is left. */
#ifndef AV_SCRIPTED_GL_HPP
#define AV_SCRIPTED_GL_HPP

#include "guest/suite/gl_entry.hpp"

#include <cstdint>

namespace av {

class ScriptedGl final : public GlEntry {
public:
    enum class Call : std::uint32_t {
        choose_pixel_format,
        set_pixel_format,
        create_context,
        make_current,
        make_current_elsewhere,
        force_fault,
        share_lists,
        swap_buffers,
        create_shader,
        compile_shader,
        create_program,
        link_program,
        count
    };
    static constexpr std::uint32_t max_contexts = 8u;
    static constexpr std::uint32_t max_textures = 4u;
    static constexpr std::uint32_t max_extent = 64u;
    static constexpr std::uint32_t refused = 0x7Bu;

    ScriptedGl(std::uint32_t width, std::uint32_t height);
    ScriptedGl(const ScriptedGl &) = delete;
    ScriptedGl &operator=(const ScriptedGl &) = delete;

    void fail_at(Call call, std::uint32_t nth);
    void set_strings(const char *version, const char *renderer);
    /* What glGetError answers after the draw. */
    void set_error(std::uint32_t error);
    /* Added to every channel the draw writes. */
    void shift_colour(std::int32_t shift);
    /* The ARB errors wglCreateContextAttribsARB then does not give: it makes the context. */
    void accept_versions();
    void accept_profiles();
    /* Textures read in another context come back empty although shared. */
    void isolate_textures();
    /* A context current here may be made current on another thread too. */
    void let_other_threads_take_contexts();
    /* No rvkForceContextFault; the fault leaves the context working; a lost
     * robust context reports no reset; a loss is never reported by
     * glGetError, or reported by every call. */
    void hide_fault_hook();
    void survive_faults();
    void forget_resets();
    void leave_losses_unreported();
    void report_losses_forever();

    std::uint32_t live_contexts() const;
    /* Shaders, programs, vertex arrays and textures not deleted. */
    std::uint32_t live_objects() const;
    std::uint32_t misuses() const;
    std::uint32_t calls(Call call) const;

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
    struct Context {
        bool          live = false;
        bool          made = false;
        std::uint32_t group = 0u;
        bool          robust = false;
        bool          lost = false;
        bool          loss_reported = false;
    };
    struct Texture {
        bool          live = false;
        std::uint32_t group = 0u;
        std::uint8_t  texels[16] = {};
    };

    bool          fails(Call call);
    std::uint32_t row_of(void *context) const;
    /* A GL call with nothing current is a misuse. */
    bool          current_or_misuse();

    std::uint32_t width_;
    std::uint32_t height_;
    std::uint32_t fail_at_[static_cast<std::uint32_t>(Call::count)];
    std::uint32_t calls_[static_cast<std::uint32_t>(Call::count)];
    const char   *version_;
    const char   *renderer_;
    std::uint32_t error_;
    std::int32_t  shift_;
    bool          accept_versions_;
    bool          accept_profiles_;
    bool          isolate_;
    bool          shared_threads_;
    bool          hidden_hook_;
    bool          survives_;
    bool          forgets_resets_;
    bool          unreported_;
    bool          reported_forever_;
    std::int32_t  format_;
    Context       contexts_[max_contexts];
    std::uint32_t current_;
    std::uint32_t last_error_;
    Texture       textures_[max_textures];
    std::uint32_t bound_texture_;
    std::uint32_t objects_;
    std::uint32_t next_name_;
    std::uint32_t program_;
    std::uint32_t linked_;
    std::uint32_t uncompiled_;
    std::uint32_t array_;
    std::uint8_t  back_[4];
    std::uint8_t  clear_[4];
    std::uint32_t misuses_;
};

}  // namespace av

#endif  // AV_SCRIPTED_GL_HPP
