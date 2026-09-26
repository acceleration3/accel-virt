#include "device/memory/held_blob.hpp"

#include "core/trace/function_scope.hpp"
#include "device/memory/blob_dma_buf.hpp"

namespace av {

HeldBlob::HeldBlob() : base_(nullptr), length_(0u), pages_(), host_memory_(nullptr)
{
    AV_SCOPE("HeldBlob::HeldBlob");
}

HeldBlob::HeldBlob(void *base, uint64_t length, const BlobWindow &pages, void *host_memory)
    : base_(base), length_(length), pages_(pages), host_memory_(host_memory)
{
    AV_SCOPE("HeldBlob::HeldBlob");
}

void *HeldBlob::base() const
{
    AV_SCOPE("HeldBlob::base");
    return base_;
}

uint64_t HeldBlob::length() const
{
    AV_SCOPE("HeldBlob::length");
    return length_;
}

Status HeldBlob::share(BlobDmaBuf &shared, uint64_t offset, uint64_t size) const
{
    AV_SCOPE("HeldBlob::share");
    if (host_memory_ != nullptr) return shared.make_over_host(host_memory_, length_, offset, size);
    return shared.make(pages_, offset, size);
}

}  // namespace av
