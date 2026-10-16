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
 * create-context carried; the miniport decoded it.
 *
 * Nothing the caller sends or asks for is bounded by those rooms
 * (docs/protocol.md, "Long commands"): a writer it lends that outgrows the
 * staging spills into memory of the transport's own, and the command crosses
 * in pieces, each a batch; an answer longer than the reply slot is fetched in
 * parts into memory of its own, kept until the call ends.
 *
 * A driver that batches has its commands held here (hold_in), not in a
 * layer of its own over the transport: held anywhere else, a command of the
 * transport's own - a Direct3D 12 heap's blob import - went to the host
 * ahead of commands the application made before it, and the blob leases
 * those commands named were retired and lent again before the host read
 * them. Held here, every command goes in the order it was made whoever sends
 * it, and a lease is retired only once the host has run the batch that
 * carried the whole of its command (ClientBlobArena::seal). */
#ifndef AV_KMT_TRANSPORT_HPP
#define AV_KMT_TRANSPORT_HPP

#include "core/support/status.hpp"
#include "guest/transport/batch_mark.hpp"
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
#include "proto/context/gl_context_options.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;
class ContextViewsRecord;

class GuestConnection;
class PublishGate;
struct KmtTransportProbe;

class KmtTransport final : public ClientTransport {
public:
    static constexpr uint32_t dma_bytes    = 4096u;

    /* A ring of 2^ring_order data bytes, a reply slot of `reply_bytes` (whole
     * pages), and `staging_bytes` of the caller's memory at `staging`, which
     * must outlive this: the room a command is lent, header included. What
     * outgrows it, and an answer that outgrows the slot, is held in memory
     * from `memory`. */
    KmtTransport(GuestConnection &connection, BlockSource &memory, SubApi sub_api, uint8_t ring_order,
                 uint32_t reply_bytes, uint8_t *staging, size_t staging_bytes);
    /* One whose owner bounds every command and answer by the staging and the
     * slot - the guest suite's scenarios: past them a call fails, as it
     * would for no memory. */
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
    /* What the context's fence page says now, without a wait: whether the
     * host faulted the context - which an idle context learns only here, or
     * by its wake event - and why (HostWatch::fault_code). False and 0 from
     * a driver that maps no page. */
    bool                 host_faulted() const;
    uint32_t             host_fault_code() const;
    /* The highest fence whose batch the host GPU has finished, read from the
     * context's fence page (docs/protocol.md, "Fence page"): memory reused
     * after a batch is free once this passes the batch's fence. 0 before any,
     * and always 0 from a driver that maps no fence page into the process -
     * knows_gpu_progress() says which. */
    uint64_t gpu_completed() const;
    bool     knows_gpu_progress() const;
    /* The fence of the newest batch published; 0 before the first. The batch
     * the transport publishes next takes the one after it. */
    uint64_t published_fence() const;
    /* Returns once the host GPU has finished the batch of `fence`, which must
     * be published: invalid_parameter for one that is not. ok at once for 0
     * or a fence already finished. A driver with no fence page is waited on
     * for the whole context, which the kernel signals only once the GPU has
     * run it. A wait that fails, or a host that goes silent (HostWatch),
     * faults the context: invalid_state, as on one already faulted. */
    [[nodiscard]] Status await_gpu(uint64_t fence);
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
    /* From now on what is sent asynchronously is held in `capacity` bytes at
     * `block`, the caller's, which outlives this: one batch goes when the
     * block cannot take another command, when a command asks for an answer
     * (it is the batch's last) and at flush. A command an application made
     * one call at a time was a batch, a fence and a publish of the tail each.
     * A block too small for a command of the staging's room frames it in
     * pieces; a null block holds nothing. */
    void                 hold_in(uint8_t *block, size_t capacity);
    /* What is held, published now, answered by nothing. ok with nothing
     * held; failed on a faulted context, or when the batch could not be
     * published - refused, or longer than the ring - which faults it, since
     * what it held is lost. */
    [[nodiscard]] TransportResult flush();
    /* The bytes held, and the most the block holds; 0 for a transport that
     * holds nothing. */
    size_t                        held() const;
    size_t                        holding_capacity() const;
    /* Whole commands framed elsewhere, none asking for an answer - a
     * Direct3D command list's - held behind what is held, what is held going
     * first when they do not fit with it; published at once by a transport
     * that holds nothing. failed for more than the block holds, or as flush
     * fails. */
    [[nodiscard]] TransportResult hold_framed(const uint8_t *framed, size_t length);
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
     * device answers itself, behind what is held: ok when answered with
     * nothing; invalid_state when the call failed, which lost the context;
     * malformed when the answer held bytes, `answered` saying how many. The
     * call is ended before this returns. */
    [[nodiscard]] Status nop(uint32_t &answered);
    /* The publish barrier: what is held is sent, and the host has run it and
     * everything before it when this returns - before another process or
     * another API's context names an object those commands made, which it
     * reaches on a context of its own that nothing orders after this one.
     * A Direct3D 11 resource whose key DWM opens, an OpenGL object an OpenCL
     * context acquires. invalid_state when the context faulted. */
    [[nodiscard]] Status run_held();

