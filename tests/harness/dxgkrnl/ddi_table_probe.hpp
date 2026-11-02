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

}

#endif
