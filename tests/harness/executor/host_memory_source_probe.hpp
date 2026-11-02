#ifndef AV_HOST_MEMORY_SOURCE_PROBE_HPP
#define AV_HOST_MEMORY_SOURCE_PROBE_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "executor/host_memory_source.hpp"

#include <stdint.h>

#include <pthread.h>

namespace av {

class ExportedMemory;
class SourceMemory;

struct HostMemorySourceProbe {
    explicit HostMemorySourceProbe(HostMemorySource &target);

    [[nodiscard]] Status add(uint32_t domain, uint64_t key, const ExportedMemory &memory);
    void                 drain(SourceMemory &memory);
    void                 mark_visible(uint32_t types);
    ObjectPool<SourceMemory> &pool();
    pthread_mutex_t          &mutex();

    HostMemorySource &source;
};

}

#endif
