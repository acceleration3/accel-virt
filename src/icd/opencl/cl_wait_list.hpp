#ifndef AV_CLICD_CL_WAIT_LIST_HPP
#define AV_CLICD_CL_WAIT_LIST_HPP

#include <CL/cl.h>

namespace av::clicd {

class ClWaitList {
public:
    ClWaitList();
    ClWaitList(cl_uint count, const cl_event *events);
    ClWaitList(cl_uint count, const cl_event *events, cl_bool blocking);

    cl_uint         count() const;
    const cl_event *events() const;
    bool            blocks() const;

private:
    cl_uint         count_;
    const cl_event *events_;
    cl_bool         blocking_;
};

}

#endif
