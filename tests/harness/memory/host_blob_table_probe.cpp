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

}  // namespace av
