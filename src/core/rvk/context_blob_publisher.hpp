/* Turns a context's ring, fence and reply aperture allocations into the guest
 * blobs the device names them by (docs/protocol.md). Each allocation is
 * scattered pages VidMm handed the driver; the publisher creates one
 * RESOURCE_CREATE_BLOB GUEST over those pages and assigns a resource id, so a
 * ContextSetup can name the three. It holds the ids per context until the
 * binding tears down: a blob the device holds may not be unref'd before
 * CTX_DESTROY. One per adapter. PASSIVE. */
#ifndef AV_CONTEXT_BLOB_PUBLISHER_HPP
#define AV_CONTEXT_BLOB_PUBLISHER_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GuestPageList;
class VirtioGpuDevice;
struct ContextBlobPublisherProbe;

class ContextBlobPublisher {
public:
    /* One per fence queue, the wire's reserved range (RingSubmitter::doorbell_pages). */
    static constexpr uint32_t max_contexts      = 64u;
    static constexpr uint32_t blobs_per_context = 3u;
    /* Above the cursor's, every scanout's, and host-visible memory's ranges, so
     * a context blob can never take a resource another subsystem is using. */
    static constexpr uint32_t first_resource_id = 0x00100000u;

    explicit ContextBlobPublisher(Platform &platform);
    ContextBlobPublisher(const ContextBlobPublisher &) = delete;
    ContextBlobPublisher &operator=(const ContextBlobPublisher &) = delete;

    /* Publishes `count` (1..blobs_per_context) blobs for `context_id`, filling
     * `ids` with that many distinct non-zero resource ids. A list that is not
     * complete - an allocation not resident, or not the whole blob - is refused
     * before anything is created; a create the device refuses unrefs the blobs
     * already made. invalid_state if the context is already published;
     * no_space when the table is full. The device is a parameter, not a member:
     * it is destroyed and rebuilt across a power transition. */
    [[nodiscard]] Status publish(VirtioGpuDevice &device, uint32_t context_id, const GuestPageList *blobs,
                                 uint32_t count, uint32_t *ids);
    /* Unrefs the context's blobs and forgets them, keeping on past a device
     * that refuses one - a teardown releases what setup created. invalid_state
     * for a context it does not hold. */
    [[nodiscard]] Status retire(VirtioGpuDevice &device, uint32_t context_id);
    bool                 holds(uint32_t context_id) const;

    bool invariants_hold() const;

private:
    /* A create-blob for a scattered allocation is the widest request this
     * sends, so it uses the control queue's wide slot, as a primary's backing
     * does; sized to the same bound the queue accepts. */
    static constexpr size_t request_bytes = GpuRequestWriter::max_backing_request_size;
    static constexpr size_t answer_bytes  = 64u;
    static constexpr uint32_t no_slot     = max_contexts;

    uint32_t             slot_of(uint32_t context_id) const;
    uint32_t             free_slot() const;
    [[nodiscard]] Status create_blob(VirtioGpuDevice &device, uint32_t resource_id, const GuestPageList &pages);
    [[nodiscard]] Status unref(VirtioGpuDevice &device, uint32_t resource_id);
    [[nodiscard]] Status exchange(VirtioGpuDevice &device, size_t length);

    Platform &platform_;
    uint32_t  next_id_;
    bool      used_[max_contexts];
    uint32_t  context_ids_[max_contexts];
    uint32_t  ids_[max_contexts][blobs_per_context];
    uint32_t  counts_[max_contexts];
    uint8_t   request_[request_bytes];
    uint8_t   answer_[answer_bytes];

    friend struct ContextBlobPublisherProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_BLOB_PUBLISHER_HPP
