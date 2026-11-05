#include "executor/opengl/gl_error_queue.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/submission/gl_error_word.hpp"

namespace av {

namespace {

constexpr uint32_t gl_context_lost = 0x0507u;

}

GlErrorQueue::GlErrorQueue(const glproto::RealGl &gl, GlErrorWord &word) : gl_(gl), word_(word), lost_(false)
{
    AV_SCOPE("GlErrorQueue::GlErrorQueue");
}

void GlErrorQueue::collect()
{
    AV_SCOPE("GlErrorQueue::collect");
    for (uint32_t flag = 0u; flag < most_collected; flag++) {
        const uint32_t error = take();
        if (error == 0u) break;
        word_.record(error);
    }
}

uint32_t GlErrorQueue::take()
{
    AV_SCOPE("GlErrorQueue::take");
    if (gl_.glGetError == nullptr) return 0u;
    const uint32_t error = gl_.glGetError();
    if (error == gl_context_lost) lost_ = true;
    return error;
}

void GlErrorQueue::record(uint32_t error)
{
    AV_SCOPE("GlErrorQueue::record");
    word_.record(error);
}

void GlErrorQueue::recover()
{
    AV_SCOPE("GlErrorQueue::recover");
    lost_ = false;
}

bool GlErrorQueue::lost() const
{
    AV_SCOPE("GlErrorQueue::lost");
    return lost_;
}

bool GlErrorQueue::invariants_hold() const
{
    AV_SCOPE("GlErrorQueue::invariants_hold");
    return !lost_ || gl_.glGetError != nullptr;
}

}
