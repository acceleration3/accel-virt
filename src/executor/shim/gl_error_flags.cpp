#include "executor/shim/gl_error_flags.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_context.hpp"

#include <stdint.h>

namespace av {

namespace {

constexpr GLenum no_error = 0u;
constexpr uint32_t error_flags = 8u;

}

GlErrorFlags::GlErrorFlags() : get_error_(nullptr)
{
    AV_SCOPE("GlErrorFlags::GlErrorFlags");
}

void GlErrorFlags::resolve(const GlContext &context)
{
    AV_SCOPE("GlErrorFlags::resolve");
    get_error_ = reinterpret_cast<GetError>(context.resolve("glGetError"));
}

bool GlErrorFlags::resolved() const
{
    AV_SCOPE("GlErrorFlags::resolved");
    return get_error_ != nullptr;
}

bool GlErrorFlags::clear()
{
    AV_SCOPE("GlErrorFlags::clear");
    bool none = true;
    for (uint32_t flag = 0u; flag < error_flags && get_error_() != no_error; flag++) none = false;
    return none;
}

}
