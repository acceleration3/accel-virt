/* What clCreateContext's pfn_notify is: the application's callback for errors
 * of its context at run time, and the user data it is handed with them. A
 * context whose device the host lost says so here once, as a driver reports a
 * device reset - every call on it answers CL_OUT_OF_RESOURCES from then on,
 * and the callback is what tells an application that does not look. */
#ifndef AV_CLICD_CL_CONTEXT_NOTIFY_HPP
#define AV_CLICD_CL_CONTEXT_NOTIFY_HPP

#include <CL/cl.h>

#include <stddef.h>

namespace av::clicd {

class ClContextNotify {
public:
    using Callback = void(CL_CALLBACK *)(const char *errinfo, const void *private_info, size_t cb, void *user_data);

    /* None: an application that registered nothing is told nothing. */
    ClContextNotify();
    ClContextNotify(Callback callback, void *user_data);

    /* `text` handed to the callback, with no private information; nothing
     * without one. */
    void say(const char *text) const;

private:
    Callback callback_;
    void    *user_data_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_CONTEXT_NOTIFY_HPP
