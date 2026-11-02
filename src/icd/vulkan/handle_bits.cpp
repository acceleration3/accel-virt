#include "icd/vulkan/handle_bits.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

uint64_t HandleBits::value() const
{
    AV_SCOPE("HandleBits::value");
    return bits_;
}

}
