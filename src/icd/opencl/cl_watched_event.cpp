#include "icd/opencl/cl_watched_event.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_object.hpp"

namespace av::clicd {

ClWatchedEvent::ClWatchedEvent()
    : event_(nullptr), transport_(nullptr), notify_(nullptr), user_data_(nullptr), wire_(0u),
      status_wanted_(0), held_(false)
{
    AV_SCOPE("ClWatchedEvent::ClWatchedEvent");
}

bool ClWatchedEvent::take(ClObject *event, uint64_t wire, ClientTransport *transport,
                          cl_int status_wanted, Notify notify, void *user_data)
{
    AV_SCOPE("ClWatchedEvent::take");
    if (notify == nullptr || event == nullptr || wire == 0u || transport == nullptr) return false;
    event_ = event;
    transport_ = transport;
    notify_ = notify;
    user_data_ = user_data;
    wire_ = wire;
    status_wanted_ = status_wanted;
    held_ = true;
    return true;
}

void ClWatchedEvent::give_up()
{
    AV_SCOPE("ClWatchedEvent::give_up");
    event_ = nullptr;
    transport_ = nullptr;
    notify_ = nullptr;
    user_data_ = nullptr;
    wire_ = 0u;
    status_wanted_ = 0;
    held_ = false;
}

bool ClWatchedEvent::held() const
{
    AV_SCOPE("ClWatchedEvent::held");
    return held_;
}

ClObject *ClWatchedEvent::event() const
{
    AV_SCOPE("ClWatchedEvent::event");
    return event_;
}

uint64_t ClWatchedEvent::wire() const
{
    AV_SCOPE("ClWatchedEvent::wire");
    return wire_;
}

ClientTransport *ClWatchedEvent::transport() const
{
    AV_SCOPE("ClWatchedEvent::transport");
    return transport_;
}

bool ClWatchedEvent::reached_by(cl_int status) const
{
    AV_SCOPE("ClWatchedEvent::reached_by");
    /* OpenCL counts down: CL_QUEUED is 3 and CL_COMPLETE is 0, so "at least as
     * far as" is "no greater than". A negative status is a command that
     * failed, and every registration runs then. */
    return status < 0 || status <= status_wanted_;
}

void ClWatchedEvent::run(cl_int status) const
{
    AV_SCOPE("ClWatchedEvent::run");
    if (notify_ == nullptr) return;
    notify_(reinterpret_cast<cl_event>(event_), status, user_data_);
}

bool ClWatchedEvent::invariants_hold() const
{
    AV_SCOPE("ClWatchedEvent::invariants_hold");
    if (!held_) {
        return event_ == nullptr && transport_ == nullptr && notify_ == nullptr && wire_ == 0u;
    }
    return event_ != nullptr && transport_ != nullptr && notify_ != nullptr && wire_ != 0u;
}

}  // namespace av::clicd
