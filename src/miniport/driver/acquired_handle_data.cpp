#include "miniport/driver/acquired_handle_data.hpp"

#include "miniport/driver/dxgkrnl_interface.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

AcquiredHandleData::AcquiredHandleData(DxgkrnlInterface &dxgkrnl, uint32_t allocation_handle)
    : dxgkrnl_(dxgkrnl), allocation_handle_(allocation_handle), data_(nullptr), release_handle_(nullptr)
    {
        AV_SCOPE("AcquiredHandleData::AcquiredHandleData");
    }

AcquiredHandleData::~AcquiredHandleData()
{
    AV_SCOPE("AcquiredHandleData::~AcquiredHandleData");
    if (release_handle_ != nullptr) dxgkrnl_.release_handle_data(release_handle_);
}

Status AcquiredHandleData::acquire()
{
    AV_SCOPE("AcquiredHandleData::acquire");
    if (data_ != nullptr) return Status::invalid_state;
    return dxgkrnl_.acquire_handle_data(allocation_handle_, data_, release_handle_);
}

void *AcquiredHandleData::data() const
{
    AV_SCOPE("AcquiredHandleData::data");
    return data_;
}

bool AcquiredHandleData::invariants_hold() const
{
    AV_SCOPE("AcquiredHandleData::invariants_hold");
    return data_ != nullptr || release_handle_ == nullptr;
}

}
