#include "device/recording/recording_tool_request.hpp"

#include "core/trace/function_scope.hpp"
#include "device/recording/recording_filter.hpp"

namespace av {

namespace {

bool same(const char *left, const char *right)
{
    AV_SCOPE("same");
    for (size_t index = 0u;; index++) {
        if (left[index] != right[index]) return false;
        if (left[index] == '\0') return true;
    }
}

/* A decimal number, refusing anything that is not one and anything that would
 * pass what 64 bits hold. */
bool number(const char *text, uint64_t &value)
{
    AV_SCOPE("number");
    value = 0u;
    if (text[0] == '\0') return false;
    for (size_t index = 0u; text[index] != '\0'; index++) {
        if (text[index] < '0' || text[index] > '9') return false;
        const uint64_t digit = static_cast<uint64_t>(text[index] - '0');
        if (value > (~uint64_t{0} - digit) / 10u) return false;
        value = value * 10u + digit;
    }
    return true;
}

/* One hexadecimal digit's value, or 16. */
uint8_t digit_of(char text)
{
    AV_SCOPE("digit_of");
    if (text >= '0' && text <= '9') return static_cast<uint8_t>(text - '0');
    if (text >= 'a' && text <= 'f') return static_cast<uint8_t>(text - 'a' + 10);
    if (text >= 'A' && text <= 'F') return static_cast<uint8_t>(text - 'A' + 10);
    return 16u;
}

}  // namespace

RecordingToolRequest::RecordingToolRequest()
    : action_(Action::none),
      recording_(nullptr),
      executor_(nullptr),
      payload_bytes_(0u),
      gpu_given_(false),
      gpu_{},
      reason_("")
{
    AV_SCOPE("RecordingToolRequest::RecordingToolRequest");
}

Status RecordingToolRequest::parse(int count, const char *const *arguments, RecordingFilter &filter)
{
    AV_SCOPE("RecordingToolRequest::parse");
    if (count < 3 || arguments == nullptr) return refuse("usage: rvkrecord print|replay <recording> ...");
    if (same(arguments[1], "print")) action_ = Action::print;
    else if (same(arguments[1], "replay")) action_ = Action::replay;
    else return refuse("the first argument is print or replay");
    recording_ = arguments[2];
    for (int index = 3; index < count; index += 2) {
        if (index + 1 == count) return refuse("a flag was given with no value");
        const Status taken = take_flag(arguments[index], arguments[index + 1], filter);
        if (taken != Status::ok) return taken;
    }
    if (action_ == Action::replay && executor_ == nullptr) return refuse("replay needs --executor");
    return Status::ok;
}

Status RecordingToolRequest::take_flag(const char *flag, const char *value, RecordingFilter &filter)
{
    AV_SCOPE("RecordingToolRequest::take_flag");
    uint64_t number_value = 0u;
    if (same(flag, "--context")) {
        if (!number(value, number_value) || number_value > 0xFFFFFFFFu)
            return refuse("--context wants a context id");
        filter.keep_context(static_cast<uint32_t>(number_value));
        return Status::ok;
    }
    if (same(flag, "--api"))
        return filter.keep_api(value) == Status::ok
                   ? Status::ok
                   : refuse("--api wants transport, vulkan, opencl or opengl");
    if (same(flag, "--fence")) return take_fences(value, filter);
    if (same(flag, "--command"))
        return filter.keep_command(value) == Status::ok ? Status::ok
                                                        : refuse("--command wants a shorter name");
    if (same(flag, "--bytes")) {
        if (!number(value, number_value) || number_value > 0xFFFFFFFFu)
            return refuse("--bytes wants a count");
        payload_bytes_ = static_cast<uint32_t>(number_value);
        return Status::ok;
    }
    if (same(flag, "--executor")) {
        executor_ = value;
        return Status::ok;
    }
    if (same(flag, "--gpu")) return take_gpu(value);
    return refuse("a flag this tool does not know");
}

/* `A` or `A:B`, both decimal. */
Status RecordingToolRequest::take_fences(const char *value, RecordingFilter &filter)
{
    AV_SCOPE("RecordingToolRequest::take_fences");
    char     first[24] = {};
    size_t   index = 0u;
    for (; value[index] != '\0' && value[index] != ':'; index++) {
        if (index + 1u == sizeof first) return refuse("--fence wants a fence or a range");
        first[index] = value[index];
    }
    uint64_t begin = 0u;
    if (!number(first, begin)) return refuse("--fence wants a fence or a range");
    if (value[index] == '\0')
        return filter.keep_fences(begin, begin) == Status::ok ? Status::ok
                                                              : refuse("--fence wants a fence or a range");
    uint64_t end = 0u;
    if (!number(value + index + 1u, end)) return refuse("--fence wants a fence or a range");
    return filter.keep_fences(begin, end) == Status::ok ? Status::ok
                                                        : refuse("--fence ends before it starts");
}

Status RecordingToolRequest::take_gpu(const char *value)
{
    AV_SCOPE("RecordingToolRequest::take_gpu");
    for (size_t index = 0u; index < uuid_digits; index++) {
        const uint8_t digit = value[index] == '\0' ? 16u : digit_of(value[index]);
        if (digit == 16u) return refuse("--gpu wants 32 hexadecimal characters");
        gpu_[index / 2u] = static_cast<uint8_t>((index % 2u == 0u) ? digit << 4u
                                                                   : (gpu_[index / 2u] | digit));
    }
    if (value[uuid_digits] != '\0') return refuse("--gpu wants 32 hexadecimal characters");
    gpu_given_ = true;
    return Status::ok;
}

Status RecordingToolRequest::refuse(const char *why)
{
    AV_SCOPE("RecordingToolRequest::refuse");
    reason_ = why;
    action_ = Action::none;
    return Status::invalid_parameter;
}

RecordingToolRequest::Action RecordingToolRequest::action() const
{
    AV_SCOPE("RecordingToolRequest::action");
    return action_;
}

const char *RecordingToolRequest::recording() const
{
    AV_SCOPE("RecordingToolRequest::recording");
    return recording_;
}

const char *RecordingToolRequest::executor() const
{
    AV_SCOPE("RecordingToolRequest::executor");
    return executor_;
}

const uint8_t *RecordingToolRequest::gpu() const
{
    AV_SCOPE("RecordingToolRequest::gpu");
    return gpu_given_ ? gpu_ : nullptr;
}

uint32_t RecordingToolRequest::payload_bytes() const
{
    AV_SCOPE("RecordingToolRequest::payload_bytes");
    return payload_bytes_;
}

const char *RecordingToolRequest::reason() const
{
    AV_SCOPE("RecordingToolRequest::reason");
    return reason_;
}

bool RecordingToolRequest::invariants_hold() const
{
    AV_SCOPE("RecordingToolRequest::invariants_hold");
    if (action_ == Action::none) return reason_[0] != '\0' || recording_ == nullptr;
    return recording_ != nullptr && reason_[0] == '\0'
           && (action_ == Action::print || executor_ != nullptr);
}

}  // namespace av
