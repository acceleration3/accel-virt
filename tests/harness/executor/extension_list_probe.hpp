/* Reaches into an ExtensionList so invariant tests can corrupt it. Declared a
 * friend by ExtensionList; nothing outside the tests uses it. */
#ifndef AV_EXTENSION_LIST_PROBE_HPP
#define AV_EXTENSION_LIST_PROBE_HPP

#include "executor/vulkan/extension_list.hpp"

#include <stdint.h>

namespace av {

struct ExtensionListProbe {
    explicit ExtensionListProbe(ExtensionList &target);

    void set_name(uint32_t index, const char *name);

    ExtensionList &list;
};

}  // namespace av

#endif  // AV_EXTENSION_LIST_PROBE_HPP
