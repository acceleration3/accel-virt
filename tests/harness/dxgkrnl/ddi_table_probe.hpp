/* Reaches into a DdiTable so tests can build tables core never would - an old
 * version for the registration rules, corrupted padding for the invariants.
 * Declared a friend by DdiTable; nothing outside the tests uses it. */
#ifndef AV_DDI_TABLE_PROBE_HPP
#define AV_DDI_TABLE_PROBE_HPP

#include "miniport/driver/ddi_table.hpp"

#include <cstdint>

namespace av {

struct DdiTableProbe {
    explicit DdiTableProbe(DdiTable &target);

    void set_version(std::uint32_t version);
    void set_padding(std::uint32_t padding);

    DdiTable &table;
};

}  // namespace av

#endif  // AV_DDI_TABLE_PROBE_HPP
