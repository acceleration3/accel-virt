#include "proto/resources/scanout_target.hpp"

namespace av {

ScanoutTarget::ScanoutTarget() : source_(0u), refresh_numerator_(0u), refresh_denominator_(0u) {}

ScanoutTarget::ScanoutTarget(uint32_t source, uint32_t refresh_numerator, uint32_t refresh_denominator)
    : source_(source), refresh_numerator_(refresh_numerator), refresh_denominator_(refresh_denominator) {}

uint32_t ScanoutTarget::source() const { return source_; }
uint32_t ScanoutTarget::refresh_numerator() const { return refresh_numerator_; }
uint32_t ScanoutTarget::refresh_denominator() const { return refresh_denominator_; }

bool ScanoutTarget::is_empty() const
{
    return source_ == 0u && refresh_numerator_ == 0u && refresh_denominator_ == 0u;
}

}  // namespace av
