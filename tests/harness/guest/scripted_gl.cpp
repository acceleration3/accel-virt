#include "harness/guest/scripted_gl.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::uint32_t never = 0u;
constexpr std::uint32_t none = ~std::uint32_t{0};
constexpr std::uint32_t gl_version = 0x1F02u, gl_renderer = 0x1F01u;
constexpr std::uint32_t gl_compile_status = 0x8B81u, gl_link_status = 0x8B82u;
constexpr std::uint32_t gl_color_buffer_bit = 0x4000u, gl_triangles = 0x0004u;
constexpr std::int32_t  major_version = 0x2091, minor_version = 0x2092, profile_mask = 0x9126;
constexpr std::uint32_t error_invalid_version_arb = 0x2095u, error_invalid_profile_arb = 0x2096u;
constexpr std::uint8_t  solid[4] = {64u, 128u, 191u, 255u};
/* ScriptedGl's context handles: a row plus this, never a real address. */
constexpr std::uintptr_t handle_base = 0x4000u;

std::uint8_t unorm(float value)
{
    return static_cast<std::uint8_t>(value * 255.0f + 0.5f);
}

}  // namespace

ScriptedGl::ScriptedGl(std::uint32_t width, std::uint32_t height)
    : width_(width), height_(height), fail_at_{}, calls_{}, version_("4.6.0 scripted"), renderer_("scripted gpu"),
      error_(0u), shift_(0), accept_versions_(false), accept_profiles_(false), isolate_(false), format_(0), contexts_{}, current_(none),
      last_error_(0u), textures_{}, bound_texture_(0u), objects_(0u), next_name_(10u), program_(0u), linked_(0u), uncompiled_(0u),
      array_(0u), back_{}, clear_{}, misuses_(0u)
{
}

void ScriptedGl::fail_at(Call call, std::uint32_t nth) { fail_at_[static_cast<std::uint32_t>(call)] = nth; }

void ScriptedGl::set_strings(const char *version, const char *renderer)
{
    version_ = version;
    renderer_ = renderer;
}

void ScriptedGl::set_error(std::uint32_t error) { error_ = error; }
void ScriptedGl::shift_colour(std::int32_t shift) { shift_ = shift; }
void ScriptedGl::accept_versions() { accept_versions_ = true; }
void ScriptedGl::accept_profiles() { accept_profiles_ = true; }
void ScriptedGl::isolate_textures() { isolate_ = true; }

std::uint32_t ScriptedGl::live_contexts() const
{
    std::uint32_t live = 0u;
    for (const Context &context : contexts_) live += context.live ? 1u : 0u;
    return live;
}

std::uint32_t ScriptedGl::live_objects() const { return objects_; }
std::uint32_t ScriptedGl::misuses() const { return misuses_; }
std::uint32_t ScriptedGl::calls(Call call) const { return calls_[static_cast<std::uint32_t>(call)]; }
std::uint32_t ScriptedGl::width() const { return width_; }
std::uint32_t ScriptedGl::height() const { return height_; }

std::int32_t ScriptedGl::choose_pixel_format()
{
    if (fails(Call::choose_pixel_format)) {
        last_error_ = refused;
        return 0;
    }
    return 3;
}

bool ScriptedGl::set_pixel_format(std::int32_t format)
{
    if (fails(Call::set_pixel_format) || format_ != 0) {
        last_error_ = refused;
        return false;
    }
    format_ = format;
    return true;
}

void *ScriptedGl::create_context()
{
    if (format_ == 0) misuses_++;
    if (fails(Call::create_context)) {
        last_error_ = refused;
        return nullptr;
    }
    for (std::uint32_t row = 0u; row < max_contexts; row++) {
        if (contexts_[row].live) continue;
        contexts_[row] = Context{true, false, row + 1u};
        return reinterpret_cast<void *>(handle_base + row);
    }
    last_error_ = refused;
    return nullptr;
}

void *ScriptedGl::create_context_attribs(const std::int32_t *attributes)
{
    std::int32_t major = 1, minor = 0, profile = 0;
    for (; attributes != nullptr && attributes[0] != 0; attributes += 2) {
        if (attributes[0] == major_version) major = attributes[1];
        if (attributes[0] == minor_version) minor = attributes[1];
        if (attributes[0] == profile_mask) profile = attributes[1];
    }
    if (!accept_versions_ && major == 4 && minor > 6) {
        last_error_ = error_invalid_version_arb;
        return nullptr;
    }
    if (!accept_profiles_ && profile == 3) {
        last_error_ = error_invalid_profile_arb;
        return nullptr;
    }
    return create_context();
}

