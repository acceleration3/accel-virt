#ifndef AV_CLICD_CL_NOTED_ERROR_HPP
#define AV_CLICD_CL_NOTED_ERROR_HPP

#include <CL/cl.h>

namespace av::clicd {

class ClIcdRoot;

class ClNotedError {
public:
    ClNotedError(ClIcdRoot &root, const char *entry_point, cl_int *application);
    ~ClNotedError();
    ClNotedError(const ClNotedError &) = delete;
    ClNotedError &operator=(const ClNotedError &) = delete;
    ClNotedError(ClNotedError &&) = delete;
    ClNotedError &operator=(ClNotedError &&) = delete;

    cl_int *pointer();

private:
    ClIcdRoot  &root_;
    const char *entry_point_;
    cl_int     *application_;
    cl_int      error_;
};

}

#endif
