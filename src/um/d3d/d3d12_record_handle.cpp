#include "um/d3d/d3d12_record_handle.hpp"

#include "core/trace/function_scope.hpp"
#include "um/d3d/d3d12_records.hpp"

namespace av::d3d12 {

D3d12RecordHandle::D3d12RecordHandle(void *block) : block_(block)
{
    AV_SCOPE("D3d12RecordHandle::D3d12RecordHandle");
}

D3d12Device &D3d12RecordHandle::device() const
{
    AV_SCOPE("D3d12RecordHandle::device");
    return *static_cast<const RecordHeader *>(block_)->device;
}

D3d12Recorder &D3d12RecordHandle::recorder() const
{
    AV_SCOPE("D3d12RecordHandle::recorder");
    return *static_cast<const RecordHeader *>(block_)->recorder;
}

}
