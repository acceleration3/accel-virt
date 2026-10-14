/* One host GPU's single source of the memory a guest CPU and a host GPU share
 * (docs/host-memory.md). Nothing is allocated until a guest asks: each request
 * - a Vulkan allocation, a Direct3D 12 heap, an API's arena - is one
 * allocation of its own on the GPU's compositing context, exported as a
 * dma-buf the device maps into the host window, and imported by whichever
 * host API the asking context runs. One memory type is chosen per request, by
 * what the guest will do with it, and only types the exporter proved it can
 * export and map are ever chosen. A Vulkan request names a type of the
 * guest's view of the GPU (GuestMemoryTypes), which is this source's: one
 * view per GPU, so the index a guest was shown is the one translated here.
 *
 * Main loop allocates and frees; a context's worker acquires and releases
 * what it imports. One lock holds them apart. */
#ifndef AV_HOST_MEMORY_SOURCE_HPP
#define AV_HOST_MEMORY_SOURCE_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "executor/source_memory.hpp"
#include "executor/vulkan/guest_memory_types.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <pthread.h>
#include <stdint.h>

#include <vulkan/vulkan_core.h>

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
    /* Any thread. The host memory types the source allocates: host-visible,
     * coherent and exportable. */
    uint32_t             mappable_types();
    /* Any thread. The memory types a Vulkan guest of this GPU sees, in
     * `answer`: false, with `answer` untouched, while the GPU shows none. */
    [[nodiscard]] bool   vulkan_types(GuestMemoryTypes &answer);
    uint32_t             live() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    static constexpr uint64_t page_bytes = 4096u;

    /* How many preferences a class has, at most: three, among the types an
     * import may map and then among the rest. */
    static constexpr uint32_t most_choices = 6u;

    /* Reads the GPU's types, asks the exporter about each it has not asked
     * about, and remakes the guest's view when one was. The lock is held. */
    void          learn();
    /* The host memory types `request` may be answered with, best first, in
     * `types`: how many. A class falls back to the next when the GPU has no
     * room in the first - a VRAM BAR without resizable BAR is 256 MiB. A
     * Vulkan request names the guest's type, which the source must back. */
    uint32_t      choices(HostMemoryRequest request, uint32_t (&types)[most_choices]) const;
    /* The first type of `among` whose flags hold all of `wanted` and none of
     * `refused`, or memory_types. */
    uint32_t      first_type(uint32_t among, VkMemoryPropertyFlags wanted, VkMemoryPropertyFlags refused) const;
    SourceMemory *find(uint32_t domain, uint64_t key);
    /* One reference let go, under the lock; the memory freed with the last. */
    void          let_go(SourceMemory &memory);

    MemoryExporter              &exporter_;
    ObjectPool<SourceMemory>     memories_;
    mutable pthread_mutex_t      mutex_;
    /* The GPU's types as last read, which were asked of the exporter, which
     * passed, and which of those an importer may also map; and what a Vulkan
     * guest sees of them. */
    VkPhysicalDeviceMemoryProperties host_;
    uint32_t                     asked_;
    uint32_t                     mappable_;
    uint32_t                     visible_;
    GuestMemoryTypes             vulkan_;

    friend struct HostMemorySourceProbe;
};

}  // namespace av

#endif  // AV_HOST_MEMORY_SOURCE_HPP
