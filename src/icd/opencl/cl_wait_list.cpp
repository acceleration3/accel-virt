#include "icd/opencl/cl_wait_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClWaitList::ClWaitList() : count_(0u), events_(nullptr)
{
    AV_SCOPE("ClWaitList::ClWaitList");
}

ClWaitList::ClWaitList(cl_uint count, const cl_event *events) : count_(count), events_(events)
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

}  // namespace av::clicd
