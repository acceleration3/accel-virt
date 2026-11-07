#include "executor/shim/gl_extensions.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_context.hpp"

#include <string.h>

namespace av {

namespace {
constexpr GLenum num_extensions = 0x821Du;
constexpr GLenum extensions_name = 0x1F03u;
}

GlExtensions::GlExtensions() : get_integerv_(nullptr), get_stringi_(nullptr)
{
    AV_SCOPE("GlExtensions::GlExtensions");
}

void GlExtensions::configure(GlContext &context)
{
    AV_SCOPE("GlExtensions::configure");
    get_integerv_ = reinterpret_cast<void(APIENTRY *)(GLenum, GLint *)>(context.resolve("glGetIntegerv"));
    get_stringi_ = reinterpret_cast<const GLubyte *(APIENTRY *)(GLenum, GLuint)>(context.resolve("glGetStringi"));
}

bool GlExtensions::lists(const char *extension) const
{
    AV_SCOPE("GlExtensions::lists");
    if (get_integerv_ == nullptr || get_stringi_ == nullptr) return false;
    GLint count = 0;
    get_integerv_(num_extensions, &count);
    for (GLint i = 0; i < count; i++) {
        const GLubyte *const name = get_stringi_(extensions_name, static_cast<GLuint>(i));
        if (name != nullptr && strcmp(reinterpret_cast<const char *>(name), extension) == 0) return true;
    }
    return false;
}

}
