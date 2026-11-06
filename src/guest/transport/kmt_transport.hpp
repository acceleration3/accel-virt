#ifndef AV_KMT_TRANSPORT_HPP
#define AV_KMT_TRANSPORT_HPP

#include "core/support/status.hpp"
#include "guest/transport/batch_mark.hpp"
#include "guest/transport/channel_stats.hpp"
#include "guest/transport/framed_block.hpp"
#include "guest/transport/guest_doorbell.hpp"
#include "guest/transport/host_watch.hpp"
#include "guest/transport/reply_reader.hpp"
#include "guest/transport/submission_path.hpp"
#include "guest/transport/transport_fence.hpp"
#include "guest/transport/transport_ring.hpp"
#include "proto/escape/follow_record.hpp"
#include "proto/wire/spill_block.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/client/transfer_room.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class KmtBlobArena;

class BlockSource;
class ContextViewsRecord;

class GuestConnection;
class PublishGate;
class ReleaseRoute;
struct KmtTransportProbe;

class KmtTransport final : public ClientTransport {
public:
    static constexpr uint32_t pressure_waits = 4u;
    static constexpr uint64_t pressure_wait_ticks = 1000000u;
    static constexpr uint64_t pressure_percent = 75u;
    static constexpr uint64_t pressure_window_ticks = 10000000u;

    static constexpr uint32_t dma_bytes    = 4096u;

    KmtTransport(GuestConnection &connection, BlockSource &memory, SubApi sub_api, uint8_t ring_order,
                 uint32_t reply_bytes, uint8_t *staging, size_t staging_bytes);
    KmtTransport(GuestConnection &connection, SubApi sub_api, uint8_t ring_order, uint32_t reply_bytes,
                 uint8_t *staging, size_t staging_bytes);
    ~KmtTransport();

    [[nodiscard]] Status initialize(const GlContextOptions &options = GlContextOptions());
    void                 teardown();
    bool                 established() const;
    bool                 faulted() const;
    bool                 host_faulted() const;
    uint32_t             host_fault_code() const;
    uint64_t gpu_completed() const;
    bool     knows_gpu_progress() const;
    uint64_t published_fence() const;
    uint64_t covering_fence() const;
    [[nodiscard]] Status await_gpu(uint64_t fence);
    uint64_t             submissions_through_doorbell() const;
    bool                 ready_for_batch() const;
    void                 hold(PublishGate &gate);
    void                 forget(const PublishGate &gate);
    [[nodiscard]] Status release_by(ReleaseRoute &route);
    void                 release_directly(const ReleaseRoute &route);
    bool                 releases_by_packet() const;
    bool                 routed_by(const ReleaseRoute &route) const;
    void                 hold_in(uint8_t *block, size_t capacity);
    [[nodiscard]] TransportResult flush();
    size_t                        held() const;
    size_t                        holding_capacity() const;
    [[nodiscard]] TransportResult hold_framed(const uint8_t *framed, size_t length);
    [[nodiscard]] Status wake_on_raise(uint64_t handle);
    uint64_t             submissions_through_kernel() const;
    uint64_t             submissions() const;
    void                 give_back_doorbell();
    uint32_t             context() const;
    uint32_t             ring() const;

    [[nodiscard]] Status nop(uint32_t &answered);
    [[nodiscard]] Status run_held();

    WireWriter      request_writer() override;
    WireWriter      request_writer_with_room(const uint64_t *sizes, uint32_t count) override;
    TransportResult send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void            end_call() override;
    [[nodiscard]] bool make_room_for_blobs(const uint64_t *sizes, uint32_t count) override;
    ClientBlobLease    open_region(uint64_t bytes) override;
    void               close_region(const ClientBlobLease &region) override;
    void               attach_arena(KmtBlobArena *arena);
    void               release_idle_segments();
    bool               grow_if_pressed();
    void               tune_arena();
    const TransferRoom &room() const override;
    void                note(ChannelCounter counter, uint64_t amount) override;
    TransportResult send_batch(const uint8_t *framed, size_t length, WireReader &reply);
    uint64_t marker_address() const;
    FollowRecord follow() const;
    BatchMark    newest_mark() const;
    WireArena      &decode_arena() override;
    ClientBlobArena &blobs() override;

    bool invariants_hold() const;

private:
    static constexpr uint32_t allocation_count = 2u;
    static constexpr uint32_t name_index  = 0u;
    static constexpr uint32_t dma_index   = 1u;
    static constexpr uint32_t wait_timed_out = 0xC00000B5u;
    static constexpr uint64_t reply_spin_us = 1000u;
    static constexpr uint64_t fence_spin_us = 1000u;
    static constexpr uint64_t drain_spin_us = 2000u;
    static constexpr uint64_t grow_spin_us = 1000u;
    static constexpr uint64_t ring_spin_us = 50000u;
    static constexpr uint64_t gpu_spin_us = 20000u;

