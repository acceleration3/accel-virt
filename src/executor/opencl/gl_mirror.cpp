#include "executor/opencl/gl_mirror.hpp"

#include "core/trace/function_scope.hpp"

#include <CL/cl_gl.h>

namespace av {

GlMirror::GlMirror() : GlMirror(nullptr, 0u, 0u, 0u, GlLevel(), 0u, nullptr, false)
{
    AV_SCOPE("GlMirror::GlMirror");
}

GlMirror::GlMirror(const void *mem, uint32_t name, uint64_t bytes, bool written_back)
    : GlMirror(mem, CL_GL_OBJECT_BUFFER, name, 0u, GlLevel(), bytes, nullptr, written_back)
{
    AV_SCOPE("GlMirror::GlMirror");
}

GlMirror::GlMirror(const void *mem, uint32_t object_type, uint32_t name, uint32_t target, const GlLevel &level,
                   uint64_t bytes, void *image, bool written_back)
    : mem_(mem), object_type_(object_type), name_(name), target_(target), level_(level), bytes_(bytes),
      image_(image), written_back_(written_back), acquired_(false)
{
    AV_SCOPE("GlMirror::GlMirror");
}

const void *GlMirror::mem() const
{
    AV_SCOPE("GlMirror::mem");
    return mem_;
}

uint32_t GlMirror::object_type() const
{
    AV_SCOPE("GlMirror::object_type");
    return object_type_;
}

bool GlMirror::buffer() const
{
    AV_SCOPE("GlMirror::buffer");
    return object_type_ == CL_GL_OBJECT_BUFFER;
}

bool GlMirror::texture() const
{
    AV_SCOPE("GlMirror::texture");
    return object_type_ != 0u && object_type_ != CL_GL_OBJECT_BUFFER && object_type_ != CL_GL_OBJECT_RENDERBUFFER;
}

uint32_t GlMirror::name() const
{
    AV_SCOPE("GlMirror::name");
    return name_;
}

uint32_t GlMirror::target() const
{
    AV_SCOPE("GlMirror::target");
    return target_;
}

const GlLevel &GlMirror::level() const
{
    AV_SCOPE("GlMirror::level");
    return level_;
}

uint64_t GlMirror::bytes() const
{
    AV_SCOPE("GlMirror::bytes");
    return bytes_;
}

void *GlMirror::image() const
{
    AV_SCOPE("GlMirror::image");
    return image_;
}

bool GlMirror::written_back() const
{
    AV_SCOPE("GlMirror::written_back");
    return written_back_;
}

bool GlMirror::acquired() const
{
    AV_SCOPE("GlMirror::acquired");
    return acquired_;
}

void GlMirror::acquire()
{
    AV_SCOPE("GlMirror::acquire");
    acquired_ = true;
}

void GlMirror::release()
{
    AV_SCOPE("GlMirror::release");
    acquired_ = false;
}

bool GlMirror::described() const
{
    AV_SCOPE("GlMirror::described");
    return mem_ != nullptr && object_type_ != 0u && name_ != 0u && bytes_ != 0u;
}

}
