#ifndef AV_HOST_MEMORY_SOURCE_HPP
#define AV_HOST_MEMORY_SOURCE_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "device/shim/posix_lock.hpp"
#include "executor/source_memory.hpp"
#include "executor/vulkan/guest_memory_types.hpp"
#include "proto/context/vram_policy.hpp"
#include "proto/resources/host_memory_request.hpp"

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
    ~HostMemorySource();
    HostMemorySource(const HostMemorySource &) = delete;
    HostMemorySource &operator=(const HostMemorySource &) = delete;

    [[nodiscard]] Status allocate(uint32_t domain, uint64_t key, uint64_t size, HostMemoryRequest request,
                                  AvHostMemory &memory);
    void                 free(uint32_t domain, uint64_t key);
    void                 free_domain(uint32_t domain);
    void                 release_aliases();
    [[nodiscard]] Status alias(uint32_t domain, uint64_t key, HostMemorySource &from, uint32_t from_domain,
                               uint64_t from_key, AvHostMemory &memory);
    SourceMemory        *acquire(uint32_t domain, uint64_t key);
    void                 release(SourceMemory &memory);
    uint32_t             mappable_types();
    [[nodiscard]] bool   vulkan_types(GuestMemoryTypes &answer);
    uint32_t             live() const;
    void                 reserve_vram(uint32_t mib);
    VramPolicy           vram_policy() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    static constexpr uint64_t page_bytes = 4096u;

    static constexpr uint32_t most_choices = 6u;

    void          learn();
    uint32_t      choices(HostMemoryRequest request, uint32_t (&types)[most_choices]) const;
    uint32_t      first_type(uint32_t among, VkMemoryPropertyFlags wanted, VkMemoryPropertyFlags refused) const;
    SourceMemory *find(uint32_t domain, uint64_t key);
    SourceMemory *aliased(uint32_t domain, uint64_t key);

    void          let_go(SourceMemory &memory);

    MemoryExporter              &exporter_;
    struct Alias {
        uint32_t      domain_;
        uint64_t      key_;
        SourceMemory *memory_;
    };

    ObjectPool<SourceMemory>     memories_;
    ObjectPool<Alias>            aliases_;
    mutable PosixLock            lock_;
    VkPhysicalDeviceMemoryProperties host_;
    uint32_t                     asked_;
    uint32_t                     mappable_;
    uint32_t                     visible_;
    GuestMemoryTypes             vulkan_;
    VramPolicy                   vram_;

    friend struct HostMemorySourceProbe;
};

}

#endif
