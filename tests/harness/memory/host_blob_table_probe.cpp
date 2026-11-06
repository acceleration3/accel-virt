#include "harness/memory/host_blob_table_probe.hpp"

#include "device/abi/av_executor.h"

#include <sys/mman.h>
#include <unistd.h>

namespace av {

HostBlobTableProbe::HostBlobTableProbe(HostBlobTable &target) : table(target) {}

void HostBlobTableProbe::place(uint32_t id, uint64_t size, uint64_t offset)
{
    const int fd = ::memfd_create("probed host blob", MFD_CLOEXEC);
    if (fd < 0 || ::ftruncate(fd, static_cast<off_t>(size)) != 0) return;
    const AvHostMemory memory = {fd, AV_MEMORY_WRITE_BACK, size};
    HostBlob          *blob = nullptr;
    if (table.blobs_.create(blob, table.host_, 1u, memory) != Status::ok) return;
    uint32_t slot = 0u;
    static_cast<void>(table.ids_.insert(id, blob, slot));
    static_cast<void>(blob->map(offset));
    static_cast<void>(table.placed_.insert_at(table.placed_from(offset), HostBlobTable::Placed{offset, size}));
}

void HostBlobTableProbe::misplace(uint32_t index, uint64_t offset)
{
    table.placed_.at(index).offset_ = offset;
}

void HostBlobTableProbe::forget_placement(uint32_t index)
{
    table.placed_.remove_at(index);
}

void HostBlobTableProbe::add_placement(uint64_t offset, uint64_t size)
{
    static_cast<void>(table.placed_.insert_at(table.placed_from(offset), HostBlobTable::Placed{offset, size}));
}

void HostBlobTableProbe::resize_placement(uint32_t index, uint64_t size)
{
    table.placed_.at(index).size_ = size;
}

void HostBlobTableProbe::orphan_blob()
{
    const int fd = ::memfd_create("orphaned host blob", MFD_CLOEXEC);
    if (fd < 0 || ::ftruncate(fd, 4096) != 0) return;
    const AvHostMemory memory = {fd, AV_MEMORY_WRITE_BACK, 4096u};
    HostBlob          *blob = nullptr;
    static_cast<void>(table.blobs_.create(blob, table.host_, 1u, memory));
}

void HostBlobTableProbe::drop(uint32_t id)
{
    table.drop(id);
}

KeyedSlots<uint32_t, HostBlob *> &HostBlobTableProbe::ids()
{
    return table.ids_;
}

ObjectPool<HostBlob> &HostBlobTableProbe::blobs()
{
    return table.blobs_;
}

}
