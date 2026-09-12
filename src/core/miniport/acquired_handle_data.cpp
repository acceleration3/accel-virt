#include "core/miniport/acquired_handle_data.hpp"

#include "core/miniport/dxgkrnl_interface.hpp"

namespace av {

AcquiredHandleData::AcquiredHandleData(DxgkrnlInterface &dxgkrnl, uint32_t allocation_handle)
    : dxgkrnl_(dxgkrnl), allocation_handle_(allocation_handle), data_(nullptr), release_handle_(nullptr) {}

AcquiredHandleData::~AcquiredHandleData()
{
    if (release_handle_ != nullptr) dxgkrnl_.release_handle_data(release_handle_);
}

Status AcquiredHandleData::acquire()
{
    if (data_ != nullptr) return Status::invalid_state;
    return dxgkrnl_.acquire_handle_data(allocation_handle_, data_, release_handle_);
}

void *AcquiredHandleData::data() const { return data_; }

/* A release handle without data would be one nothing can read and the
 * destructor would still hand back. */
bool AcquiredHandleData::invariants_hold() const { return data_ != nullptr || release_handle_ == nullptr; }

}  // namespace av
