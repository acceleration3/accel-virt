#include "harness/icd/opengl32_gl_entry.hpp"

#include "harness/icd/fake_opengl32.hpp"
#include "harness/icd/scripted_thread_slot.hpp"

#include "gl_types.h"

namespace av {

namespace {

/* opengl32's table slots (tools/codegen/profiles/opengl_proc_table.txt). */
constexpr std::uint32_t slot_tex_image_2d = 183u, slot_clear = 203u, slot_clear_color = 206u, slot_finish = 216u,
                        slot_read_pixels = 256u, slot_get_error = 261u, slot_get_string = 275u,
                        slot_get_tex_image = 281u, slot_viewport = 305u, slot_bind_texture = 307u,
                        slot_draw_arrays = 310u, slot_delete_textures = 327u, slot_gen_textures = 328u;
constexpr std::uint32_t gl_texture_2d = 0x0DE1u, gl_rgba = 0x1908u, gl_rgba8 = 0x8058u, gl_unsigned_byte = 0x1401u;

}  // namespace

Opengl32GlEntry::Opengl32GlEntry(FakeOpengl32 &opengl32, ScriptedThreadSlot &threads, void *dc, std::uint32_t width,
                                 std::uint32_t height)
    : opengl32_(opengl32), threads_(threads), dc_(dc), width_(width), height_(height)
{
}

template <typename Function>
Function Opengl32GlEntry::named(const char *name) const
{
    return reinterpret_cast<Function>(opengl32_.get_proc_address(name));
}

std::uint32_t Opengl32GlEntry::width() const { return width_; }
std::uint32_t Opengl32GlEntry::height() const { return height_; }
std::int32_t  Opengl32GlEntry::choose_pixel_format() { return 1; }
bool Opengl32GlEntry::set_pixel_format(std::int32_t format) { return opengl32_.set_pixel_format(dc_, format); }
void *Opengl32GlEntry::create_context() { return opengl32_.create_context(dc_); }

void *Opengl32GlEntry::create_context_attribs(const std::int32_t *attributes)
{
    using Create = void *(APIENTRY *)(void *, void *, const std::int32_t *);
    const Create create = named<Create>("wglCreateContextAttribsARB");
    /* What opengl32 leaves a caller with nothing current: no function. */
    if (create == nullptr) {
        threads_.set_last_error(proc_not_found);
        return nullptr;
    }
    return create(dc_, nullptr, attributes);
}

bool Opengl32GlEntry::make_current(void *context) { return opengl32_.make_current(dc_, context); }
bool Opengl32GlEntry::delete_context(void *context) { return opengl32_.delete_context(context); }
bool Opengl32GlEntry::share_lists(void *first, void *second) { return opengl32_.share_lists(first, second); }
bool Opengl32GlEntry::swap_buffers() { return opengl32_.swap_buffers(dc_); }
std::uint32_t Opengl32GlEntry::last_error() const { return threads_.last_error(); }

/* The second thread is a thread number the probe's own never uses. */
std::uint32_t Opengl32GlEntry::make_current_elsewhere(void *context)
{
    const std::uint32_t here = threads_.thread_id();
    threads_.switch_to(elsewhere);
    threads_.clear_last_error();
    const bool          made = opengl32_.make_current(dc_, context);
    const std::uint32_t error = made ? 0u : threads_.last_error();
    if (made) static_cast<void>(opengl32_.make_current(dc_, nullptr));
    threads_.switch_to(here);
    return error;
}

std::int32_t Opengl32GlEntry::force_fault()
{
    using Force = std::int32_t (APIENTRY *)();
    const auto hook = reinterpret_cast<Force>(opengl32_.get_proc_address("rvkForceContextFault"));
    return hook != nullptr ? hook() : -1;
}

std::uint32_t Opengl32GlEntry::reset_status()
{
    using ResetFn = GLenum (APIENTRY *)();
    const auto query = reinterpret_cast<ResetFn>(opengl32_.get_proc_address("glGetGraphicsResetStatus"));
    return query != nullptr ? query() : 0u;
}

const char *Opengl32GlEntry::string(std::uint32_t name)
{
    using GetString = const GLubyte *(APIENTRY *)(GLenum);
    return reinterpret_cast<const char *>(opengl32_.gl<GetString>(slot_get_string)(name));
}

std::uint32_t Opengl32GlEntry::error() { return opengl32_.gl<GLenum (APIENTRY *)()>(slot_get_error)(); }

void Opengl32GlEntry::viewport(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height)
{
    opengl32_.gl<void (APIENTRY *)(GLint, GLint, GLsizei, GLsizei)>(slot_viewport)(x, y, width, height);
}

void Opengl32GlEntry::clear_colour(float red, float green, float blue, float alpha)
{
    opengl32_.gl<void (APIENTRY *)(GLfloat, GLfloat, GLfloat, GLfloat)>(slot_clear_color)(red, green, blue, alpha);
}

void Opengl32GlEntry::clear(std::uint32_t mask) { opengl32_.gl<void (APIENTRY *)(GLbitfield)>(slot_clear)(mask); }

void Opengl32GlEntry::draw_arrays(std::uint32_t mode, std::int32_t first, std::int32_t count)
{
    opengl32_.gl<void (APIENTRY *)(GLenum, GLint, GLsizei)>(slot_draw_arrays)(mode, first, count);
}

void Opengl32GlEntry::finish() { opengl32_.gl<void (APIENTRY *)()>(slot_finish)(); }

void Opengl32GlEntry::read_pixels(std::int32_t width, std::int32_t height, std::uint8_t *rgba)
{
    opengl32_.gl<void (APIENTRY *)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)>(slot_read_pixels)(
        0, 0, width, height, gl_rgba, gl_unsigned_byte, rgba);
}

