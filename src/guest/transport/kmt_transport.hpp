/* One sub-API context's transport to the host, over D3DKMT (docs/protocol.md;
 * feature 043). It is the ClientTransport the generated client is handed: it
 * owns the context's ring, reply slot and a small DMA buffer for the submission
 * descriptor, all allocated on a shared GuestConnection's device and kept
 * mapped for the context's life. request_writer lends room for one command in
 * staging memory the caller owns, sized to its largest command - for an ICD far
 * more than a stack holds (feature 045); send_* frames it as a batch, publishes
 * it, rings it through D3DKMTSubmitCommand on the virtual context, and - for a
 * synchronous call - waits for the context and reads the reply. send_batch
 * publishes commands the caller framed itself. The record it was built with is
 * the private data the create-context carried; the miniport decoded it. */
#ifndef AV_KMT_TRANSPORT_HPP
#define AV_KMT_TRANSPORT_HPP

#include "core/support/status.hpp"
#include "guest/transport/reply_reader.hpp"
#include "guest/transport/transport_fence.hpp"
#include "guest/transport/transport_ring.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/client_transport.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GuestConnection;
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

    /* Creates the ring, fence, reply and DMA allocations, maps and locks them,
     * and creates the virtual context whose private data - the RvkContextRecord -
     * names the ring, fence and reply by the handles just allocated, for the
     * miniport to publish. invalid_state once set up; invalid_parameter, with
     * nothing asked of the Kmt, for staging with no room past a command header;
     * the Kmt's own error otherwise, with nothing left open. */
    [[nodiscard]] Status initialize();
    void                 teardown();
    bool                 established() const;
    /* A device-side failure (a refused submission, a faulted wait, a bad reply)
     * loses the context: every later send fails until it is torn down. */
    bool                 faulted() const;
    /* The virtual context's D3DKMT handle, for a call the driver takes beside
     * the transport - an escape on this context; 0 unless established. */
    uint32_t             context() const;

    /* A round trip of the transport's own no-op (feature 044), which the
     * device answers itself: ok when answered with nothing; invalid_state when
     * the call failed, which lost the context; malformed when the answer held
     * bytes, `answered` saying how many. */
    [[nodiscard]] Status nop(uint32_t &answered);

    WireWriter      request_writer() override;
    TransportResult send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    /* Nothing is held for a caller: one thread owns this transport. */
    void            end_call() override;
    /* `length` bytes of CommandHeader-framed commands, published as one batch:
     * the whole batch is judged before anything is written, and one that does
     * not frame exactly is refused without faulting the context. When any
     * command asks for an answer the call waits, and `reply` is the answer the
     * device kept - the last such command's; otherwise it is empty. */
    TransportResult send_batch(const uint8_t *framed, size_t length, WireReader &reply);
    WireArena      &decode_arena() override;
    ClientBlobArena &blobs() override;

    bool invariants_hold() const;

private:
    static constexpr uint32_t allocation_count = 4u;   /* ring, fence, reply, dma */
    static constexpr uint32_t ring_index  = 0u;
    static constexpr uint32_t fence_index = 1u;
    static constexpr uint32_t reply_index = 2u;
    static constexpr uint32_t dma_index   = 3u;

    [[nodiscard]] Status setup_allocation(uint32_t index, AllocationKind kind, uint64_t bytes, bool map_gpu,
                                          bool do_lock, uint8_t *&view, uint64_t &address);
    [[nodiscard]] TransportResult publish(uint32_t command_id, WireWriter &writer, uint8_t flags,
                                          uint64_t &fence);
    [[nodiscard]] TransportResult publish_batch(const uint8_t *batch, uint32_t length, uint64_t &fence);
    /* Waits for the context and points `reply` at the answer for `fence`. */
    [[nodiscard]] TransportResult await(uint64_t fence, WireReader &reply);

    GuestConnection &connection_;
    SubApi           sub_api_;
    uint8_t          ring_order_;
    uint32_t         reply_bytes_;
    uint8_t         *staging_;
    size_t           staging_bytes_;
    uint32_t         allocations_[allocation_count];   /* ring, fence, reply, dma */
    /* Which of them this process holds a view of: an unlock of one it never
     * locked is a call D3DKMT refuses. */
    bool             locked_[allocation_count];
    uint64_t         dma_address_;
    uint32_t         context_;
    TransportRing    ring_;
    ReplyReader      reply_;
    TransportFence   fences_;
    bool             faulted_;
    WireArena        decode_arena_;
    ClientBlobArena  blobs_;

    friend struct KmtTransportProbe;
};

}  // namespace av

#endif  // AV_KMT_TRANSPORT_HPP
