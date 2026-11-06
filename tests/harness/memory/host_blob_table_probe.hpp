#ifndef AV_HOST_BLOB_TABLE_PROBE_HPP
#define AV_HOST_BLOB_TABLE_PROBE_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "device/memory/host_blob_table.hpp"

#include <stdint.h>

namespace av {

struct HostBlobTableProbe {
    explicit HostBlobTableProbe(HostBlobTable &target);

    void place(uint32_t id, uint64_t size, uint64_t offset);
    void misplace(uint32_t index, uint64_t offset);
    void forget_placement(uint32_t index);
    void add_placement(uint64_t offset, uint64_t size);
    void resize_placement(uint32_t index, uint64_t size);
    void orphan_blob();
    void drop(uint32_t id);

    KeyedSlots<uint32_t, HostBlob *> &ids();
    ObjectPool<HostBlob>             &blobs();

    HostBlobTable &table;
};

}

#endif
