#ifndef AV_FOREIGN_ENUM_HPP
#define AV_FOREIGN_ENUM_HPP

#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av {

template <typename Enumeration>
int32_t foreign_enum(const Enumeration &field)
{
    AV_SCOPE("foreign_enum");
    int32_t value = 0;
    __builtin_memcpy(&value, &field, sizeof value);
    return value;
}

}

#endif
