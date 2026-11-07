#ifndef AV_CONTEXT_SESSION_HPP
#define AV_CONTEXT_SESSION_HPP

#include "core/support/status.hpp"
#include "executor/channel_stats_latest.hpp"
#include "executor/import_table.hpp"
#include "executor/shim/host_heap.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/submission/blob_role.hpp"
#include "proto/wire/spill_block.hpp"
#include "proto/wire/wire_arena.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DriverLog;
class GpuClockHold;
class HostMemorySource;
class SessionBackend;
class StateWriter;
class TeardownBudget;

class ContextSession {
public:
    static constexpr size_t arena_bytes = 1u << 20;
    static constexpr uint32_t parked_most = ClientBlobArena::max_segments + ClientBlobArena::max_regions + 1u;

    explicit ContextSession(DriverLog &lines);
    ContextSession(const ContextSession &) = delete;
    ContextSession &operator=(const ContextSession &) = delete;

    [[nodiscard]] Status initialize(SessionBackend &backend, uint8_t *block, size_t block_bytes);
    [[nodiscard]] bool   drain(TeardownBudget &budget);
    [[nodiscard]] bool   release(TeardownBudget &budget);
    void                 vacate();
    void                 let_go_of_thread();
    bool                 live() const;
    bool                 faulted() const;
    void                 hold_clocks(GpuClockHold &clocks);
    void                 draw_from(HostMemorySource &source, uint32_t domain);

    void     end_batch();
    uint32_t poll();
    void     bind_signal(void (*signal)(void *context), void *context);
    void     gpu_progress(uint64_t &marked, uint64_t &reached) const;
    uint32_t execute(uint32_t command_id, const uint8_t *payload, size_t size, uint8_t *reply,
                     size_t reply_capacity, size_t &answer);
    size_t   answer_part(uint64_t offset, uint8_t *out, size_t capacity, uint64_t &length) const;

    uint32_t import_blob(uint64_t blob_id, uint64_t memory_key, const BlobRole &role = BlobRole());
    void     release_blob(uint64_t blob_id);
    uint64_t imported_handle(uint64_t blob_id);
    ImportTable &imports();
    uint32_t lose_device();
    void     channel_stats(uint32_t context_id, const uint8_t *body, size_t size);
    void     report_channel(StateWriter &writer) const;


    size_t   starved() const;
    uint32_t parked() const;

    bool invariants_hold() const;

private:
    friend struct ContextSessionProbe;

    struct Parked {
        uint64_t blob_id_;
        uint64_t mark_;
    };

    void retire_blob(uint64_t blob_id);
    void drain_parked();

    SessionBackend *backend_;
    GpuClockHold   *clocks_;
    HostMemorySource *source_;
    uint32_t        domain_;
    HostHeap        heap_;
    WireArena       arena_;
    SpillBlock      answer_;
    size_t          answer_length_;
    ImportTable     imports_;
    size_t          starved_;
    ChannelStatsLatest channel_;
    bool            faulted_;
    bool            drained_;
    Parked          parked_[parked_most];
    uint32_t        parked_count_;
    DriverLog      &lines_;
};

}

#endif
