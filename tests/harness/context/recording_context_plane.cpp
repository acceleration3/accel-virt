#include "harness/context/recording_context_plane.hpp"

#include <sys/eventfd.h>
#include <sys/mman.h>
#include <unistd.h>

namespace av {

/* A page, so a test that reads the descriptor's size reads something. */
RecordingContextPlane::~RecordingContextPlane()
{
    if (composed_fd_ >= 0) ::close(composed_fd_);
}

int RecordingContextPlane::fresh_memfd() const
{
    const int fd = ::memfd_create("recorded-surface", MFD_CLOEXEC);
    if (fd >= 0 && ::ftruncate(fd, 4096) != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

int RecordingContextPlane::fresh_eventfd() const
{
    return ::eventfd(1u, EFD_CLOEXEC);
}

}  // namespace av
