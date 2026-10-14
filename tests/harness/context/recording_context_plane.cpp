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

int RecordingContextPlane::fresh_memfd(std::uint64_t size) const
{
    const int fd = ::memfd_create("recorded-surface", MFD_CLOEXEC);
    if (fd >= 0 && ::ftruncate(fd, static_cast<off_t>(size)) != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

Status RecordingContextPlane::allocate_memory(std::uint32_t context, std::uint64_t key, std::uint64_t size,
                                              std::uint32_t request, AvHostMemory &memory)
{
    last_memory_ = MemoryAsked{context, key, size, request};
    memory_allocations_++;
    if (refuse_memory_ != 0u && --refuse_memory_ == 0u) return Status::no_memory;
    if (library_ != nullptr) return library_->allocate_memory(key, size, request, memory);
    memory.fd = without_descriptor_ ? -1 : fresh_memfd(size);
    memory.caching = caching_;
    memory.size = short_ ? size - 4096u : size;
    return Status::ok;
}

void RecordingContextPlane::free_memory(std::uint64_t key)
{
    if (freed_count_ < capacity) freed_[freed_count_++] = key;
    if (library_ != nullptr) library_->free_memory(key);
}

int RecordingContextPlane::fresh_eventfd() const
{
    return ::eventfd(1u, EFD_CLOEXEC);
}

}  // namespace av
