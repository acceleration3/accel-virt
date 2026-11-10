#ifndef AV_HOST_MEMORY_SOURCE_HPP
#define AV_HOST_MEMORY_SOURCE_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "device/shim/posix_lock.hpp"
#include "executor/memory_reuse.hpp"
#include "executor/owned_fd.hpp"
#include "executor/source_memory.hpp"
#include "executor/vram_account.hpp"
#include "executor/vulkan/guest_memory_types.hpp"
#include "proto/context/vram_policy.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

struct AvHostMemory;

namespace av {

class BlockSource;
class ByteLedger;
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
    static constexpr uint32_t heap_type = HostMemoryRequest::memory_types;

    [[nodiscard]] Status place(uint64_t owner, uint64_t handle, uint32_t type, uint64_t size, uint32_t caching, int fd);
    [[nodiscard]] Status place_heap(uint64_t owner, uint64_t handle, uint64_t size, uint32_t caching, int fd);
    [[nodiscard]] Status take_placed(uint64_t owner, uint32_t type, uint64_t size, AvHostMemory &memory);
    void                 drop_placed(uint64_t owner, uint64_t handle);
    void                 drop_placed_of(uint64_t owner);
    uint32_t             placed() const;
    SourceMemory        *acquire(uint32_t domain, uint64_t key);
    void                 release(SourceMemory &memory);
    uint32_t             mappable_types();
    bool                 imports_heaps();
    [[nodiscard]] bool   vulkan_types(GuestMemoryTypes &answer);
    uint32_t             live() const;
    void                 reserve_vram(uint32_t mib);
    void                 use_host_ram(ByteLedger *ledger);
    void                 use_vram_ledger(ByteLedger *ledger);
    uint64_t             host_ram_held() const;
    VramPolicy           vram_policy() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    static constexpr uint64_t page_bytes = 4096u;

    static constexpr uint32_t most_choices = 32u;

    struct Placed {
        Placed(uint64_t owner, uint64_t handle, uint32_t type, uint64_t size, uint32_t caching, uint64_t sequence)
            : owner_(owner), handle_(handle), size_(size), sequence_(sequence), type_(type), caching_(caching), fd_()
        {
        }
        uint64_t owner_;
        uint64_t handle_;
        uint64_t size_;
        uint64_t sequence_;
        uint32_t type_;
        uint32_t caching_;
        OwnedFd  fd_;
    };

    static constexpr uint32_t most_placed_per_owner = 16u;

    Status   keep_placed(uint64_t owner, uint64_t handle, uint32_t type, uint64_t size, uint32_t caching, int fd);
    uint32_t placed_of(uint64_t owner) const;
    void     drop_oldest_placed(uint64_t owner);

    void          learn();
    uint32_t      choices(HostMemoryRequest request, uint32_t (&types)[most_choices]) const;
    bool          system_type(uint32_t type) const;
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
    ObjectPool<Placed>           placed_;
    uint64_t                     placed_sequence_;
    mutable PosixLock            lock_;
    VkPhysicalDeviceMemoryProperties host_;
    uint32_t                     asked_;
    uint32_t                     mappable_;
    uint32_t                     visible_;
    uint32_t                     alike_;
    GuestMemoryTypes             vulkan_;
    VramPolicy                   vram_;
    VramAccount                  host_ram_;
    VramAccount                  device_ram_;
    MemoryReuse                  reuse_;

    friend struct HostMemorySourceProbe;
};

}

#endif
