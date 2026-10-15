/* What every enqueue is handed after its own arguments: the events of the
 * application's that the command may not start before, as the application
 * spelled them - a count and its array. The root translates and judges them
 * (ClIcdRoot::wait_list); this only carries them there. */
#ifndef AV_CLICD_CL_WAIT_LIST_HPP
#define AV_CLICD_CL_WAIT_LIST_HPP

#include <CL/cl.h>

namespace av::clicd {

class ClWaitList {
public:
    /* None: a command that waits for nothing but its queue. */
    ClWaitList();
    ClWaitList(cl_uint count, const cl_event *events);

    cl_uint         count() const;
    const cl_event *events() const;

private:
    cl_uint         count_;
    const cl_event *events_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_WAIT_LIST_HPP
