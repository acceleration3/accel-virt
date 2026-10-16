#include "icd/d3d/record_stamp.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

RecordStamp::RecordStamp() : tag_(0u), id_(0u)
{
    AV_SCOPE("RecordStamp::RecordStamp");
}

RecordStamp::RecordStamp(uint32_t tag, uint32_t id) : tag_(tag), id_(id)
{
    AV_SCOPE("RecordStamp::RecordStamp");
}

bool RecordStamp::holds(uint32_t tag) const
{
    AV_SCOPE("RecordStamp::holds");
    return tag_ == tag;
}

uint32_t RecordStamp::id() const
{
    AV_SCOPE("RecordStamp::id");
    return id_;
}

}  // namespace av::d3d
