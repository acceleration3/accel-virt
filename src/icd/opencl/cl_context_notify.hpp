#ifndef AV_CLICD_CL_CONTEXT_NOTIFY_HPP
#define AV_CLICD_CL_CONTEXT_NOTIFY_HPP

#include <CL/cl.h>

#include <stddef.h>

namespace av::clicd {

class ClContextNotify {
public:
    using Callback = void(CL_CALLBACK *)(const char *errinfo, const void *private_info, size_t cb, void *user_data);

    ClContextNotify();
    ClContextNotify(Callback callback, void *user_data);

    void say(const char *text) const;
    bool valid() const;

private:
    Callback callback_;
    void    *user_data_;
};

}

#endif
