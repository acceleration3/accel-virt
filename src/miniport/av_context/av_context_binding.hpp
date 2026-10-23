/* Binds one dxgkrnl virtual context to an av context on the device
 * (docs/protocol.md): it owns the GuestContext the context runs on and the
 * record it was asked for with, builds the context's setup from that record,
 * the device's capset and the resource ids the driver gave it, and drives the
 * setup and teardown request sequences over the device. Submission and the
 * fence-page serve are the DISPATCH service the adapter's submit path and DPC
 * call. One per dxgkrnl context. */
#ifndef AV_AV_CONTEXT_BINDING_HPP
#define AV_AV_CONTEXT_BINDING_HPP

#include "core/platform/platform.hpp"
#include "miniport/av_context/guest_context.hpp"
#include "core/support/status.hpp"
#include "proto/escape/av_context_record.hpp"
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
struct AvContextBindingProbe;

class AvContextBinding {
public:
    /* The ring, fence and reply blobs, in that order (docs/protocol.md,
     * Context setup). A process's chunks take host blob ids of their own
     * (HostBlobId). */
    static constexpr uint32_t transport_resources   = 3u;
    static constexpr uint32_t resources_per_context = transport_resources;
    /* Above the cursor's and every scanout's resource ids and below the host
     * blobs' (HostBlobId), so a context's blob never takes one another
     * subsystem uses. */
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
    /* `generation` counts the slot's earlier bindings: a teardown the device
     * never finished leaves that binding's context and blobs behind, and the
     * next binding of the slot takes ids of its own rather than colliding
     * with them for good. */
    AvContextBinding(Platform &platform, RegisterWindow &doorbells, const AvContextRecord &record, uint32_t slot,
                      uint32_t generation, uint8_t *request, size_t capacity);
    static constexpr uint32_t generations = 256u;
    AvContextBinding(const AvContextBinding &) = delete;
    AvContextBinding &operator=(const AvContextBinding &) = delete;

    /* PASSIVE. Builds the setup from the record, whose share context is
     * already the device's id for it, at the version the capset and this
     * driver both speak, and drives the sequence - CTX_CREATE, a blob over each
     * of `pages`' lists and the setup - to established. The pages must outlive
     * the binding's use of them. invalid_parameter, with nothing sent, for
     * options that version cannot carry; for a record the capset forbids or
     * pages not complete;
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
    void                 kick();
    /* DISPATCH. See GuestContext::fail and next_begin. */
    void                 fail();
    uint64_t             next_begin() const;
    /* DISPATCH. Which side publishes this context's tail and rings its
     * doorbell, once a process has one of its own (feature 049). */
    void                 user_mode_submits(uint64_t *progress);
    void                 driver_submits();
    bool                 forget_progress(const uint64_t *progress);
    /* DISPATCH, from the DPC. Reads the fence page and advances what completed. */
    [[nodiscard]] Status serve();

    GuestContext::State state() const;
    bool                established() const;
    uint32_t            context_id() const;
    SubApi              sub_api() const;
    uint64_t            completed() const;
    uint64_t            raised() const;
    bool                faulted() const;

    bool invariants_hold() const;

private:
    void run_sequence(VirtioGpuDevice &device);

    static constexpr size_t answer_bytes  = 64u;

    GuestContext     context_;
    AvContextRecord record_;
    uint32_t         context_id_;
    uint32_t         doorbell_;
    uint32_t         first_resource_id_;
    uint8_t         *request_;
    size_t           capacity_;
    uint8_t          answer_[answer_bytes];

    friend struct AvContextBindingProbe;
};

}  // namespace av

#endif  // AV_AV_CONTEXT_BINDING_HPP
