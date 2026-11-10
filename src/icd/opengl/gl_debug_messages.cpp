#include "icd/opengl/gl_debug_messages.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_transport.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

namespace {

using DebugProc = void (APIENTRY *)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                    const GLchar *message, const void *user);

}

GlDebugMessages::GlDebugMessages(bool output)
    : function_(0u), user_(0u), output_(output), synchronous_(false), delivering_(false)
{
    AV_SCOPE("GlDebugMessages::GlDebugMessages");
}

void GlDebugMessages::set_callback(uint64_t function, uint64_t user)
{
    AV_SCOPE("GlDebugMessages::set_callback");
    function_ = function;
    user_ = user;
}

uint64_t GlDebugMessages::function() const
{
    AV_SCOPE("GlDebugMessages::function");
    return function_;
}

uint64_t GlDebugMessages::user() const
{
    AV_SCOPE("GlDebugMessages::user");
    return user_;
}

bool GlDebugMessages::tracks(uint32_t capability) const
{
    AV_SCOPE("GlDebugMessages::tracks");
    return capability == debug_output || capability == debug_output_synchronous;
}

void GlDebugMessages::enable(uint32_t capability, bool on)
{
    AV_SCOPE("GlDebugMessages::enable");
    if (capability == debug_output) output_ = on;
    if (capability == debug_output_synchronous) synchronous_ = on;
}

bool GlDebugMessages::listening() const
{
    AV_SCOPE("GlDebugMessages::listening");
    return function_ != 0u && output_;
}

bool GlDebugMessages::synchronous() const
{
    AV_SCOPE("GlDebugMessages::synchronous");
    return listening() && synchronous_;
}

void GlDebugMessages::deliver(ClientTransport &wire)
{
    AV_SCOPE("GlDebugMessages::deliver");
    if (delivering_ || !listening()) return;
    delivering_ = true;
    for (uint32_t taken = 0u; taken < most_per_delivery && listening(); taken++) {
        GLenum  source = 0u, type = 0u, severity = 0u;
        GLuint  id = 0u;
        GLsizei length = 0;
        GLchar  text[text_bytes] = {};
        if (glproto::av_glGetDebugMessageLog(wire, 1u, static_cast<GLsizei>(text_bytes), &source, &type, &id,
                                             &severity, &length, text) == 0u)
            break;
        text[text_bytes - 1u] = '\0';
        const GLsizei kept = length > 0 ? length - 1 : 0;
        reinterpret_cast<DebugProc>(static_cast<uintptr_t>(function_))(
            source, type, id, severity, kept, text, reinterpret_cast<const void *>(static_cast<uintptr_t>(user_)));
    }
    delivering_ = false;
}

bool GlDebugMessages::invariants_hold() const
{
    AV_SCOPE("GlDebugMessages::invariants_hold");
    return !delivering_;
}

}
