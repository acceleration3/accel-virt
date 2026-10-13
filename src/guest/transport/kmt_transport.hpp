/* One sub-API context's transport to the host, over D3DKMT (docs/protocol.md;
 * feature 043). It is the ClientTransport the generated client is handed: it
 * holds the page that names the context and a small DMA buffer for the
 * submission descriptor, allocated on a shared GuestConnection's device, and
 * the views the driver gives it of the ring and reply slot the context runs on
 * - memory the driver owns, for the context's life ("Context memory").
 * request_writer lends room for one command in staging memory the caller owns,
 * sized to its largest command - for an ICD far more than a stack holds
 * (feature 045); send_* frames it as a batch, publishes it, rings it through
 * D3DKMTSubmitCommand on the virtual context, and - for a synchronous call -
 * waits for the context and reads the reply. send_batch publishes commands the
 * caller framed itself. The record it was built with is the private data the
 * create-context carried; the miniport decoded it. */
#ifndef AV_KMT_TRANSPORT_HPP
#define AV_KMT_TRANSPORT_HPP

#include "core/support/status.hpp"
#include "guest/transport/batch_mark.hpp"
#include "guest/transport/guest_doorbell.hpp"
#include "guest/transport/reply_reader.hpp"
#include "guest/transport/submission_path.hpp"
#include "guest/transport/transport_fence.hpp"
#include "guest/transport/transport_ring.hpp"
#include "proto/escape/follow_record.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextViewsRecord;

class GuestConnection;
class PublishGate;
struct KmtTransportProbe;

class KmtTransport final : public ClientTransport {
public:
    static constexpr uint32_t dma_bytes    = 4096u;

    /* A ring of 2^ring_order data bytes, a reply slot of `reply_bytes` (whole
     * pages), and `staging_bytes` of the caller's memory at `staging`, which
     * must outlive this: the largest command, header included, it can send. */
    KmtTransport(GuestConnection &connection, SubApi sub_api, uint8_t ring_order, uint32_t reply_bytes,
                 uint8_t *staging, size_t staging_bytes);
    ~KmtTransport();

    /* Creates the context's name and its DMA buffer, creates the virtual
     * context whose private data - the AvContextRecord - names it and sizes
     * its reply slot, and asks the driver where it mapped the ring and reply
     * slot it made with it. invalid_state once set up; invalid_parameter, with
     * nothing asked of the Kmt, for staging with no room past a command header
     * or a reply slot the record cannot size; the Kmt's own error otherwise,
     * and malformed for views the driver could not have answered, with nothing
     * left open. The record carries `options` (feature 050): options it cannot
     * carry - any but the defaults off OpenGL, or none OpenGL has - are
     * invalid_parameter, asked of no one. */
    [[nodiscard]] Status initialize(const GlContextOptions &options = GlContextOptions());
    void                 teardown();
    bool                 established() const;
    /* A device-side failure (a refused submission, a faulted wait, a bad reply)
     * loses the context: every later send fails until it is torn down. */
    bool                 faulted() const;
    /* The highest fence whose batch the host GPU has finished, read from the
     * context's fence page (docs/protocol.md, "Fence page"): memory reused
     * after a batch is free once this passes the batch's fence. 0 before any,
     * and always 0 from a driver that maps no fence page into the process -
     * knows_gpu_progress() says which. */
    uint64_t gpu_completed() const;
    bool     knows_gpu_progress() const;
    /* How this transport's submissions went (feature 049): through the
     * doorbell, and through the kernel. */
    uint64_t             submissions_through_doorbell() const;
    /* The host has taken every batch but the newest: one more now keeps it
     * fed, and holding a batch back until then lets it grow instead. Never
     * while held: the batch keeps growing until something must send it. */
    bool                 ready_for_batch() const;
    /* After a present that hands a buffer to another process: the next batch
     * written to the ring, whatever sends it, passes `gate` first, and until
     * then no batch is ready (PublishGate). `gate` outlives that write. */
    void                 hold(PublishGate &gate);
    /* `gate` is going away: a hold by it is dropped, one by another kept. */
    void                 forget(const PublishGate &gate);
    /* Has the driver set the event `handle` names - this process's, one it may
     * set - whenever the host raises on this context, and when the context is
     * lost or goes (docs/protocol.md, "Wake event record"). invalid_state
     * before the context is set up or once lost; the Kmt's own error
     * otherwise, which is not_supported from a driver or device that never
     * raises, and the context goes on without. */
    [[nodiscard]] Status wake_on_raise(uint64_t handle);
    uint64_t             submissions_through_kernel() const;
    /* Every batch this transport has submitted: what has not moved has sent
     * nothing in between. */
    uint64_t             submissions() const;
    /* Gives the doorbell back: every submission after this goes through the
     * kernel, which is how the in-guest benchmark measures the two paths on
     * one boot (feature 049, US3). Harmless without one. */
    void                 give_back_doorbell();
    /* The virtual context's D3DKMT handle, for a call the driver takes beside
     * the transport - an escape on this context; 0 unless established. */
    uint32_t             context() const;
    /* The D3DKMT handle of the page that names the context, by which another
     * OpenGL context of this process names this one to share with; 0 unless
     * established. */
    uint32_t             ring() const;

    /* A round trip of the transport's own no-op (feature 044), which the
     * device answers itself: ok when answered with nothing; invalid_state when
     * the call failed, which lost the context; malformed when the answer held
     * bytes, `answered` saying how many. */
    [[nodiscard]] Status nop(uint32_t &answered);

