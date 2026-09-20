/* Reaches into an ImportableTypes so invariant tests can corrupt it. Declared a
 * friend by ImportableTypes; nothing outside the tests uses it. */
#ifndef AV_IMPORTABLE_TYPES_PROBE_HPP
#define AV_IMPORTABLE_TYPES_PROBE_HPP

#include "device/executor/importable_types.hpp"

#include <stdint.h>

namespace av {

struct ImportableTypesProbe {
    explicit ImportableTypesProbe(ImportableTypes &target);

    void set_mask(uint32_t mask);
    /* A kind no ABI names, which learn never stores. */
    void set_kind(uint32_t kind);

    ImportableTypes &types;
};

}  // namespace av

#endif  // AV_IMPORTABLE_TYPES_PROBE_HPP
