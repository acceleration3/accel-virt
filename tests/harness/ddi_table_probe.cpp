#include "ddi_table_probe.hpp"

#include <cstdint>

namespace av {

DdiTableProbe::DdiTableProbe(DdiTable &target) : table(target) {}

void DdiTableProbe::set_version(std::uint32_t version) { table.version_ = version; }

void DdiTableProbe::set_padding(std::uint32_t padding) { table.padding_ = padding; }

}  // namespace av
