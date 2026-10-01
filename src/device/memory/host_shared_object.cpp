#include "device/memory/host_shared_object.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"
#include "device/host/host.hpp"

namespace av {

HostSharedObject::HostSharedObject()
    : id_(0u), kind_(0u), handle_kind_(0u), size_(0u), descriptor_(-1), holders_(0u)
{
    AV_SCOPE("HostSharedObject::HostSharedObject");
}

Status HostSharedObject::take(uint64_t object_id, uint32_t kind, uint32_t handle_kind, uint64_t size,
                              int descriptor)
{
    AV_SCOPE("HostSharedObject::take");
    if (live()) return Status::invalid_state;
    if (object_id == 0u || descriptor < 0) return Status::invalid_parameter;
    if (kind < AV_SHARED_MEMORY || kind > AV_SHARED_NATIVE_FENCE) return Status::invalid_parameter;
    /* Memory is a length the importer allocates; a semaphore is not a length
     * at all, so one that claims to be is a guest describing something the
     * host cannot make. */
    if ((kind == AV_SHARED_MEMORY) != (size != 0u)) return Status::invalid_parameter;
    if (handle_kind != AV_SHARED_HANDLE_OPAQUE_FD && handle_kind != AV_SHARED_HANDLE_DMA_BUF) return Status::invalid_parameter;
    id_ = object_id;
    kind_ = kind;
    handle_kind_ = handle_kind;
    size_ = size;
    descriptor_ = descriptor;
    holders_ = 1u;
    return Status::ok;
}

void HostSharedObject::hold()
{
    AV_SCOPE("HostSharedObject::hold");
    if (live()) holders_++;
}

bool HostSharedObject::drop(Host &host)
{
    AV_SCOPE("HostSharedObject::drop");
    if (!live()) return false;
    holders_--;
    if (holders_ != 0u) return false;
    give_back(host);
    return true;
}

void HostSharedObject::give_back(Host &host)
{
    AV_SCOPE("HostSharedObject::give_back");
    if (descriptor_ >= 0) host.close_dmabuf(descriptor_);
    id_ = 0u;
    kind_ = 0u;
    handle_kind_ = 0u;
    size_ = 0u;
    descriptor_ = -1;
    holders_ = 0u;
}

bool HostSharedObject::live() const
{
    AV_SCOPE("HostSharedObject::live");
    return id_ != 0u;
}

bool HostSharedObject::holds(uint64_t object_id) const
{
    AV_SCOPE("HostSharedObject::holds");
    return live() && id_ == object_id;
}

uint64_t HostSharedObject::id() const
{
    AV_SCOPE("HostSharedObject::id");
    return id_;
}

uint32_t HostSharedObject::kind() const
{
    AV_SCOPE("HostSharedObject::kind");
    return kind_;
}

uint32_t HostSharedObject::handle_kind() const
{
    AV_SCOPE("HostSharedObject::handle_kind");
    return handle_kind_;
}

uint64_t HostSharedObject::size() const
{
    AV_SCOPE("HostSharedObject::size");
    return size_;
}

int HostSharedObject::descriptor() const
{
    AV_SCOPE("HostSharedObject::descriptor");
    return descriptor_;
}

uint32_t HostSharedObject::holders() const
{
    AV_SCOPE("HostSharedObject::holders");
    return holders_;
}

bool HostSharedObject::invariants_hold() const
{
    AV_SCOPE("HostSharedObject::invariants_hold");
    if (!live())
        return kind_ == 0u && handle_kind_ == 0u && size_ == 0u && descriptor_ < 0 && holders_ == 0u;
    if (kind_ < AV_SHARED_MEMORY || kind_ > AV_SHARED_NATIVE_FENCE) return false;
    if (handle_kind_ != AV_SHARED_HANDLE_OPAQUE_FD && handle_kind_ != AV_SHARED_HANDLE_DMA_BUF) return false;
    if ((kind_ == AV_SHARED_MEMORY) != (size_ != 0u)) return false;
    return descriptor_ >= 0 && holders_ != 0u;
}

}  // namespace av
