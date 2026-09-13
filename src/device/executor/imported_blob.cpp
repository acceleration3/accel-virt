#include "device/executor/imported_blob.hpp"

#include "device/abi/rvk_executor.h"

namespace av {

ImportedBlob::ImportedBlob()
    : id_(0u), kind_(0u), size_(0u), pointer_(nullptr), descriptor_(-1), object_(0u), live_(false)
{}

Status ImportedBlob::take(uint64_t blob_id, const RvkBlobHandle &handle)
{
    if (live_) return Status::invalid_state;
    if (handle.reserved != 0u) return Status::invalid_parameter;
    if (handle.size == 0u || handle.size % page_bytes != 0u) return Status::invalid_parameter;
    /* Exactly one kind. Two would mean the device could not decide which the
     * API is about to be handed, and none means it decided nothing. */
    if (handle.kind == RVK_BLOB_HOST_POINTER) {
        if (handle.pointer == nullptr || handle.fd != -1) return Status::invalid_parameter;
    } else if (handle.kind == RVK_BLOB_DMA_BUF) {
        if (handle.fd < 0 || handle.pointer != nullptr) return Status::invalid_parameter;
    } else {
        return Status::invalid_parameter;
    }

    id_ = blob_id;
    kind_ = handle.kind;
    size_ = handle.size;
    pointer_ = handle.pointer;
    descriptor_ = handle.fd;
    object_ = 0u;
    live_ = true;
    return Status::ok;
}

void ImportedBlob::give_back()
{
    id_ = 0u;
    kind_ = 0u;
    size_ = 0u;
    pointer_ = nullptr;
    descriptor_ = -1;
    object_ = 0u;
    live_ = false;
}

bool     ImportedBlob::holds(uint64_t blob_id) const { return live_ && id_ == blob_id; }
bool     ImportedBlob::live() const { return live_; }
uint64_t ImportedBlob::id() const { return id_; }
uint32_t ImportedBlob::kind() const { return kind_; }
uint64_t ImportedBlob::size() const { return size_; }
void    *ImportedBlob::pointer() const { return pointer_; }
int      ImportedBlob::descriptor() const { return descriptor_; }

void     ImportedBlob::set_object(uint64_t object) { object_ = object; }
uint64_t ImportedBlob::object() const { return object_; }

bool ImportedBlob::invariants_hold() const
{
    if (!live_) return kind_ == 0u && size_ == 0u && pointer_ == nullptr && descriptor_ == -1;
    if (size_ == 0u || size_ % page_bytes != 0u) return false;
    if (kind_ == RVK_BLOB_HOST_POINTER) return pointer_ != nullptr && descriptor_ == -1;
    if (kind_ == RVK_BLOB_DMA_BUF) return pointer_ == nullptr && descriptor_ >= 0;
    return false;
}

}  // namespace av
