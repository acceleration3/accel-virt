#include "icd/opencl/cl_wait_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClWaitList::ClWaitList() : count_(0u), events_(nullptr), blocking_(CL_FALSE)
{
    AV_SCOPE("ClWaitList::ClWaitList");
}

ClWaitList::ClWaitList(cl_uint count, const cl_event *events) : count_(count), events_(events), blocking_(CL_FALSE)
{
    AV_SCOPE("ClWaitList::ClWaitList");
}

ClWaitList::ClWaitList(cl_uint count, const cl_event *events, cl_bool blocking)
    : count_(count), events_(events), blocking_(blocking)
{
    AV_SCOPE("ClWaitList::ClWaitList");
}

cl_uint ClWaitList::count() const
{
    AV_SCOPE("ClWaitList::count");
    return count_;
}

const cl_event *ClWaitList::events() const
{
    AV_SCOPE("ClWaitList::events");
    return events_;
}

bool ClWaitList::blocks() const
{
    AV_SCOPE("ClWaitList::blocks");
    return blocking_ != CL_FALSE;
}

}  // namespace av::clicd
