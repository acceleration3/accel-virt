/* One sub-API context's transport to the host, over D3DKMT (docs/protocol.md;
 * feature 043). It is the ClientTransport the generated client is handed: it
 * owns the context's ring, reply slot and a small DMA buffer for the submission
 * descriptor, all allocated on a shared GuestConnection's device and kept
 * mapped for the context's life. request_writer lends room in the ring for one
 * command; send_* frames it as a batch, publishes it, rings it through
 * D3DKMTSubmitCommand on the virtual context, and - for a synchronous call -
 * waits for the context and reads the reply. The record it was built with is
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

#include <stddef.h>
#include <stdint.h>

namespace av {

class GuestConnection;
struct KmtTransportProbe;

class KmtTransport final : public ClientTransport {
public:
    /* The largest command this lends room for; a larger array crosses as a blob
     * (US2), not inline. */
    static constexpr size_t   max_command  = 1024u;
    static constexpr uint32_t reply_bytes  = 4096u;
    static constexpr uint32_t dma_bytes    = 4096u;

    KmtTransport(GuestConnection &connection, const RvkContextRecord &record);
    ~KmtTransport();

    /* Creates the ring, reply and DMA allocations, maps and locks them, and
     * creates the virtual context with the record as private data. invalid_state
     * once set up; the Kmt's own error otherwise, with nothing left open. */
    [[nodiscard]] Status initialize();
    void                 teardown();
    bool                 established() const;

    WireWriter      request_writer() override;
    TransportResult send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    WireArena      &decode_arena() override;
    ClientBlobArena &blobs() override;

    bool invariants_hold() const;

private:
    static constexpr uint32_t blob_count = 3u;   /* ring, reply, dma */

    [[nodiscard]] Status setup_allocation(uint64_t bytes, bool map_gpu, uint32_t &allocation, uint8_t *&view,
                                          uint64_t &address);
    [[nodiscard]] TransportResult publish(uint32_t command_id, WireWriter &writer, uint8_t flags,
                                          uint64_t &fence);

    GuestConnection &connection_;
    RvkContextRecord record_;
    uint32_t         allocations_[blob_count];   /* ring, reply, dma */
    uint64_t         dma_address_;
    uint32_t         context_;
    TransportRing    ring_;
    ReplyReader      reply_;
    TransportFence   fences_;
    uint8_t         *dma_view_;
    WireArena        decode_arena_;
    ClientBlobArena  blobs_;
    uint8_t          staging_[max_command];

    friend struct KmtTransportProbe;
};

}  // namespace av

#endif  // AV_KMT_TRANSPORT_HPP
