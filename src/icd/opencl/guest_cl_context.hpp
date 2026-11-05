#ifndef AV_CLICD_GUEST_CL_CONTEXT_HPP
#define AV_CLICD_GUEST_CL_CONTEXT_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "icd/opencl/cl_arena_blocks.hpp"
#include "icd/opencl/cl_context_notify.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/context_link.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/loss_latch.hpp"
#include "icd/common/open_mappings.hpp"
#include "icd/opencl/cl_d3d11_objects.hpp"
#include "icd/opencl/cl_external_memory.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "icd/opencl/mapped_region.hpp"
#include "icd/opencl/svm_table.hpp"

#include "icd/common/arena_region.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class HostAllocation;
class KeyedLog;
class StateWriter;
class ThreadLock;
}

namespace av::icd {
class IcdConnection;
class SharedHandles;
}

namespace av::clicd {

struct GuestClContextProbe;

class GuestClContext {
public:
    enum class State : uint8_t { created, made, lost };

    static constexpr uint8_t  ring_order = 20u;
    static constexpr uint32_t reply_bytes = 1024u * 1024u;
    static constexpr size_t   staging_bytes = 256u * 1024u;
    static constexpr size_t   batch_bytes = 2u * staging_bytes;
    static constexpr uint64_t arena_bytes = uint64_t{16} << 20u;
    static constexpr uint64_t staging_reserve = uint64_t{8} << 20u;
    static constexpr uint64_t blocks_reserve = uint64_t{8} << 20u;

    GuestClContext(icd::IcdConnection &connection, BlockSource &memory, ThreadLock &send_lock,
                   ThreadLock &blocks_lock, KeyedLog &lines, icd::SharedHandles &handles);
    ~GuestClContext();
    GuestClContext(const GuestClContext &) = delete;
    GuestClContext &operator=(const GuestClContext &) = delete;

    [[nodiscard]] Status make(uint32_t gl_share = 0u, uint64_t wake = 0u);
    bool                 wakes() const;
    State                state() const;
    bool                 shares_gl() const;
    bool                 on(const icd::IcdConnection &connection) const;
    void                 keep_properties(const cl_context_properties *properties);
    void                 notify_with(const ClContextNotify &notify);
    uint32_t             properties(cl_context_properties *out, uint32_t capacity) const;

    ClientTransport *wire();
    [[nodiscard]] TransportResult flush();
    [[nodiscard]] Status lend(void *address, uint64_t bytes, HostAllocation *&made);
    void                 give_back_lent(HostAllocation &lent);
    void                 retire(const uint64_t *block, HostAllocation *lent);
    void                 reclaim();
    void                 orphan_work();
    ClArenaBlocks   &blocks();
    ClArenaBlocks   &staging();
    bool             lost() const;
    SvmTable        &svm();
    ClExternalMemory &external();
    ClD3d11Objects   &d3d11();
    uint32_t         guest_device() const;

    MappedRegion    *take_mapping(uint64_t buffer, uint64_t host, size_t offset, size_t bytes,
                                  cl_map_flags flags);
    MappedRegion    *take_shared_mapping(uint64_t buffer, uint64_t host, size_t offset,
                                         size_t bytes, uint8_t *at);
    MappedRegion    *take_application_mapping(uint64_t buffer, uint64_t host, size_t offset, size_t bytes,
                                              cl_map_flags flags, uint8_t *at);
    MappedRegion    *take_image_mapping(uint64_t image, const size_t *origin, const size_t *region,
                                        size_t element_bytes, cl_map_flags flags);
    MappedRegion    *mapping_at(uint64_t buffer, const void *pointer);
    void             give_back_mapping(MappedRegion &region);
    uint32_t         give_back_mappings_of(uint64_t buffer);
    uint32_t         mappings() const;
    bool             has_arena() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    friend struct GuestClContextProbe;

    [[nodiscard]] Status build(uint32_t gl_share);
    void                 teardown();

    BlockSource        &memory_;
    KeyedLog           &lines_;
    icd::ContextLink    link_;
    icd::LossLatch      loss_;
    uint32_t            gl_share_;
    bool                wakes_;
    icd::HostChannel                     channel_;
    ClArenaBlocks                        blocks_;
    bool                                 orphaned_work_;
    ClArenaBlocks                        staging_;
    icd::ArenaRegion                     region_;
    SvmTable                             svm_;
    ClExternalMemory                     external_;
    ClD3d11Objects                       d3d11_;
    icd::OpenMappings<MappedRegion>      mappings_;
    ClPropertyList                       properties_;
    ClContextNotify                      notify_;
    State                                state_;
};

}

#endif
