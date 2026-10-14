#include "executor/exported_memory.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ExportedMemory::ExportedMemory() : handle_(0u), fd_(-1), pointer_(nullptr), size_(0u)
{
    AV_SCOPE("ExportedMemory::ExportedMemory");
}

ExportedMemory::ExportedMemory(uint64_t handle, int fd, uint8_t *pointer, uint64_t size)
    : handle_(handle), fd_(fd), pointer_(pointer), size_(size)
{
    AV_SCOPE("ExportedMemory::ExportedMemory");
}

bool ExportedMemory::made() const
{
    AV_SCOPE("ExportedMemory::made");
    return handle_ != 0u;
}

uint64_t ExportedMemory::handle() const
{
    AV_SCOPE("ExportedMemory::handle");
    return handle_;
}

int ExportedMemory::fd() const
{
    AV_SCOPE("ExportedMemory::fd");
    return fd_;
}

uint8_t *ExportedMemory::pointer() const
{
    AV_SCOPE("ExportedMemory::pointer");
    return pointer_;
}

uint64_t ExportedMemory::size() const
{
    AV_SCOPE("ExportedMemory::size");
    return size_;
}

}  // namespace av
