#include "device/recording/replay_context.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/executor_library.hpp"
#include "device/recording/recorded_command.hpp"

namespace av {

namespace {

constexpr uint16_t vulkan_commands = 1u;
constexpr uint16_t opencl_commands = 2u;
constexpr uint16_t opengl_commands = 3u;
constexpr uint16_t d3d_commands = 4u;

}  // namespace

ReplayContext::ReplayContext()
    : executor_(nullptr),
      context_(0u),
      api_(SubApi::vulkan),
      answered_(false),
      answer_length_(0u),
      gave_up_(false),
      answer_{}
{
    AV_SCOPE("ReplayContext::ReplayContext");
}

/* The library's own destroy is the caller's: this object does not hold the
 * library it was made on. */
ReplayContext::~ReplayContext()
{
    AV_SCOPE("ReplayContext::~ReplayContext");
}

Status ReplayContext::create(const ExecutorLibrary &library, uint32_t context, SubApi api,
                             const uint8_t *gpu_uuid)
{
    AV_SCOPE("ReplayContext::create");
    if (executor_ != nullptr) return Status::invalid_state;
    AvExecutor *const made = library.create(api, gpu_uuid);
    if (made == nullptr) return Status::no_memory;
    executor_ = made;
    context_ = context;
    api_ = api;
    answered_ = false;
    answer_length_ = 0u;
    gave_up_ = false;
    return Status::ok;
}

void ReplayContext::destroy(const ExecutorLibrary &library)
{
    AV_SCOPE("ReplayContext::destroy");
    if (executor_ == nullptr) return;
    library.destroy(executor_);
    executor_ = nullptr;
}

bool ReplayContext::created() const
{
    AV_SCOPE("ReplayContext::created");
    return executor_ != nullptr;
}

uint32_t ReplayContext::context() const
{
    AV_SCOPE("ReplayContext::context");
    return context_;
}

uint16_t ReplayContext::command_api() const
{
    AV_SCOPE("ReplayContext::command_api");
    if (api_ == SubApi::opencl) return opencl_commands;
    if (api_ == SubApi::d3d) return d3d_commands;
    return api_ == SubApi::opengl ? opengl_commands : vulkan_commands;
}

Status ReplayContext::execute(const ExecutorLibrary &library, const RecordedCommand &command)
{
    AV_SCOPE("ReplayContext::execute");
    if (executor_ == nullptr) return Status::invalid_state;
    /* Only a command that asks for an answer is given somewhere to write one,
     * exactly as the device does it, so a later command cannot overwrite the
     * answer the batch's reply will be compared against. */
    const bool     replying = command.header().generates_reply();
    size_t         produced = 0u;
    const uint32_t answered =
        library.execute(executor_, command.header().id(), command.payload(), command.payload_length(),
                        replying ? answer_ : nullptr, replying ? sizeof answer_ : 0u, produced);
    if (answered == AV_UNKNOWN) return Status::not_found;
    if (answered == AV_UNDECODABLE) return Status::malformed;
    if (answered != AV_EXECUTED) return Status::invalid_state;
    if (!replying) return Status::ok;
    answered_ = true;
    answer_length_ = produced;
    return Status::ok;
}

bool ReplayContext::answered() const
{
    AV_SCOPE("ReplayContext::answered");
    return answered_;
}

size_t ReplayContext::answer_length() const
{
    AV_SCOPE("ReplayContext::answer_length");
    return answer_length_;
}

bool ReplayContext::answers_with(const uint8_t *recorded, uint32_t length) const
{
    AV_SCOPE("ReplayContext::answers_with");
    if (!answered_ || answer_length_ != length) return false;
    if (answer_length_ > sizeof answer_) return false;
    for (uint32_t index = 0u; index < length; index++)
        if (answer_[index] != recorded[index]) return false;
    return true;
}

void ReplayContext::forget_answer()
{
    AV_SCOPE("ReplayContext::forget_answer");
    answered_ = false;
    answer_length_ = 0u;
}

void ReplayContext::give_up()
{
    AV_SCOPE("ReplayContext::give_up");
    gave_up_ = true;
}

bool ReplayContext::gave_up() const
{
    AV_SCOPE("ReplayContext::gave_up");
    return gave_up_;
}

bool ReplayContext::invariants_hold() const
{
    AV_SCOPE("ReplayContext::invariants_hold");
    if (executor_ == nullptr) return !answered_ && answer_length_ == 0u;
    /* No context is named 0, and a context that answered nothing kept no
     * length to compare a recorded reply against. */
    if (context_ == 0u) return false;
    return answered_ || answer_length_ == 0u;
}

}  // namespace av
