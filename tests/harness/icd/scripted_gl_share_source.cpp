#include "harness/icd/scripted_gl_share_source.hpp"

namespace av {

ScriptedGlShareSource::ScriptedGlShareSource()
    : current_(nullptr), name_(0u), luid_(0u), asked_(0u), runs_(0u), commands_at_run_(0u), count_(nullptr), counter_(nullptr)
{
}

void ScriptedGlShareSource::make_current(void *gl_context, std::uint32_t name, std::uint64_t luid)
{
    current_ = gl_context;
    name_ = name;
    luid_ = luid;
}

std::uint32_t ScriptedGlShareSource::share_name(void *gl_context)
{
    asked_++;
    return gl_context != nullptr && gl_context == current_ ? name_ : 0u;
}

std::uint64_t ScriptedGlShareSource::adapter_luid(void *gl_context)
{
    return gl_context != nullptr && gl_context == current_ ? luid_ : 0u;
}

void ScriptedGlShareSource::run_held_calls()
{
    runs_++;
    commands_at_run_ = count_ != nullptr ? count_(counter_) : 0u;
}

std::uint32_t ScriptedGlShareSource::asked() const { return asked_; }

std::uint32_t ScriptedGlShareSource::runs() const { return runs_; }

std::uint32_t ScriptedGlShareSource::commands_at_run() const { return commands_at_run_; }

void ScriptedGlShareSource::count_commands_with(std::uint32_t (*count)(void *), void *counter)
{
    count_ = count;
    counter_ = counter;
}

}
