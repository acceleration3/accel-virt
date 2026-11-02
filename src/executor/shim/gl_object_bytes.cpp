#include "executor/shim/gl_object_bytes.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_level.hpp"

namespace av {

namespace {
constexpr GLenum copy_read_buffer = 0x8F36u;
constexpr GLenum copy_write_buffer = 0x8F37u;
constexpr GLenum buffer_size_query = 0x8764u;
constexpr GLenum pack_alignment = 0x0D05u;
constexpr GLenum unpack_alignment = 0x0CF5u;
constexpr GLenum texture_width = 0x1000u;
constexpr GLenum texture_height = 0x1001u;
constexpr GLenum texture_depth = 0x8071u;
constexpr GLenum texture_internal_format = 0x1003u;

}

GlObjectBytes::GlObjectBytes()
    : is_buffer_(nullptr), bind_buffer_(nullptr), get_buffer_parameter_(nullptr), get_buffer_sub_data_(nullptr),
      buffer_sub_data_(nullptr), is_texture_(nullptr), bind_texture_(nullptr), get_level_parameter_(nullptr),
      get_tex_image_(nullptr), tex_sub_image_1d_(nullptr), tex_sub_image_2d_(nullptr), tex_sub_image_3d_(nullptr),
      pixel_store_(nullptr), errors_(), finish_(nullptr), context_(nullptr)
{
    AV_SCOPE("GlObjectBytes::GlObjectBytes");
}

void GlObjectBytes::configure(GlContext &context)
{
    AV_SCOPE("GlObjectBytes::configure");
    context_ = &context;
    if (!context.make_current()) return;
    is_buffer_ = reinterpret_cast<decltype(is_buffer_)>(context.resolve("glIsBuffer"));
    bind_buffer_ = reinterpret_cast<decltype(bind_buffer_)>(context.resolve("glBindBuffer"));
    get_buffer_parameter_ =
        reinterpret_cast<decltype(get_buffer_parameter_)>(context.resolve("glGetBufferParameteri64v"));
    get_buffer_sub_data_ = reinterpret_cast<decltype(get_buffer_sub_data_)>(context.resolve("glGetBufferSubData"));
    buffer_sub_data_ = reinterpret_cast<decltype(buffer_sub_data_)>(context.resolve("glBufferSubData"));
    is_texture_ = reinterpret_cast<decltype(is_texture_)>(context.resolve("glIsTexture"));
    bind_texture_ = reinterpret_cast<decltype(bind_texture_)>(context.resolve("glBindTexture"));
    get_level_parameter_ =
        reinterpret_cast<decltype(get_level_parameter_)>(context.resolve("glGetTexLevelParameteriv"));
    get_tex_image_ = reinterpret_cast<decltype(get_tex_image_)>(context.resolve("glGetTexImage"));
    tex_sub_image_1d_ = reinterpret_cast<decltype(tex_sub_image_1d_)>(context.resolve("glTexSubImage1D"));
    tex_sub_image_2d_ = reinterpret_cast<decltype(tex_sub_image_2d_)>(context.resolve("glTexSubImage2D"));
    tex_sub_image_3d_ = reinterpret_cast<decltype(tex_sub_image_3d_)>(context.resolve("glTexSubImage3D"));
    pixel_store_ = reinterpret_cast<decltype(pixel_store_)>(context.resolve("glPixelStorei"));
    errors_.resolve(context);
    finish_ = reinterpret_cast<decltype(finish_)>(context.resolve("glFinish"));
}

void GlObjectBytes::give_back()
{
    AV_SCOPE("GlObjectBytes::give_back");
    context_ = nullptr;
}

bool GlObjectBytes::moves_bytes() const
{
    AV_SCOPE("GlObjectBytes::moves_bytes");
    return is_buffer_ != nullptr && bind_buffer_ != nullptr && get_buffer_parameter_ != nullptr &&
           get_buffer_sub_data_ != nullptr && buffer_sub_data_ != nullptr && is_texture_ != nullptr &&
           bind_texture_ != nullptr && get_level_parameter_ != nullptr && get_tex_image_ != nullptr &&
           tex_sub_image_1d_ != nullptr && tex_sub_image_2d_ != nullptr && tex_sub_image_3d_ != nullptr &&
           pixel_store_ != nullptr && errors_.resolved() && finish_ != nullptr;
}

GlContext *GlObjectBytes::current()
{
    AV_SCOPE("GlObjectBytes::current");
    if (context_ == nullptr || !moves_bytes() || !context_->make_current()) return nullptr;
    return context_;
}

bool GlObjectBytes::quiet()
{
    AV_SCOPE("GlObjectBytes::quiet");
    return errors_.clear();
}

bool GlObjectBytes::buffer_size(uint32_t buffer, uint64_t &bytes)
{
    AV_SCOPE("GlObjectBytes::buffer_size");
    if (current() == nullptr || buffer == 0u || is_buffer_(buffer) == 0u) return false;
    static_cast<void>(quiet());
    GLint64 size = 0;
    bind_buffer_(copy_read_buffer, buffer);
    get_buffer_parameter_(copy_read_buffer, buffer_size_query, &size);
    bind_buffer_(copy_read_buffer, 0u);
    if (!quiet() || size <= 0) return false;
    bytes = static_cast<uint64_t>(size);
    return true;
}

bool GlObjectBytes::read_buffer(uint32_t buffer, uint64_t bytes, void *into)
{
    AV_SCOPE("GlObjectBytes::read_buffer");
    if (current() == nullptr || buffer == 0u || is_buffer_(buffer) == 0u) return false;
    static_cast<void>(quiet());
    bind_buffer_(copy_read_buffer, buffer);
    get_buffer_sub_data_(copy_read_buffer, 0, static_cast<GLsizeiptr>(bytes), into);
    bind_buffer_(copy_read_buffer, 0u);
    return quiet();
}

bool GlObjectBytes::write_buffer(uint32_t buffer, uint64_t bytes, const void *from)
{
    AV_SCOPE("GlObjectBytes::write_buffer");
    if (current() == nullptr || buffer == 0u || is_buffer_(buffer) == 0u) return false;
    static_cast<void>(quiet());
    bind_buffer_(copy_write_buffer, buffer);
    buffer_sub_data_(copy_write_buffer, 0, static_cast<GLsizeiptr>(bytes), from);
    bind_buffer_(copy_write_buffer, 0u);
    return quiet();
}

bool GlObjectBytes::bound(uint32_t target, uint32_t texture)
{
    AV_SCOPE("GlObjectBytes::bound");
    if (texture == 0u || is_texture_(texture) == 0u) return false;
    static_cast<void>(quiet());
    bind_texture_(target, texture);
    return quiet();
}

bool GlObjectBytes::level_shape(const GlLevel &level, uint32_t &internal_format, uint32_t &width,
                                uint32_t &height, uint32_t &depth)
{
    AV_SCOPE("GlObjectBytes::level_shape");
    if (current() == nullptr || !bound(level.bind_target(), level.texture())) return false;
    GLint format = 0, wide = 0, high = 0, deep = 0;
    get_level_parameter_(level.image_target(), level.level(), texture_internal_format, &format);
    get_level_parameter_(level.image_target(), level.level(), texture_width, &wide);
    get_level_parameter_(level.image_target(), level.level(), texture_height, &high);
    get_level_parameter_(level.image_target(), level.level(), texture_depth, &deep);
    bind_texture_(level.bind_target(), 0u);
    if (!quiet() || wide <= 0 || high <= 0 || deep <= 0) return false;
    internal_format = static_cast<uint32_t>(format);
    width = static_cast<uint32_t>(wide);
    height = static_cast<uint32_t>(high);
    depth = static_cast<uint32_t>(deep);
    return true;
}

bool GlObjectBytes::read_level(const GlLevel &level, void *into)
{
    AV_SCOPE("GlObjectBytes::read_level");
    if (current() == nullptr || !bound(level.bind_target(), level.texture())) return false;
    pixel_store_(pack_alignment, 1);
    get_tex_image_(level.image_target(), level.level(), level.format(), level.type(), into);
    bind_texture_(level.bind_target(), 0u);
    return quiet();
}

bool GlObjectBytes::write_level(const GlLevel &level, const void *from)
{
    AV_SCOPE("GlObjectBytes::write_level");
    if (current() == nullptr || !bound(level.bind_target(), level.texture())) return false;
    pixel_store_(unpack_alignment, 1);
    const auto wide = static_cast<GLsizei>(level.width());
    const auto high = static_cast<GLsizei>(level.height());
    const auto deep = static_cast<GLsizei>(level.depth());
    if (level.dimensions() == 1u)
        tex_sub_image_1d_(level.image_target(), level.level(), 0, wide, level.format(), level.type(), from);
    else if (level.dimensions() == 2u)
        tex_sub_image_2d_(level.image_target(), level.level(), 0, 0, wide, high, level.format(), level.type(), from);
    else
        tex_sub_image_3d_(level.image_target(), level.level(), 0, 0, 0, wide, high, deep, level.format(),
                          level.type(), from);
    bind_texture_(level.bind_target(), 0u);
    return quiet();
}

void GlObjectBytes::finish()
{
    AV_SCOPE("GlObjectBytes::finish");
    if (current() != nullptr) finish_();
}

}