    WireWriter      request_writer() override;
    TransportResult send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    /* Nothing is held for a caller: one thread owns this transport. A call
     * the host answered retires every blob lease, since the host has run
     * every command sent before it. */
    void            end_call() override;
    /* `length` bytes of CommandHeader-framed commands, published as one batch:
     * the whole batch is judged before anything is written, and one that does
     * not frame exactly is refused without faulting the context. When any
     * command asks for an answer the call waits, and `reply` is the answer the
     * device kept - the last such command's; otherwise it is empty. An
     * answered batch ends its call as a synchronous command does: end_call
     * retires every blob lease. */
    /* Room for a `bytes` lease in the blob arena, waiting for the oldest
     * batches the host has yet to run to come back - not for all of them.
     * false when what is unsent holds the room, or the context faulted. */
    [[nodiscard]] bool make_room_for_blobs(size_t bytes);
    TransportResult send_batch(const uint8_t *framed, size_t length, WireReader &reply);
    TransportResult send_async_batch(const uint8_t *framed, size_t length);
    /* The GPU address a packet on another of this process's contexts names:
     * the transport's DMA buffer, which every context of the process reaches
     * and the miniport reads nothing of there. */
    uint64_t marker_address() const;
    /* What a packet on another of this process's contexts carries to complete
     * no sooner than the newest batch published here (FollowRecord); a record
     * that names no fence before the first batch. */
    FollowRecord follow() const;
    /* The same newest batch, for another thread of this process to wait on
     * (BatchMark). */
    BatchMark    newest_mark() const;
    TransportResult send_answered_batch(const uint8_t *framed, size_t length, WireReader &reply);
    WireArena      &decode_arena() override;
    ClientBlobArena &blobs() override;

    bool invariants_hold() const;

private:
    static constexpr uint32_t allocation_count = 2u;   /* name, dma */
    static constexpr uint32_t name_index  = 0u;
    static constexpr uint32_t dma_index   = 1u;
    /* The NTSTATUS a kernel wait that ran out its bound leaves in last_code(). */
    static constexpr uint32_t wait_timed_out = 0xC00000B5u;

    /* Whether the record can carry `options` for this sub-API and order. */
    bool                 record_carries(const GlContextOptions &options) const;
    [[nodiscard]] Status setup_allocation(uint32_t index, AllocationKind kind, uint64_t bytes, bool map_gpu,
                                          uint64_t &address);
    /* The ring and reply slot where the driver mapped them for this context. */
    [[nodiscard]] Status adopt_views();
    /* One views escape of `views`' version, its answer accepted into it. */
    [[nodiscard]] Status ask_views(ContextViewsRecord &views);
    [[nodiscard]] TransportResult publish(uint32_t command_id, WireWriter &writer, uint8_t flags,
                                          uint64_t &fence);
    /* Waits for everything sent so far, and retires its fences. */
    [[nodiscard]] bool drain();
    void               retire_consumed();
    [[nodiscard]] bool room_for_fence();
    uint64_t           published_now() const;
    [[nodiscard]] TransportResult publish_batch(const uint8_t *batch, uint32_t length, uint64_t &fence);
    /* Waits for the context and points `reply` at the answer for `fence`. */
    [[nodiscard]] TransportResult await(uint64_t fence, WireReader &reply);
    bool                          replied_within_spin(uint64_t fence);
    bool                          consumed_within_spin();
    void                          spin_once() const;
    template <typename Arrived> bool arrives_eventually(Arrived arrived);
    [[nodiscard]] Status          wait_for_host(uint64_t fence);
    bool                          room_within_spin(uint64_t span);

    GuestConnection &connection_;
    SubApi           sub_api_;
    uint8_t          ring_order_;
    uint32_t         reply_bytes_;
    uint8_t         *staging_;
    size_t           staging_bytes_;
    uint32_t         allocations_[allocation_count];   /* name, dma */
    uint64_t         dma_address_;
    uint32_t         context_;
    /* Whether the OS gave this context out as one a hardware queue may run on:
     * a context that is hardware scheduled cannot be submitted to through
     * D3DKMTSubmitCommand at all (feature 049, research.md §13). */
    bool             hardware_scheduled_;
    TransportRing    ring_;
    uint64_t         newest_end_;
    /* The fence of the newest batch published, which follow() names. */
    uint64_t         newest_fence_;
    uint64_t         previous_end_;
    /* Feature 049: the doorbell this context submits through where the OS
     * gives it one, and the choice made per submission. A context that never
     * gets a doorbell submits exactly as it did before. */
    GuestDoorbell    doorbell_;
    SubmissionPath   path_;
    ReplyReader      reply_;
    /* The fence page, read-only; null from a driver that maps none. */
    const uint8_t   *fence_page_;
    TransportFence   fences_;
    /* Where each outstanding fence's batch ends in the ring. */
    uint64_t         fence_ends_[TransportFence::max_outstanding];
    bool             faulted_;
    /* What the next batch written waits for; null when nothing is held. */
    PublishGate     *held_;
    /* A synchronous call was answered since the last end_call. */
    bool             answered_;
    WireArena        decode_arena_;
    ClientBlobArena  blobs_;

    friend struct KmtTransportProbe;
};

}  // namespace av

#endif  // AV_KMT_TRANSPORT_HPP
