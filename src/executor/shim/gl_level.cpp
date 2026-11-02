#include "executor/shim/gl_level.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GlLevel::GlLevel() : GlLevel(0u, 0u, 0u, 0u, 0)
{
    AV_SCOPE("GlLevel::GlLevel");
}

GlLevel::GlLevel(uint32_t texture, uint32_t bind_target, uint32_t image_target, uint32_t dimensions, int32_t level)
    : texture_(texture), bind_target_(bind_target), image_target_(image_target), dimensions_(dimensions),
      level_(level), format_(0u), type_(0u), width_(0u), height_(0u), depth_(0u)
{
    AV_SCOPE("GlLevel::GlLevel");
}

void GlLevel::moves_as(uint32_t format, uint32_t type, uint32_t width, uint32_t height, uint32_t depth)
{
    AV_SCOPE("GlLevel::moves_as");
    format_ = format;
    type_ = type;
    width_ = width;
    height_ = height;
    depth_ = depth;
}

uint32_t GlLevel::texture() const
{
    AV_SCOPE("GlLevel::texture");
    return texture_;
}

uint32_t GlLevel::bind_target() const
{
    AV_SCOPE("GlLevel::bind_target");
    return bind_target_;
}

uint32_t GlLevel::image_target() const
{
    AV_SCOPE("GlLevel::image_target");
    return image_target_;
}

uint32_t GlLevel::dimensions() const
{
    AV_SCOPE("GlLevel::dimensions");
    return dimensions_;
}

int32_t GlLevel::level() const
{
    AV_SCOPE("GlLevel::level");
    return level_;
}

uint32_t GlLevel::format() const
{
    AV_SCOPE("GlLevel::format");
    return format_;
}

uint32_t GlLevel::type() const
{
    AV_SCOPE("GlLevel::type");
    return type_;
}

uint32_t GlLevel::width() const
{
    AV_SCOPE("GlLevel::width");
    return width_;
}

uint32_t GlLevel::height() const
{
    AV_SCOPE("GlLevel::height");
    return height_;
}

uint32_t GlLevel::depth() const
{
    AV_SCOPE("GlLevel::depth");
    return depth_;
}

}
