#include "icd/common/shared_bytes.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/shared_object_record.hpp"

namespace av {

SharedBytes::SharedBytes(uint64_t asked) : asked_(asked)
{
    AV_SCOPE("SharedBytes::SharedBytes");
}

uint64_t SharedBytes::pages() const
{
    AV_SCOPE("SharedBytes::pages");
    constexpr uint64_t page = SharedObjectRecord::page_size;
    if (asked_ == 0u || asked_ > ~uint64_t{0} - (page - 1u)) return 0u;
    return (asked_ + page - 1u) & ~(page - 1u);
}

}  // namespace av
