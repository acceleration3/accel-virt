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
class RegisterWindow;
class SubmissionDescriptor;
class VirtioGpuDevice;
struct RvkContextBindingProbe;

class RvkContextBinding {
public:
    /* The ring, fence and reply blobs, in that order, from `first_resource_id`. */
    static constexpr uint32_t resources_per_context = 3u;

    /* The blobs are named by ids the driver assigns, not by the record's
     * allocation handles: those are per process, and two processes' handles
     * would collide on the one device. */
    RvkContextBinding(Platform &platform, RegisterWindow &doorbells, const RvkContextRecord &record,
                      uint32_t context_id, uint32_t doorbell, uint32_t first_resource_id);
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
    /* DISPATCH. Hands the context a batch its ring submitter rings the doorbell
     * for. invalid_state unless established. */
    [[nodiscard]] Status submit(const SubmissionDescriptor &submission);
    /* DISPATCH, from the DPC. Reads the fence page and advances what completed. */
    [[nodiscard]] Status serve();

    GuestContext::State state() const;
    bool                established() const;
    uint32_t            context_id() const;
    uint64_t            completed() const;
    bool                faulted() const;
    /* The fence page the device writes and the serve reads; the route hands the
     * ISR nothing else about a context. */
    const uint8_t      *fence_page() const;

    bool invariants_hold() const;

private:
    void run_sequence(VirtioGpuDevice &device);

    /* A blob over a scattered ring is as wide as a primary's backing, and goes
     * to the same wide control slot. */
    static constexpr size_t request_bytes = GpuRequestWriter::max_backing_request_size;
    static constexpr size_t answer_bytes  = 64u;

    GuestContext     context_;
    RvkContextRecord record_;
    uint32_t         context_id_;
    uint32_t         doorbell_;
    uint32_t         first_resource_id_;
    uint8_t          request_[request_bytes];
    uint8_t          answer_[answer_bytes];

    friend struct RvkContextBindingProbe;
};

}  // namespace av

#endif  // AV_RVK_CONTEXT_BINDING_HPP