bool ScriptedGl::make_current(void *context)
{
    if (context == nullptr) {
        current_ = none;
        return true;
    }
    const std::uint32_t row = row_of(context);
    if (row == none) misuses_++;
    if (row == none || fails(Call::make_current)) {
        last_error_ = refused;
        return false;
    }
    contexts_[row].made = true;
    current_ = row;
    return true;
}

bool ScriptedGl::delete_context(void *context)
{
    const std::uint32_t row = row_of(context);
    if (row == none) {
        misuses_++;
        return false;
    }
    contexts_[row].live = false;
    if (current_ == row) current_ = none;
    return true;
}

bool ScriptedGl::share_lists(void *first, void *second)
{
    const std::uint32_t from = row_of(first);
    const std::uint32_t to = row_of(second);
    if (from == none || to == none) misuses_++;
    if (from == none || to == none || contexts_[from].made || contexts_[to].made || fails(Call::share_lists)) {
        last_error_ = refused;
        return false;
    }
    contexts_[to].group = contexts_[from].group;
    return true;
}

bool ScriptedGl::swap_buffers()
{
    if (!current_or_misuse()) return false;
    return !fails(Call::swap_buffers);
}

std::uint32_t ScriptedGl::last_error() const { return last_error_; }

const char *ScriptedGl::string(std::uint32_t name)
{
    if (!current_or_misuse()) return nullptr;
    if (name == gl_version) return version_;
    return name == gl_renderer ? renderer_ : nullptr;
}

std::uint32_t ScriptedGl::error()
{
    if (!current_or_misuse()) return 0u;
    return error_;
}

void ScriptedGl::viewport(std::int32_t, std::int32_t, std::int32_t width, std::int32_t height)
{
    if (!current_or_misuse()) return;
    if (width != static_cast<std::int32_t>(width_) || height != static_cast<std::int32_t>(height_)) misuses_++;
}

void ScriptedGl::clear_colour(float red, float green, float blue, float alpha)
{
    if (!current_or_misuse()) return;
    clear_[0] = unorm(red);
    clear_[1] = unorm(green);
    clear_[2] = unorm(blue);
    clear_[3] = unorm(alpha);
}

void ScriptedGl::clear(std::uint32_t mask)
{
    if (!current_or_misuse()) return;
    if (mask != gl_color_buffer_bit) misuses_++;
    std::memcpy(back_, clear_, sizeof back_);
}

void ScriptedGl::draw_arrays(std::uint32_t mode, std::int32_t first, std::int32_t count)
{
    if (!current_or_misuse()) return;
    if (mode != gl_triangles || first != 0 || count != 3) misuses_++;
    if (program_ == 0u || program_ != linked_ || array_ == 0u) return;
    for (std::uint32_t channel = 0u; channel < 4u; channel++) {
        const std::int32_t shifted = static_cast<std::int32_t>(solid[channel]) + shift_;
        back_[channel] = static_cast<std::uint8_t>(shifted < 0 ? 0 : shifted > 255 ? 255 : shifted);
    }
}

void ScriptedGl::finish() { static_cast<void>(current_or_misuse()); }

void ScriptedGl::read_pixels(std::int32_t width, std::int32_t height, std::uint8_t *rgba)
{
    if (!current_or_misuse()) return;
    if (width <= 0 || height <= 0 || static_cast<std::uint32_t>(width) > max_extent ||
        static_cast<std::uint32_t>(height) > max_extent) {
        misuses_++;
        return;
    }
    const auto pixels = static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height);
    for (std::uint32_t pixel = 0u; pixel < pixels; pixel++) std::memcpy(rgba + pixel * 4u, back_, sizeof back_);
}

std::uint32_t ScriptedGl::create_shader(std::uint32_t)
{
    if (!current_or_misuse() || fails(Call::create_shader)) return 0u;
    objects_++;
    return next_name_++;
}

void ScriptedGl::shader_source(std::uint32_t shader, const char *source)
{
    if (!current_or_misuse()) return;
    if (shader == 0u || source == nullptr || source[0] != '#') misuses_++;
}

