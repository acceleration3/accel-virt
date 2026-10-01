#include "device/shim/probe_dma_buf.hpp"

#include "core/trace/function_scope.hpp"

#include <fcntl.h>
#include <linux/udmabuf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace av {

ProbeDmaBuf::ProbeDmaBuf() : memory_(-1), shared_(-1)
{
    AV_SCOPE("ProbeDmaBuf::ProbeDmaBuf");
}

ProbeDmaBuf::~ProbeDmaBuf()
{
    AV_SCOPE("ProbeDmaBuf::~ProbeDmaBuf");
    release();
}

bool ProbeDmaBuf::make()
{
    AV_SCOPE("ProbeDmaBuf::make");
    if (shared_ >= 0) return true;
    const int device = ::open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
    if (device < 0) return false;
    memory_ = static_cast<int>(::syscall(SYS_memfd_create, "av-probe", MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (memory_ < 0 || ::ftruncate(memory_, page_size) != 0 ||
        ::fcntl(memory_, F_ADD_SEALS, F_SEAL_SHRINK) != 0) {
        ::close(device);
        release();
        return false;
    }
    udmabuf_create_item item = {};
    item.memfd = static_cast<__u32>(memory_);
    item.offset = 0u;
    item.size = page_size;
    udmabuf_create_list list = {};
    list.flags = UDMABUF_FLAGS_CLOEXEC;
    list.count = 1u;
    /* The list's items are a flexible array the struct only declares, so the
     * one item is written where the kernel reads it: immediately after. */
    unsigned char bytes[sizeof(udmabuf_create_list) + sizeof(udmabuf_create_item)] = {};
    __builtin_memcpy(bytes, &list, sizeof(list));
    __builtin_memcpy(bytes + sizeof(list), &item, sizeof(item));
    shared_ = ::ioctl(device, UDMABUF_CREATE_LIST, bytes);
    ::close(device);
    if (shared_ < 0) {
        release();
        return false;
    }
    return true;
}

void ProbeDmaBuf::release()
{
    AV_SCOPE("ProbeDmaBuf::release");
    if (shared_ >= 0) ::close(shared_);
    if (memory_ >= 0) ::close(memory_);
    shared_ = -1;
    memory_ = -1;
}

int ProbeDmaBuf::handle() const
{
    AV_SCOPE("ProbeDmaBuf::handle");
    return shared_;
}

/* A descriptor exists only over memory that does. */
bool ProbeDmaBuf::invariants_hold() const
{
    AV_SCOPE("ProbeDmaBuf::invariants_hold");
    return shared_ < 0 || memory_ >= 0;
}

}  // namespace av
