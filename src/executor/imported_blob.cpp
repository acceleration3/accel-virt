#include "executor/imported_blob.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"

namespace av {

ImportedBlob::ImportedBlob()
    : id_(0u), kind_(0u), size_(0u), pointer_(nullptr), descriptor_(-1), memory_type_(0u), object_(0u),
      wire_handle_(0u), live_(false)
{
    AV_SCOPE("ImportedBlob::ImportedBlob");
}

Status ImportedBlob::take(uint64_t blob_id, const AvBlobHandle &handle)
{
    AV_SCOPE("ImportedBlob::take");
    if (live_) return Status::invalid_state;
    if (handle.size == 0u || handle.size % page_bytes != 0u) return Status::invalid_parameter;
    /* Exactly one kind. Two would mean the device could not decide which the
     * API is about to be handed, and none means it decided nothing. */
    if (handle.kind == AV_BLOB_HOST_POINTER) {
        if (handle.pointer == nullptr || handle.fd != -1) return Status::invalid_parameter;
    } else if (handle.kind == AV_BLOB_DMA_BUF) {
        /* With the device's own mapping of the memory, or none. */
        if (handle.fd < 0) return Status::invalid_parameter;
    } else {
        return Status::invalid_parameter;
    }

    id_ = blob_id;
    kind_ = handle.kind;
    size_ = handle.size;
    pointer_ = handle.pointer;
    descriptor_ = handle.fd;
    memory_type_ = handle.reserved;
    object_ = 0u;
    wire_handle_ = 0u;
    live_ = true;
    return Status::ok;
}

void ImportedBlob::give_back()
{
    AV_SCOPE("ImportedBlob::give_back");
    id_ = 0u;
    kind_ = 0u;
    size_ = 0u;
    pointer_ = nullptr;
    descriptor_ = -1;
    memory_type_ = 0u;
    object_ = 0u;
    wire_handle_ = 0u;
    live_ = false;
}

bool     ImportedBlob::holds(uint64_t blob_id) const
{
    AV_SCOPE("ImportedBlob::holds");
    return live_ && id_ == blob_id;
}
bool     ImportedBlob::live() const
{
    AV_SCOPE("ImportedBlob::live");
    return live_;
}
uint64_t ImportedBlob::id() const
{
    AV_SCOPE("ImportedBlob::id");
    return id_;
}
uint32_t ImportedBlob::kind() const
{
    AV_SCOPE("ImportedBlob::kind");
    return kind_;
}
uint64_t ImportedBlob::size() const
{
    AV_SCOPE("ImportedBlob::size");
    return size_;
}
void    *ImportedBlob::pointer() const
{
    AV_SCOPE("ImportedBlob::pointer");
    return pointer_;
}
int      ImportedBlob::descriptor() const
{
    AV_SCOPE("ImportedBlob::descriptor");
    return descriptor_;
}
uint32_t ImportedBlob::memory_type() const
{
    AV_SCOPE("ImportedBlob::memory_type");
    return memory_type_;
}

void ImportedBlob::record(uint64_t object, uint64_t wire_handle)
{
    AV_SCOPE("ImportedBlob::record");
    object_ = object;
    wire_handle_ = wire_handle;
}

uint64_t ImportedBlob::object() const
{
    AV_SCOPE("ImportedBlob::object");
    return object_;
}
uint64_t ImportedBlob::wire_handle() const
{
    AV_SCOPE("ImportedBlob::wire_handle");
    return wire_handle_;
}

bool ImportedBlob::invariants_hold() const
{
    AV_SCOPE("ImportedBlob::invariants_hold");
    if (!live_)
        return kind_ == 0u && size_ == 0u && pointer_ == nullptr && descriptor_ == -1 && memory_type_ == 0u &&
               object_ == 0u && wire_handle_ == 0u;
    if (size_ == 0u || size_ % page_bytes != 0u) return false;
    if (wire_handle_ != 0u && object_ == 0u) return false;
    if (kind_ == AV_BLOB_HOST_POINTER) return pointer_ != nullptr && descriptor_ == -1;
    if (kind_ == AV_BLOB_DMA_BUF) return pointer_ == nullptr && descriptor_ >= 0;
    return false;
}

}  // namespace av
