#include "icd/opengl/pixel_format_criteria.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

PixelFormatCriteria::PixelFormatCriteria() : names_{}, values_{}, count_(0u)
{
    AV_SCOPE("PixelFormatCriteria::PixelFormatCriteria");
}

bool PixelFormatCriteria::add(uint32_t name, double value)
{
    AV_SCOPE("PixelFormatCriteria::add");
    if (count_ == capacity) return false;
    names_[count_] = name;
    values_[count_] = value;
    count_++;
    return true;
}

uint32_t PixelFormatCriteria::count() const
{
    AV_SCOPE("PixelFormatCriteria::count");
    return count_;
}
uint32_t PixelFormatCriteria::name(uint32_t index) const
{
    AV_SCOPE("PixelFormatCriteria::name");
    return index < count_ ? names_[index] : 0u;
}
double   PixelFormatCriteria::value(uint32_t index) const
{
    AV_SCOPE("PixelFormatCriteria::value");
    return index < count_ ? values_[index] : 0.0;
}

/* A held criterion always names an attribute: zero ends a list. */
bool PixelFormatCriteria::invariants_hold() const
{
    AV_SCOPE("PixelFormatCriteria::invariants_hold");
    if (count_ > capacity) return false;
    for (uint32_t index = 0u; index < count_; index++)
        if (names_[index] == 0u) return false;
    return true;
}

}  // namespace av::glicd
