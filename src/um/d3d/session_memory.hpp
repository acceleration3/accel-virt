#ifndef AV_UM_SESSION_MEMORY_HPP
#define AV_UM_SESSION_MEMORY_HPP

#include "core/support/object_storage.hpp"
#include "icd/common/stream_memory.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "um/common/performance_clock.hpp"
#include "um/common/srw_lock.hpp"
#include "um/d3d/lent_pages.hpp"
#include "icd/common/locked_timeline.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class DriverLog;
class HostAllocation;
class ThreadLock;
}

namespace av::icd {
class HostChannel;
}

namespace av::d3d {

class SliceOffer;

class SessionMemory {
public:
    SessionMemory(BlockSource &memory, icd::HostChannel &channel, ThreadLock &channel_lock, DriverLog &lines,
                  const SliceOffer *offer, const void *owner);
    SessionMemory(const SessionMemory &) = delete;
    SessionMemory &operator=(const SessionMemory &) = delete;

    void open(ClientTransport &host);
    void close_streams();
    void release_pages();

    icd::StreamMemory *stream(HostMemoryRequest::Kind kind);
    void               retire_memory(HostAllocation &allocation);
    void               retire_pages(void *pages);
    void               reclaim();
    void               trim();
    uint64_t           gpu_fence() const;
    bool               gpu_pending(uint64_t fence) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t stream_kinds = 2u;

    bool host_grants_slices(ClientTransport &host);

    BlockSource                     &memory_;
    icd::HostChannel                &channel_;
    ThreadLock                      &channel_lock_;
    DriverLog                       &lines_;
    const SliceOffer                *offer_;
    const void                      *owner_;
    SrwLock                          slices_lock_;
    ObjectStorage<icd::LockedTimeline> timeline_;
    ObjectStorage<icd::StreamMemory> streams_[stream_kinds];
    LentPages                        lent_pages_;
    PerformanceClock                 clock_;
};

}

#endif
