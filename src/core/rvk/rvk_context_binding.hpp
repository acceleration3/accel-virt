/* Binds one dxgkrnl virtual context to an rvk context on the device
 * (docs/protocol.md): it owns the GuestContext the context runs on and the
 * record it was asked for with, builds the context's setup from that record,
 * the device's capset and the resource ids the driver gave it, and drives the
 * setup and teardown request sequences over the device. Submission and the
 * fence-page serve are the DISPATCH service the adapter's submit path and DPC
 * call. One per dxgkrnl context. */
#ifndef AV_RVK_CONTEXT_BINDING_HPP
#define AV_RVK_CONTEXT_BINDING_HPP

#include "core/platform/platform.hpp"
#include "core/rvk/guest_context.hpp"
#include "core/support/status.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CapsetInfo;
class ContextPages;
class GuestPageList;
class RegisterWindow;
class SubmissionDescriptor;
class VirtioGpuDevice;
struct RvkContextBindingProbe;

class RvkContextBinding {
public:
    /* The ring, fence and reply blobs, in that order, then the host-visible
     * chunks a process binds (feature 045), per slot. */
    static constexpr uint32_t transport_resources   = 3u;
    static constexpr uint32_t chunk_slots           = 64u;
    static constexpr uint32_t resources_per_context = transport_resources + chunk_slots;
    /* Above the cursor's, every scanout's and host-visible memory's resource
     * ids, so a context's blob never takes one another subsystem uses. */
    static constexpr uint32_t first_resource_id = 0x00100000u;
    /* What a request must have room for: a blob over a scattered ring is as
     * wide as a primary's backing, and goes to the same wide control slot. */
    static constexpr size_t   request_bytes = GpuRequestWriter::max_backing_request_size;

    /* Slot `slot` of the adapter's contexts: doorbell `slot`, host context
     * `slot` + 1, and blobs from first_resource_id + resources_per_context * `slot`. The blobs are
     * named by ids the driver assigns, not by the record's allocation handles:
     * those are per process, and two processes' handles would collide on the
     * one device. `request` is `capacity` bytes the setup and teardown write
     * their requests into, lent by the owner, which serializes them: a request
     * buffer per context would be tens of kilobytes each. */
    RvkContextBinding(Platform &platform, RegisterWindow &doorbells, const RvkContextRecord &record, uint32_t slot,
                      uint8_t *request, size_t capacity);
    RvkContextBinding(const RvkContextBinding &) = delete;
    RvkContextBinding &operator=(const RvkContextBinding &) = delete;

    /* PASSIVE. Builds the setup from the record and the capset and drives the
     * sequence - CTX_CREATE, a blob over each of `pages`' lists and the setup -
     * to established. The pages must outlive the binding's use of them.
     * invalid_parameter for a record the capset forbids or pages not complete;
     * not_supported when the device refuses a step, which leaves the context failed. */
    [[nodiscard]] Status set_up(VirtioGpuDevice &device, const CapsetInfo &capset, const ContextPages &pages);
    /* PASSIVE. Drives CTX_DESTROY and a blob unref for each blob, then lets go
     * of the pages. invalid_state unless established or failed. */
    [[nodiscard]] Status tear_down(VirtioGpuDevice &device);
    /* PASSIVE. The device was reset or is gone: it holds nothing of this
     * context, so the pages are let go of without a request. */
    void                 abandon();
    /* DISPATCH. Hands the context a batch its ring submitter rings the doorbell
     * for. invalid_state unless established. */
    [[nodiscard]] Status submit(const SubmissionDescriptor &submission);
    /* DISPATCH, from the DPC. Reads the fence page and advances what completed. */
    [[nodiscard]] Status serve();

    /* The resource of the context's chunk slot `index`, below chunk_slots. */
    uint32_t             chunk_resource(uint32_t index) const;
    /* PASSIVE. A GUEST blob of `resource_id` over `pages`. ok when the device
     * made it, not_supported when it refused, malformed for an answer no device
     * sends. The request buffer is the owner's; it serializes. */
    [[nodiscard]] Status publish_chunk(VirtioGpuDevice &device, uint32_t resource_id, const GuestPageList &pages);
    /* PASSIVE. Drops the blob; the answer is the device's to give, and a chunk
     * whose release was not answered is let go of all the same. */
    void                 release_chunk(VirtioGpuDevice &device, uint32_t resource_id);

    GuestContext::State state() const;
    bool                established() const;
    uint32_t            context_id() const;
    uint64_t            completed() const;
    bool                faulted() const;

    bool invariants_hold() const;

private:
    void run_sequence(VirtioGpuDevice &device);

    static constexpr size_t answer_bytes  = 64u;

    GuestContext     context_;
    RvkContextRecord record_;
    uint32_t         context_id_;
    uint32_t         doorbell_;
    uint32_t         first_resource_id_;
    uint8_t         *request_;
    size_t           capacity_;
    uint8_t          answer_[answer_bytes];

    friend struct RvkContextBindingProbe;
};

}  // namespace av

#endif  // AV_RVK_CONTEXT_BINDING_HPP
