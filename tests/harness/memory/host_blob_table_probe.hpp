#ifndef AV_HOST_BLOB_TABLE_PROBE_HPP
#define AV_HOST_BLOB_TABLE_PROBE_HPP

#include "device/memory/host_blob_table.hpp"

#include <stdint.h>

namespace av {

struct HostBlobTableProbe {
    explicit HostBlobTableProbe(HostBlobTable &target);

    void place(uint32_t id, uint64_t size, uint64_t offset);
    void misplace(uint32_t index, uint64_t offset);

    HostBlobTable &table;
};

}

#endif
