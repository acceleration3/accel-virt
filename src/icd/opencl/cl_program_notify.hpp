#ifndef AV_CLICD_CL_PROGRAM_NOTIFY_HPP
#define AV_CLICD_CL_PROGRAM_NOTIFY_HPP

#include <CL/cl.h>

namespace av::clicd {

class ClProgramNotify {
public:
    using Callback = void(CL_CALLBACK *)(cl_program program, void *user_data);

    ClProgramNotify();
    ClProgramNotify(Callback callback, void *user_data);

    bool valid() const;
    void tell(cl_program program) const;

private:
    Callback callback_;
    void    *user_data_;
};

}

#endif
