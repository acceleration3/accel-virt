#include "harness/memory/host_blob_table_probe.hpp"

#include "device/abi/av_executor.h"
#include "proto/resources/host_blob_id.hpp"

#include <sys/mman.h>
#include <unistd.h>

namespace av {

HostBlobTableProbe::HostBlobTableProbe(HostBlobTable &target) : table(target) {}

void HostBlobTableProbe::place(uint32_t id, uint64_t size, uint64_t offset)
{
    const int fd = ::memfd_create("probed host blob", MFD_CLOEXEC);
    if (fd < 0 || ::ftruncate(fd, static_cast<off_t>(size)) != 0) return;
    const AvHostMemory       memory = {fd, AV_MEMORY_WRITE_BACK, size};
    ObjectStorage<HostBlob> &slot = table.slots_[HostBlobId(id).slot()];
    static_cast<void>(slot.construct(table.host_, 1u, memory));
    static_cast<void>(slot.object().map(offset));
}

}  // namespace av
