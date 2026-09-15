#include "harness/executor/extension_list_probe.hpp"

namespace av {

ExtensionListProbe::ExtensionListProbe(ExtensionList &target) : list(target) {}

void ExtensionListProbe::set_count(uint32_t count) { list.count_ = count; }

}  // namespace av
