#include "executor/sharing/share_holder.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ShareHolder::ShareHolder(SessionBackend &session, uint64_t into, uint64_t handle, uint64_t detail)
    : session_(&session), next_(nullptr), into_(into), handle_(handle), detail_(detail)
{
    AV_SCOPE("ShareHolder::ShareHolder");
}

SessionBackend &ShareHolder::session() const
{
    AV_SCOPE("ShareHolder::session");
    return *session_;
}

uint64_t ShareHolder::into() const
{
    AV_SCOPE("ShareHolder::into");
    return into_;
}

uint64_t ShareHolder::handle() const
{
    AV_SCOPE("ShareHolder::handle");
    return handle_;
}

uint64_t ShareHolder::detail() const
{
    AV_SCOPE("ShareHolder::detail");
    return detail_;
}

ShareHolder *ShareHolder::next() const
{
    AV_SCOPE("ShareHolder::next");
    return next_;
}

void ShareHolder::link(ShareHolder *next)
{
    AV_SCOPE("ShareHolder::link");
    next_ = next;
}

}
