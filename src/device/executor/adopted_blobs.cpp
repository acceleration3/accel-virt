#include "device/executor/adopted_blobs.hpp"

#include "core/trace/function_scope.hpp"
#include "device/executor/imported_blob.hpp"

namespace av {

AdoptedBlobs::AdoptedBlobs() : blobs_{}, count_(0u)
{
    AV_SCOPE("AdoptedBlobs::AdoptedBlobs");
}

Status AdoptedBlobs::add(ImportedBlob &blob)
{
    AV_SCOPE("AdoptedBlobs::add");
    if (!blob.live() || blob.object() == 0u) return Status::invalid_parameter;
    if (find(blob.object()) != nullptr) return Status::conflict;
    if (count_ == capacity) return Status::no_space;
    blobs_[count_++] = &blob;
    return Status::ok;
}

ImportedBlob *AdoptedBlobs::find(uint64_t object)
{
    AV_SCOPE("AdoptedBlobs::find");
    if (object == 0u) return nullptr;
    for (uint32_t i = 0u; i < count_; i++) {
        if (blobs_[i]->object() == object) return blobs_[i];
    }
    return nullptr;
}

void AdoptedBlobs::remove(const ImportedBlob &blob)
{
    AV_SCOPE("AdoptedBlobs::remove");
    for (uint32_t i = 0u; i < count_; i++) {
        if (blobs_[i] != &blob) continue;
        /* Order means nothing here, so the last one fills the hole. */
        blobs_[i] = blobs_[--count_];
        blobs_[count_] = nullptr;
        return;
    }
}

void AdoptedBlobs::clear()
{
    AV_SCOPE("AdoptedBlobs::clear");
    for (uint32_t i = 0u; i < count_; i++) blobs_[i] = nullptr;
    count_ = 0u;
}

uint32_t AdoptedBlobs::count() const
{
    AV_SCOPE("AdoptedBlobs::count");
    return count_;
}

bool AdoptedBlobs::invariants_hold() const
{
    AV_SCOPE("AdoptedBlobs::invariants_hold");
    if (count_ > capacity) return false;
    for (uint32_t i = 0u; i < count_; i++) {
        const ImportedBlob *const blob = blobs_[i];
        if (blob == nullptr || !blob->live() || blob->object() == 0u) return false;
        for (uint32_t j = i + 1u; j < count_; j++) {
            if (blobs_[j] == blob) return false;
        }
    }
    return true;
}

}  // namespace av
