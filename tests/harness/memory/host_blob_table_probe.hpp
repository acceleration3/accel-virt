/* Reaches into a HostBlobTable so invariant tests can corrupt it: a blob kept
 * behind the table's back, placed wherever the test says - over another blob,
 * or in a table with no lock - and a placement moved. Declared a friend by
 * HostBlobTable; nothing outside the tests uses it. */
#ifndef AV_HOST_BLOB_TABLE_PROBE_HPP
#define AV_HOST_BLOB_TABLE_PROBE_HPP

#include "device/memory/host_blob_table.hpp"

#include <stdint.h>

namespace av {

struct HostBlobTableProbe {
    explicit HostBlobTableProbe(HostBlobTable &target);

    /* A blob of `size` bytes of fresh memory for context 1 under `id`,
     * placed at `offset` of the hostmem region and noted there, wherever
     * that is. */
    void place(uint32_t id, uint64_t size, uint64_t offset);
    /* The `index`th placement noted at `offset` instead of where it is. */
    void misplace(uint32_t index, uint64_t offset);

    HostBlobTable &table;
};

}  // namespace av

#endif  // AV_HOST_BLOB_TABLE_PROBE_HPP