std::uint32_t Opengl32GlEntry::create_shader(std::uint32_t type)
{
    return named<GLuint (APIENTRY *)(GLenum)>("glCreateShader")(type);
}

void Opengl32GlEntry::shader_source(std::uint32_t shader, const char *source)
{
    /* The generated entry takes the list as the registry's (not const) pointer type. */
    const GLchar *sources[] = {source};
    named<void (APIENTRY *)(GLuint, GLsizei, const GLchar **, const GLint *)>("glShaderSource")(shader, 1, sources,
                                                                                            nullptr);
}

void Opengl32GlEntry::compile_shader(std::uint32_t shader)
{
    named<void (APIENTRY *)(GLuint)>("glCompileShader")(shader);
}

std::int32_t Opengl32GlEntry::shader_parameter(std::uint32_t shader, std::uint32_t pname)
{
    GLint value = 0;
    named<void (APIENTRY *)(GLuint, GLenum, GLint *)>("glGetShaderiv")(shader, pname, &value);
    return value;
}

void Opengl32GlEntry::delete_shader(std::uint32_t shader) { named<void (APIENTRY *)(GLuint)>("glDeleteShader")(shader); }
std::uint32_t Opengl32GlEntry::create_program() { return named<GLuint (APIENTRY *)()>("glCreateProgram")(); }

void Opengl32GlEntry::attach_shader(std::uint32_t program, std::uint32_t shader)
{
    named<void (APIENTRY *)(GLuint, GLuint)>("glAttachShader")(program, shader);
}

void Opengl32GlEntry::link_program(std::uint32_t program)
{
    named<void (APIENTRY *)(GLuint)>("glLinkProgram")(program);
}

std::int32_t Opengl32GlEntry::program_parameter(std::uint32_t program, std::uint32_t pname)
{
    GLint value = 0;
    named<void (APIENTRY *)(GLuint, GLenum, GLint *)>("glGetProgramiv")(program, pname, &value);
    return value;
}

void Opengl32GlEntry::use_program(std::uint32_t program) { named<void (APIENTRY *)(GLuint)>("glUseProgram")(program); }

void Opengl32GlEntry::delete_program(std::uint32_t program)
{
    named<void (APIENTRY *)(GLuint)>("glDeleteProgram")(program);
}

std::uint32_t Opengl32GlEntry::gen_vertex_array()
{
    GLuint array = 0u;
    named<void (APIENTRY *)(GLsizei, GLuint *)>("glGenVertexArrays")(1, &array);
    return array;
}

void Opengl32GlEntry::bind_vertex_array(std::uint32_t array)
{
    named<void (APIENTRY *)(GLuint)>("glBindVertexArray")(array);
}

void Opengl32GlEntry::delete_vertex_array(std::uint32_t array)
{
    const GLuint arrays[] = {array};
    named<void (APIENTRY *)(GLsizei, const GLuint *)>("glDeleteVertexArrays")(1, arrays);
}

std::uint32_t Opengl32GlEntry::gen_texture()
{
    GLuint texture = 0u;
    opengl32_.gl<void (APIENTRY *)(GLsizei, GLuint *)>(slot_gen_textures)(1, &texture);
    return texture;
}

void Opengl32GlEntry::bind_texture(std::uint32_t texture)
{
    opengl32_.gl<void (APIENTRY *)(GLenum, GLuint)>(slot_bind_texture)(gl_texture_2d, texture);
}

void Opengl32GlEntry::texture_image(std::int32_t width, std::int32_t height, const std::uint8_t *rgba)
{
    opengl32_.gl<void (APIENTRY *)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)>(
        slot_tex_image_2d)(gl_texture_2d, 0, static_cast<GLint>(gl_rgba8), width, height, 0, gl_rgba,
                           gl_unsigned_byte, rgba);
}

void Opengl32GlEntry::read_texture(std::uint8_t *rgba)
{
    opengl32_.gl<void (APIENTRY *)(GLenum, GLint, GLenum, GLenum, void *)>(slot_get_tex_image)(
        gl_texture_2d, 0, gl_rgba, gl_unsigned_byte, rgba);
}

void Opengl32GlEntry::delete_texture(std::uint32_t texture)
{
    const GLuint textures[] = {texture};
    opengl32_.gl<void (APIENTRY *)(GLsizei, const GLuint *)>(slot_delete_textures)(1, textures);
}

}  // namespace av