    bool                 record_carries(const GlContextOptions &options) const;
    [[nodiscard]] Status setup_allocation(uint32_t index, AllocationKind kind, uint64_t bytes, bool map_gpu,
                                          uint64_t &address);
    [[nodiscard]] Status adopt_views();
    [[nodiscard]] Status ask_views(ContextViewsRecord &views);
    [[nodiscard]] TransportResult publish(uint32_t command_id, WireWriter &writer, uint8_t flags,
                                          uint64_t &fence);
    [[nodiscard]] TransportResult publish_pieces(uint32_t command_id, const WireWriter &writer, uint8_t flags,
                                                 uint64_t &fence);
    [[nodiscard]] Status          await_slot(uint64_t fence, WireReader &reply);
    [[nodiscard]] TransportResult fetch_answer(WireReader &reply);
    [[nodiscard]] TransportResult fetch_part(uint64_t offset, WireReader &part);
    [[nodiscard]] bool drain();
    void               retire_consumed();
    [[nodiscard]] bool room_for_fence();
    uint64_t           published_now() const;
    [[nodiscard]] TransportResult publish_batch(const uint8_t *batch, uint32_t length, uint64_t &fence);
    [[nodiscard]] Status          write_when_room(const uint8_t *batch, uint32_t length, const uint8_t *trailer,
                                                  uint32_t trailer_length, uint64_t &next, uint64_t &begin,
                                                  uint64_t &end);
    [[nodiscard]] TransportResult submit_written(uint64_t begin, uint64_t end, uint64_t &fence);
    uint32_t                      stats_trailer(const uint8_t *batch, uint32_t length, uint64_t now, uint8_t *into);
    void                          note_wait(uint64_t began, ChannelCounter waits, ChannelCounter wait_us);
    [[nodiscard]] TransportResult hold(uint32_t command_id, const WireWriter &writer, uint8_t flags);
    [[nodiscard]] TransportResult hold_pieces(uint32_t command_id, const WireWriter &writer, uint8_t flags);
    [[nodiscard]] TransportResult publish_held_answered(WireReader &reply);
    [[nodiscard]] TransportResult await(uint64_t fence, WireReader &reply);
    bool                          replied_within_spin(uint64_t fence, WireReader &reply, Status &read);
    bool                          consumed_within_spin();
    bool                          consumed_for_blobs(const uint64_t *sizes, uint32_t count);
    bool                          grow_for_blobs(const uint64_t *sizes, uint32_t count);
    bool                          grow_arena(uint64_t need);
    void                          spin_once() const;
    template <typename Arrived> bool arrives_eventually(Arrived arrived);
    [[nodiscard]] Status          wait_for_host(uint64_t fence);
    [[nodiscard]] Status          wait_on_page(uint64_t fence);
    static constexpr uint64_t     slow_wait_seconds = 3u;
    bool                          kernel_waits_for(uint64_t fence) const;
    bool                          room_within_spin(uint64_t span);

    GuestConnection &connection_;
    SubApi           sub_api_;
    uint8_t          ring_order_;
    uint32_t         reply_bytes_;
    uint8_t         *staging_;
    size_t           staging_bytes_;
    TransferRoom     room_;
    uint32_t         allocations_[allocation_count];
    uint64_t         dma_address_;
    uint32_t         context_;
    bool             hardware_scheduled_;
    TransportRing    ring_;
    uint64_t         newest_end_;
    uint64_t         newest_fence_;
    uint64_t         previous_end_;
    GuestDoorbell    doorbell_;
    SubmissionPath   path_;
    ReplyReader      reply_;
    HostWatch        watch_;
    TransportFence   fences_;
    uint64_t         fence_ends_[TransportFence::max_outstanding];
    bool             faulted_;
    static constexpr uint32_t most_gates = 4u;
    PublishGate     *held_[most_gates];
    const ReleaseRoute *route_;
    const ReleaseRoute *standby_;
    uint32_t         gates_busy_;

    void             lock_gates();
    void             unlock_gates();
    FramedBlock      pending_;
    uint32_t         unpublished_;
    bool             answered_;
    uint64_t         answered_fence_;
    WireArena        decode_arena_;
    ClientBlobArena  blobs_;
    KmtBlobArena    *arena_;
    uint32_t         room_pressure_;
    uint64_t         room_wait_ticks_;
    uint64_t         window_began_;
    bool             windowed_;
    SpillBlock       spill_;
    SpillBlock       answer_;
    ChannelStats     stats_;

    friend struct KmtTransportProbe;
};

}

#endif
