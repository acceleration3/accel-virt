/* What clBuildProgram, clCompileProgram and clLinkProgram take as pfn_notify:
 * the application's callback for when the work is done, and its user data.
 *
 * The work is done when the call returns - the host answers once it has built
 * - so the callback runs then, on the calling thread, before the call returns,
 * which the API allows: with a callback the call "does not need to wait", and
 * nothing says it may not. An application that waits on a condition its
 * callback sets finds it set. No guest function pointer crosses the wire. */
#ifndef AV_CLICD_CL_PROGRAM_NOTIFY_HPP
#define AV_CLICD_CL_PROGRAM_NOTIFY_HPP

#include <CL/cl.h>

namespace av::clicd {

class ClProgramNotify {
public:
    using Callback = void(CL_CALLBACK *)(cl_program program, void *user_data);

    /* None: the call returns when it is done, and that is all. */
    ClProgramNotify();
    ClProgramNotify(Callback callback, void *user_data);

    /* The API's own rule: user data without a callback is CL_INVALID_VALUE. */
    bool valid() const;
    /* `program` is done with; nothing without a callback or a program. */
    void tell(cl_program program) const;

private:
    Callback callback_;
    void    *user_data_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_PROGRAM_NOTIFY_HPP
