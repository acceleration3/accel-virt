#ifndef AV_UM_D3D_SESSION_HPP
#define AV_UM_D3D_SESSION_HPP

#include "proto/resources/host_memory_request.hpp"

#include "core/support/object_storage.hpp"
#include "um/d3d/query_results.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/id_mint.hpp"
#include "icd/common/stream_memory.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "icd/d3d/session_timeline.hpp"
#include "um/d3d/d3d11_ddi.hpp"
#include "um/d3d/lent_pages.hpp"
#include "core/support/object_pool.hpp"
#include "um/common/performance_clock.hpp"
#include "um/common/srw_lock.hpp"

#include <stdint.h>

namespace av {
class DriverLog;
}

namespace av {
class BlockSource;
class HostAllocation;
class Kmt;
}


namespace av::d3d11 {
class D3d11Device;
}

namespace av::d3d {

class SliceOffer;

class D3dSession {
public:
    D3dSession(Kmt &kmt, BlockSource &memory, DriverLog &log, uint64_t arena_size, uint32_t select_command,
               uint32_t flush_command, const SliceOffer *offer);
    ~D3dSession();
    D3dSession(const D3dSession &) = delete;
    D3dSession &operator=(const D3dSession &) = delete;

    HRESULT join(uint32_t adapter);
    [[nodiscard]] Status escape(uint8_t *data, uint32_t size);
    void    leave();
    uint32_t users() const;
    bool      lost() const;
    FaultCode fault_code() const;

    uint32_t               next_id();
    d3d::D3dCommandStream &stream();
    ThreadLock            &lock();
    d3d11::QueryResults   &query_results();
    uint8_t               *arena_view() const;
    uint64_t               arena_bytes() const;
    ClientBlobArena       &blobs();
    KmtTransport          &transport();
    Kmt                   &kmt() const;
    ObjectPool<d3d11::D3d11Device> &contexts();
    icd::StreamMemory              *memory_stream(HostMemoryRequest::Kind kind);
    [[nodiscard]] Status            open_memory(uint64_t bytes, HostMemoryRequest memory, HostAllocation *&made);
    [[nodiscard]] Status            adopt_memory(void *address, uint64_t bytes, HostMemoryRequest memory,
                                                 HostAllocation *&made);
    void                            close_memory(HostAllocation &allocation);
    void                            retire_memory(HostAllocation &allocation);
    void                            retire_pages(void *pages);
    void                            reclaim_retired();
    void                            trim_streams();

private:
    static constexpr uint8_t  ring_order = 22u;
    static constexpr uint32_t reply_bytes = 64u * 1024u;
    static constexpr size_t   staging_bytes = 512u * 1024u;
    static constexpr size_t   batch_bytes = 1u << 20u;
    static constexpr uint32_t stream_kinds = 2u;

    HRESULT open(uint32_t adapter);
    void    close();
    bool    host_grants_slices();

    BlockSource                    &memory_;
    DriverLog                      &lines_;
    SrwLock                              lock_;
    GuestConnection                      connection_;
    icd::HostChannel                     channel_;
    ObjectStorage<d3d::D3dCommandStream> stream_;
    const SliceOffer                    *offer_;
    SrwLock                              slices_lock_;
    ObjectStorage<SessionTimeline>        timeline_;
    ObjectStorage<icd::StreamMemory>      streams_[stream_kinds];
    LentPages                            lent_pages_;
    PerformanceClock                     clock_;
    d3d11::QueryResults                  query_results_;
    ObjectPool<d3d11::D3d11Device>       contexts_;
    uint32_t                             members_;
    icd::IdMint<uint32_t>                ids_;
    uint32_t                             select_command_;
    uint32_t                             flush_command_;
};

}

#endif
