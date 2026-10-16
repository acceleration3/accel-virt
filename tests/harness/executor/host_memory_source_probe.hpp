/* Reaches into a HostMemorySource so invariant tests can corrupt it: a second
 * live allocation answering a key one already answers, and an allocation
 * still pooled with no reference left. Declared a friend by HostMemorySource;
 * nothing outside the tests uses it. */
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

    /* `memory` pooled under `key` in `domain` beside whatever answers it
     * already; the source frees it with the rest. */
    [[nodiscard]] Status add(uint32_t domain, uint64_t key, const ExportedMemory &memory);
    /* `memory`'s references let go of while it stays pooled. */
    void                 drain(SourceMemory &memory);
    /* `types` taken as importable host-visible, mapped or not. */
    void                 mark_visible(uint32_t types);
    /* The pool the allocations live in, for a test that corrupts it. */
    ObjectPool<SourceMemory> &pool();
    /* The mutex allocate and release hold, for a test that holds it as a
     * worker would. */
    pthread_mutex_t          &mutex();

    HostMemorySource &source;
};

}  // namespace av

#endif  // AV_HOST_MEMORY_SOURCE_PROBE_HPP
