#include "harness/icd/scripted_gl_share_source.hpp"

namespace av {

ScriptedGlShareSource::ScriptedGlShareSource() : current_(nullptr), name_(0u), asked_(0u) {}

void ScriptedGlShareSource::make_current(void *gl_context, std::uint32_t name)
{
    current_ = gl_context;
    name_ = name;
}

std::uint32_t ScriptedGlShareSource::share_name(void *gl_context)
{
    asked_++;
    return gl_context != nullptr && gl_context == current_ ? name_ : 0u;
}

std::uint32_t ScriptedGlShareSource::asked() const { return asked_; }

}  // namespace av