void ScriptedGl::compile_shader(std::uint32_t shader)
{
    if (!current_or_misuse()) return;
    if (fails(Call::compile_shader)) uncompiled_ = shader;
}

std::int32_t ScriptedGl::shader_parameter(std::uint32_t shader, std::uint32_t pname)
{
    if (!current_or_misuse() || pname != gl_compile_status) return 0;
    return shader == uncompiled_ ? 0 : 1;
}

void ScriptedGl::delete_shader(std::uint32_t)
{
    if (current_or_misuse()) objects_--;
}

std::uint32_t ScriptedGl::create_program()
{
    if (!current_or_misuse() || fails(Call::create_program)) return 0u;
    objects_++;
    return next_name_++;
}

void ScriptedGl::attach_shader(std::uint32_t program, std::uint32_t shader)
{
    if (current_or_misuse() && (program == 0u || shader == 0u)) misuses_++;
}

void ScriptedGl::link_program(std::uint32_t program)
{
    if (!current_or_misuse()) return;
    linked_ = fails(Call::link_program) ? 0u : program;
}

std::int32_t ScriptedGl::program_parameter(std::uint32_t program, std::uint32_t pname)
{
    if (!current_or_misuse() || pname != gl_link_status) return 0;
    return program != 0u && program == linked_ ? 1 : 0;
}

void ScriptedGl::use_program(std::uint32_t program)
{
    if (current_or_misuse()) program_ = program;
}

void ScriptedGl::delete_program(std::uint32_t)
{
    if (current_or_misuse()) objects_--;
}

std::uint32_t ScriptedGl::gen_vertex_array()
{
    if (!current_or_misuse()) return 0u;
    objects_++;
    return next_name_++;
}

void ScriptedGl::bind_vertex_array(std::uint32_t array)
{
    if (current_or_misuse()) array_ = array;
}

void ScriptedGl::delete_vertex_array(std::uint32_t)
{
    if (current_or_misuse()) objects_--;
}

std::uint32_t ScriptedGl::gen_texture()
{
    if (!current_or_misuse()) return 0u;
    for (std::uint32_t row = 0u; row < max_textures; row++) {
        if (textures_[row].live) continue;
        textures_[row] = Texture{true, contexts_[current_].group, {}};
        objects_++;
        return row + 1u;
    }
    return 0u;
}

void ScriptedGl::bind_texture(std::uint32_t texture)
{
    if (current_or_misuse()) bound_texture_ = texture;
}

void ScriptedGl::texture_image(std::int32_t width, std::int32_t height, const std::uint8_t *rgba)
{
    if (!current_or_misuse()) return;
    if (bound_texture_ == 0u || width * height * 4 != sizeof textures_[0].texels) {
        misuses_++;
        return;
    }
    std::memcpy(textures_[bound_texture_ - 1u].texels, rgba, sizeof textures_[0].texels);
}

void ScriptedGl::read_texture(std::uint8_t *rgba)
{
    if (!current_or_misuse()) return;
    if (bound_texture_ == 0u) {
        misuses_++;
        return;
    }
    const Texture &texture = textures_[bound_texture_ - 1u];
    const bool     visible = !isolate_ && texture.live && texture.group == contexts_[current_].group;
    if (visible) std::memcpy(rgba, texture.texels, sizeof texture.texels);
}

void ScriptedGl::delete_texture(std::uint32_t texture)
{
    if (!current_or_misuse()) return;
    if (texture == 0u || texture > max_textures || !textures_[texture - 1u].live) {
        misuses_++;
        return;
    }
    textures_[texture - 1u].live = false;
    objects_--;
}

bool ScriptedGl::fails(Call call)
{
    const auto index = static_cast<std::uint32_t>(call);
    calls_[index]++;
    return fail_at_[index] != never && calls_[index] == fail_at_[index];
}

std::uint32_t ScriptedGl::row_of(void *context) const
{
    const std::uintptr_t row = reinterpret_cast<std::uintptr_t>(context) - handle_base;
    return row < max_contexts && contexts_[row].live ? static_cast<std::uint32_t>(row) : none;
}

bool ScriptedGl::current_or_misuse()
{
    if (current_ != none) return true;
    misuses_++;
    return false;
}

}  // namespace av
