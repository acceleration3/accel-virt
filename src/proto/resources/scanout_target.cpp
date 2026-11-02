#include "proto/resources/scanout_target.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ScanoutTarget::ScanoutTarget() : source_(0u), refresh_numerator_(0u), refresh_denominator_(0u)
{
    AV_SCOPE("ScanoutTarget::ScanoutTarget");
}

ScanoutTarget::ScanoutTarget(uint32_t source, uint32_t refresh_numerator, uint32_t refresh_denominator)
    : source_(source), refresh_numerator_(refresh_numerator), refresh_denominator_(refresh_denominator)
    {
        AV_SCOPE("ScanoutTarget::ScanoutTarget");
    }

uint32_t ScanoutTarget::source() const
{
    AV_SCOPE("ScanoutTarget::source");
    return source_;
}
uint32_t ScanoutTarget::refresh_numerator() const
{
    AV_SCOPE("ScanoutTarget::refresh_numerator");
    return refresh_numerator_;
}
uint32_t ScanoutTarget::refresh_denominator() const
{
    AV_SCOPE("ScanoutTarget::refresh_denominator");
    return refresh_denominator_;
}

bool ScanoutTarget::is_empty() const
{
    AV_SCOPE("ScanoutTarget::is_empty");
    return source_ == 0u && refresh_numerator_ == 0u && refresh_denominator_ == 0u;
}

}
