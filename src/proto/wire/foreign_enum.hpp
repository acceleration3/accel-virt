/* An application's structure field of an enumeration type may hold any bit
 * pattern - the ICD's callers are foreign code, and the fuzzer's structures
 * are foreign bytes. Loading such a field as its own type is undefined
 * behaviour (and UBSan traps on it), so a field the ICD did not write is read
 * as the integer it is.
 *
 * It lives beside the wire rather than in one ICD because both sides read
 * such a field: a descriptor template's entries are an application's, and the
 * device validates them again (feature 047). */
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

}  // namespace av

#endif  // AV_FOREIGN_ENUM_HPP
