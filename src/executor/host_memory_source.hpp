/* One host GPU's single source of the memory a guest CPU and a host GPU share
 * (docs/host-memory.md). Nothing is allocated until a guest asks: each request
 * - a Vulkan allocation, a Direct3D 12 heap, an API's arena - is one
 * allocation of its own on the GPU's compositing context, exported as a
 * dma-buf the device maps into the host window, and imported by whichever
 * host API the asking context runs. One memory type is chosen per request, by
 * what the guest will do with it, and only types the exporter proved it can
 * export and map are ever chosen.
 *
 * Main loop allocates and frees; a context's worker acquires and releases
 * what it imports. One lock holds them apart. */
#ifndef AV_HOST_MEMORY_SOURCE_HPP
#define AV_HOST_MEMORY_SOURCE_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "executor/source_memory.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <pthread.h>
#include <stdint.h>

struct AvHostMemory;

namespace av {

class BlockSource;
class MemoryExporter;
class StateWriter;
struct HostMemorySourceProbe;

class HostMemorySource {
public:
    HostMemorySource(MemoryExporter &exporter, BlockSource &blocks);
    /* Every allocation freed, imported or not: the GPU is going. */
    ~HostMemorySource();
    HostMemorySource(const HostMemorySource &) = delete;
    HostMemorySource &operator=(const HostMemorySource &) = delete;

    /* Main loop. `size` bytes, whole pages, of the memory `request` names,
     * kept under `key` in `domain` until free: its dma-buf, the caller's, in
     * `memory`. conflict for a key already live; invalid_parameter for a
     * request no memory type of this GPU answers, or a size of no pages;
     * no_memory when the GPU has none to give. */
    [[nodiscard]] Status allocate(uint32_t domain, uint64_t key, uint64_t size, HostMemoryRequest request,
                                  AvHostMemory &memory);
    /* Main loop. The device's hold on `key` let go; an import still holding
     * the memory keeps it. Nothing for a key not live. */
    void                 free(uint32_t domain, uint64_t key);
    /* Main loop. Every key of `domain` let go: the device is going. */
    void                 free_domain(uint32_t domain);
    /* Any thread. The live memory under `key` in `domain`, held for an import
     * until release; null for none. */
    SourceMemory        *acquire(uint32_t domain, uint64_t key);
    void                 release(SourceMemory &memory);
    /* The memory types a Vulkan guest may map: host-visible, coherent and
     * exportable. */
    uint32_t             mappable_types();
    uint32_t             live() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    /* VkMemoryPropertyFlagBits, named here so this compiles with no loader. */
    static constexpr uint32_t device_local = 1u;
    static constexpr uint32_t host_visible = 2u;
    static constexpr uint32_t host_coherent = 4u;
    static constexpr uint32_t host_cached = 8u;
    static constexpr uint64_t page_bytes = 4096u;

    /* How many preferences a class has, at most: three, among the types an
     * import may map and then among the rest. */
    static constexpr uint32_t most_choices = 6u;

    /* The memory types `request` may be answered with, best first, in
     * `types`: how many. A class falls back to the next when the GPU has no
     * room in the first - a VRAM BAR without resizable BAR is 256 MiB. */
    uint32_t      choices(HostMemoryRequest request, uint32_t (&types)[most_choices]);
    /* The first type of `among` whose flags hold all of `wanted` and none of
     * `refused`, or memory_types. */
    uint32_t      first_type(uint32_t among, uint32_t wanted, uint32_t refused);
    SourceMemory *find(uint32_t domain, uint64_t key);
    /* One reference let go, under the lock; the memory freed with the last. */
    void          let_go(SourceMemory &memory);

    MemoryExporter              &exporter_;
    ObjectPool<SourceMemory>     memories_;
    mutable pthread_mutex_t      mutex_;
    /* Which types were asked of the exporter, which passed, and which of
     * those an importer may also map. */
    uint32_t                     asked_;
    uint32_t                     mappable_;
    uint32_t                     visible_;

    friend struct HostMemorySourceProbe;
};

}  // namespace av

#endif  // AV_HOST_MEMORY_SOURCE_HPP
