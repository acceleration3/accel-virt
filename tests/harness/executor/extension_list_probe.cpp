#include "harness/executor/extension_list_probe.hpp"

namespace av {

ExtensionListProbe::ExtensionListProbe(ExtensionList &target) : list(target) {}

void ExtensionListProbe::set_name(uint32_t index, const char *name) { list.names_.at(index) = name; }

}
