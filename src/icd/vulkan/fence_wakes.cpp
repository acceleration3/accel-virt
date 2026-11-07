#include "icd/vulkan/fence_wakes.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/connection.hpp"

namespace av::icd {

FenceWakes::FenceWakes(ServiceThread &thread) : thread_(thread)
{
    AV_SCOPE("FenceWakes::FenceWakes");
}

FenceWakes::~FenceWakes()
{
    AV_SCOPE("FenceWakes::~FenceWakes");
    thread_.stop();
}

bool FenceWakes::turn()
{
    AV_SCOPE("FenceWakes::turn");
    thread_.pause(true);
    return true;
}

bool FenceWakes::register_with(Connection &connection)
{
    AV_SCOPE("FenceWakes::register_with");
    if (thread_.wake_handle() == 0u) return false;
    const Status started = thread_.start(*this);
    if (started != Status::ok && started != Status::invalid_state) return false;
    return connection.wake_on(thread_.wake_handle());
}

uint64_t FenceWakes::woken() const
{
    AV_SCOPE("FenceWakes::woken");
    return thread_.woken();
}

void FenceWakes::await(uint64_t seen, uint64_t timeout)
{
    AV_SCOPE("FenceWakes::await");
    thread_.await_woken(seen, timeout);
}

}
