#include "executor/owned_fd.hpp"

#include "core/trace/function_scope.hpp"

#include <fcntl.h>
#include <unistd.h>

namespace av {

OwnedFd::OwnedFd() : fd_(-1)
{
    AV_SCOPE("OwnedFd::OwnedFd");
}

OwnedFd::OwnedFd(int fd) : fd_(fd >= 0 ? fd : -1)
{
    AV_SCOPE("OwnedFd::OwnedFd");
}

OwnedFd::~OwnedFd()
{
    AV_SCOPE("OwnedFd::~OwnedFd");
    close();
}

void OwnedFd::hold(int fd)
{
    AV_SCOPE("OwnedFd::hold");
    close();
    fd_ = fd >= 0 ? fd : -1;
}

bool OwnedFd::hold_copy(int fd)
{
    AV_SCOPE("OwnedFd::hold_copy");
    close();
    if (fd < 0) return false;
    fd_ = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    return fd_ >= 0;
}

bool OwnedFd::copy_to(OwnedFd &copy) const
{
    AV_SCOPE("OwnedFd::copy_to");
    return copy.hold_copy(fd_);
}

int OwnedFd::fd() const
{
    AV_SCOPE("OwnedFd::fd");
    return fd_;
}

bool OwnedFd::held() const
{
    AV_SCOPE("OwnedFd::held");
    return fd_ >= 0;
}

int OwnedFd::take()
{
    AV_SCOPE("OwnedFd::take");
    const int taken = fd_;
    fd_ = -1;
    return taken;
}

void OwnedFd::close()
{
    AV_SCOPE("OwnedFd::close");
    if (fd_ >= 0) static_cast<void>(::close(fd_));
    fd_ = -1;
}

}
