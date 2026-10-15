/* What every enqueue is handed after its own arguments: the events of the
 * application's that the command may not start before, as the application
 * spelled them - a count and its array - and whether the call itself was told
 * to block, which is the API's own flag for a transfer or a map. The root
 * translates and judges them (ClIcdRoot::wait_list); this only carries them
 * there. */
#ifndef AV_CLICD_CL_WAIT_LIST_HPP
#define AV_CLICD_CL_WAIT_LIST_HPP

#include <CL/cl.h>

namespace av::clicd {

class ClWaitList {
public:
    /* None: a command that waits for nothing but its queue. */
    ClWaitList();
    /* Of a call that may return before its work is done - a command the
     * host runs on its own, an unmap - which is every call without a flag. */
    ClWaitList(cl_uint count, const cl_event *events);
    /* Of a transfer or a map, with the application's own `blocking`. */
    ClWaitList(cl_uint count, const cl_event *events, cl_bool blocking);

    cl_uint         count() const;
    const cl_event *events() const;
    /* Whether the call was told to block until its work is done. */
    bool            blocks() const;

private:
    cl_uint         count_;
    const cl_event *events_;
    cl_bool         blocking_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_WAIT_LIST_HPP
