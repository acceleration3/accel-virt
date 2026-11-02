#ifndef AV_ICD_HOST_CHANNEL_HPP
#define AV_ICD_HOST_CHANNEL_HPP

#include "proto/resources/host_memory_request.hpp"

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"

#include "guest/transport/host_allocation.hpp"

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/retire_list.hpp"
#include "icd/common/locked_transport.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/fault_code.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class DriverLog;
class GuestConnection;
class StateWriter;
class ThreadLock;
}

namespace av::icd {

struct HostChannelProbe;

class ChannelShape {
public:
    ChannelShape(SubApi sub_api, uint8_t ring_order, uint32_t reply_bytes, size_t staging_bytes, size_t batch_bytes,
                 uint64_t arena_bytes);

private:
    friend class HostChannel;

    SubApi   sub_api_;
    uint8_t  ring_order_;
    uint32_t reply_bytes_;
    size_t   staging_bytes_;
    size_t   batch_bytes_;
    uint64_t arena_bytes_;
};

class HostChannel {
public:
    HostChannel(BlockSource &memory, DriverLog &log, const ChannelShape &shape, ThreadLock &lock);
    ~HostChannel();
    HostChannel(const HostChannel &) = delete;
    HostChannel &operator=(const HostChannel &) = delete;

    [[nodiscard]] Status open(GuestConnection &device, const GlContextOptions &options = GlContextOptions());
    void                 close(bool lost);

    bool opened() const;
    bool faulted() const;
    bool      lost() const;
    FaultCode fault_code() const;
    ClientTransport    &wire();
    KmtTransport       &transport();
    const KmtTransport &transport() const;
    [[nodiscard]] TransportResult flush();
    [[nodiscard]] TransportResult send_batch(const uint8_t *framed, size_t length);
    [[nodiscard]] Status          run_held();
    [[nodiscard]] TransportResult send_command(uint32_t command_id, WireWriter &writer);
    uint8_t          *arena_view() const;
    uint64_t          arena_bytes() const;
    uint64_t          host_errors() const;

    [[nodiscard]] Status open_memory(uint64_t bytes, HostMemoryRequest memory, const void *owner,
                                     HostAllocation *&made);
    [[nodiscard]] Status adopt_memory(void *address, uint64_t bytes, HostMemoryRequest memory, const void *owner,
                                      HostAllocation *&made);
    [[nodiscard]] Status close_memory(HostAllocation &allocation);
    void                 retire_memory(HostAllocation &allocation);
    void                 reclaim_retired();
    [[nodiscard]] Status retire_memory_after(HostAllocation &allocation, uint64_t fence);
    bool                 take_oldest_retired(HostAllocation *&allocation, uint64_t &fence);
    void                 reclaim_retired_through(uint64_t completed);
    [[nodiscard]] Status close_owned(const void *owner);
    HostAllocation      *find_memory(uint64_t handle, const void *owner);
    uint32_t             memories() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    friend struct HostChannelProbe;

    [[nodiscard]] Status open_arena(GuestConnection &device);
    struct Kept {
        ObjectStorage<HostAllocation> allocation_;
        const void                   *owner_;
    };

    [[nodiscard]] Status keep(const void *owner, Kept *&kept);
    [[nodiscard]] Status settle(Kept *kept, Status taken, HostAllocation *&made);
    [[nodiscard]] Status close_kept(Kept *kept, bool lost);
    void                 close_memories(bool lost);

    BlockSource                   &memory_;
    DriverLog                     &lines_;
    ThreadLock                    &lock_;
    SubApi                         sub_api_;
    uint8_t                        ring_order_;
    uint32_t                       reply_bytes_;
    size_t                         staging_bytes_;
    size_t                         batch_bytes_;
    uint64_t                       arena_bytes_;
    uint8_t                       *staging_;
    uint8_t                       *batch_block_;
    ObjectStorage<KmtTransport>     transport_;
    ObjectStorage<LockedTransport>  locked_;
    ObjectStorage<KmtBlobArena>     arena_;
    GuestConnection                *device_;
    ObjectPool<Kept>                          memories_;
    KeyedSlots<uint64_t, Kept *>              handles_;
    RetireList<HostAllocation *>              retired_;
    struct Fenced {
        HostAllocation *allocation_;
        uint64_t        fence_;
    };
    RetireList<Fenced>                        fenced_;
};

}

#endif