    /* Lent the staging's room - in the held block when holding, the block
     * sent first when that room is not left in it - spilling into memory of
     * the transport's own past it. */
    WireWriter      request_writer() override;
    /* Held when holding; published otherwise. */
    TransportResult send_async(uint32_t command_id, WireWriter &writer) override;
    /* Published behind what is held, as one batch, and waited for. */
    TransportResult send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    /* Nothing is held for a caller: one thread owns this transport. A call
     * the host answered retires the blob leases of every batch up to the
     * one answered. */
    void            end_call() override;
    /* Room for a `bytes` lease in the blob arena: what is held is sent, and
     * the oldest batches the host has yet to run are waited for - not all of
     * them. false when even the host having run everything leaves too little,
     * or the context faulted. */
    [[nodiscard]] bool make_room_for_blobs(size_t bytes);
    /* `length` bytes of CommandHeader-framed commands, published behind what
     * is held as one batch: the whole batch is judged before anything is
     * written, and one that does not frame exactly is refused without
     * faulting the context. When any command asks for an answer the call
     * waits, and `reply` is the answer the device kept - the last such
     * command's; otherwise it is empty. An answered batch ends its call as a
     * synchronous command does. */
    TransportResult send_batch(const uint8_t *framed, size_t length, WireReader &reply);
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
    /* A command that spilled past the staging, a batch per piece. */
    [[nodiscard]] TransportResult publish_pieces(uint32_t command_id, const WireWriter &writer, uint8_t flags,
                                                 uint64_t &fence);
    /* The answer for `fence` as the slot holds it: ok, too_large with the
     * answer kept on the host, or a failure that faulted the context. */
    [[nodiscard]] Status          await_slot(uint64_t fence, WireReader &reply);
    /* The answer the host kept, `required` bytes as the slot said, fetched
     * in parts into answer_. failed without faulting when there is no memory
     * for it: the host keeps it only until the next command. */
    [[nodiscard]] TransportResult fetch_answer(WireReader &reply);
    /* One part of it, from `offset`. */
    [[nodiscard]] TransportResult fetch_part(uint64_t offset, WireReader &part);
    /* Waits for everything sent so far, and retires its fences. */
    [[nodiscard]] bool drain();
    void               retire_consumed();
    [[nodiscard]] bool room_for_fence();
    uint64_t           published_now() const;
    [[nodiscard]] TransportResult publish_batch(const uint8_t *batch, uint32_t length, uint64_t &fence);
    /* The command `writer` wrote, framed into the held block - in pieces
     * past it, the block sent each time it fills - and its leases sealed. */
    [[nodiscard]] TransportResult hold(uint32_t command_id, const WireWriter &writer, uint8_t flags);
    [[nodiscard]] TransportResult hold_pieces(uint32_t command_id, const WireWriter &writer, uint8_t flags);
    /* What is held, published as one batch whose last command asks for an
     * answer, and the answer awaited. */
    [[nodiscard]] TransportResult publish_held_answered(WireReader &reply);
    /* Waits for the context and points `reply` at the answer for `fence`. */
    [[nodiscard]] TransportResult await(uint64_t fence, WireReader &reply);
    bool                          replied_within_spin(uint64_t fence);
    bool                          consumed_within_spin();
    void                          spin_once() const;
    template <typename Arrived> bool arrives_eventually(Arrived arrived);
    [[nodiscard]] Status          wait_for_host(uint64_t fence);
    /* Whether the kernel can wait for `fence` alone: a queue's progress fence
     * can, and so can a context's wait for its newest batch; for an older one
     * the context's wait would wait for every batch after it too. */
    bool                          kernel_waits_for(uint64_t fence) const;
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
    /* The fence page, read-only, and whether the host still works on what is
     * waited for. */
    HostWatch        watch_;
    TransportFence   fences_;
    /* Where each outstanding fence's batch ends in the ring. */
    uint64_t         fence_ends_[TransportFence::max_outstanding];
    bool             faulted_;
    /* What the next batch written waits for; null when nothing is held. */
    PublishGate     *held_;
    /* The commands held for one batch; no block when nothing is held. */
    FramedBlock      pending_;
    /* A synchronous call was answered since the last end_call, and the
     * newest fence an answer came back for. */
    bool             answered_;
    uint64_t         answered_fence_;
    WireArena        decode_arena_;
    ClientBlobArena  blobs_;
    /* A command longer than the staging, while it is written and sent. */
    SpillBlock       spill_;
    /* An answer longer than the slot, until its call ends. */
    SpillBlock       answer_;

    friend struct KmtTransportProbe;
};

}  // namespace av

#endif  // AV_KMT_TRANSPORT_HPP
