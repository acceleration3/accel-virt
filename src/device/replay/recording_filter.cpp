#include "device/replay/recording_filter.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/command_api.hpp"
#include "device/recording/record_header.hpp"
#include "device/replay/recorded_command.hpp"

namespace av {

namespace {
bool holds(const char *text, const char *fragment)
{
    AV_SCOPE("holds");
    for (size_t start = 0u; text[start] != '\0'; start++) {
        size_t index = 0u;
        while (fragment[index] != '\0' && text[start + index] == fragment[index]) index++;
        if (fragment[index] == '\0') return true;
    }
    return false;
}

}

RecordingFilter::RecordingFilter()
    : context_(any_context), api_(any_api), first_fence_(0u), last_fence_(~uint64_t{0}), fragment_{}
{
    AV_SCOPE("RecordingFilter::RecordingFilter");
}

void RecordingFilter::keep_context(uint32_t context)
{
    AV_SCOPE("RecordingFilter::keep_context");
    context_ = context;
}

Status RecordingFilter::keep_api(const char *name)
{
    AV_SCOPE("RecordingFilter::keep_api");
    CommandApi named(any_api);
    if (!named.take_name(name)) return Status::invalid_parameter;
    api_ = named.api();
    return Status::ok;
}

Status RecordingFilter::keep_fences(uint64_t first, uint64_t last)
{
    AV_SCOPE("RecordingFilter::keep_fences");
    if (last < first) return Status::invalid_parameter;
    first_fence_ = first;
    last_fence_ = last;
    return Status::ok;
}

Status RecordingFilter::keep_command(const char *fragment)
{
    AV_SCOPE("RecordingFilter::keep_command");
    if (fragment == nullptr) return Status::invalid_parameter;
    size_t length = 0u;
    while (fragment[length] != '\0') {
        length++;
        if (length == longest_fragment) return Status::invalid_parameter;
    }
    for (size_t index = 0u; index <= length; index++) fragment_[index] = fragment[index];
    return Status::ok;
}

uint32_t RecordingFilter::context() const
{
    AV_SCOPE("RecordingFilter::context");
    return context_;
}

uint16_t RecordingFilter::api() const
{
    AV_SCOPE("RecordingFilter::api");
    return api_;
}

bool RecordingFilter::names_a_command() const
{
    AV_SCOPE("RecordingFilter::names_a_command");
    return fragment_[0] != '\0';
}

bool RecordingFilter::keeps(const RecordHeader &record) const
{
    AV_SCOPE("RecordingFilter::keeps");
    if (context_ != any_context && record.context() != context_) return false;
    if (record.fence() == 0u) return true;
    return record.fence() >= first_fence_ && record.fence() <= last_fence_;
}

bool RecordingFilter::keeps(const RecordedCommand &command) const
{
    AV_SCOPE("RecordingFilter::keeps");
    if (api_ != any_api && command.header().api() != api_) return false;
    return fragment_[0] == '\0' || holds(command.name(), fragment_);
}

bool RecordingFilter::invariants_hold() const
{
    AV_SCOPE("RecordingFilter::invariants_hold");
    if (first_fence_ > last_fence_) return false;
    return fragment_[longest_fragment - 1u] == '\0';
}

}
