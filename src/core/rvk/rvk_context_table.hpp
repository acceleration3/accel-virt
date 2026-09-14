/* The adapter's rvk contexts (docs/protocol.md; feature 043): for each dxgkrnl
 * context a process created with an RvkContextRecord, the binding that runs it
 * on the device, the doorbell slot it holds, the allocations its record named
 * and the kernel's own views of their fence page and ring header. A process's
 * submission, the DPC's serve and two contexts' creation or teardown can meet,
 * so a slot is claimed, made live and given back under a spin lock, and the
 * device is waited on outside it. A power transition or a reset, which abandon
 * every context, run under dxgkrnl's exclusive hold of the adapter and so never
 * meet a creation or a teardown. One per adapter. */
#ifndef AV_RVK_CONTEXT_TABLE_HPP
#define AV_RVK_CONTEXT_TABLE_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/rvk/ring_submitter.hpp"
#include "core/rvk/rvk_context_binding.hpp"
#include "core/rvk/submission_reader.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Allocation;
class AllocationTable;
class CapsetInfo;
class ContextLoss;
class DxgkrnlInterface;
class ExecutionNode;
class GuestPageList;
class RegisterWindow;
class RvkContextRecord;
class VirtioGpuDevice;
struct RvkContextTableProbe;

class RvkContextTable {
public:
    /* One per doorbell page. */
    static constexpr uint32_t max_contexts = RingSubmitter::doorbell_pages;

    RvkContextTable(Platform &platform, DxgkrnlInterface &dxgkrnl, AllocationTable &allocations);
    ~RvkContextTable();
    RvkContextTable(const RvkContextTable &) = delete;
    RvkContextTable &operator=(const RvkContextTable &) = delete;

    /* PASSIVE, inside the CreateContext of the process that made the record's
     * allocations, whose handles only resolve there. Takes a free doorbell,
     * resolves the ring, fence page and reply slot, maps the two pages the
     * driver reads, connects the doorbell's fence queue and sets the host
     * context up. invalid_state for a context already bound; no_space when no
     * doorbell is free or the device has none for the slot; invalid_parameter
     * for a handle naming no allocation of its role, one another context
     * already runs on, a ring too small for its order, or pages VidMm has not
     * all mapped; no_memory when a page cannot be
     * mapped; the device's refusal otherwise. Nothing stays bound on failure. */
    [[nodiscard]] Status bind(void *context, const RvkContextRecord &record, VirtioGpuDevice &device,
                              RegisterWindow &doorbells, const CapsetInfo &capset);
    /* PASSIVE. Tears the host context down on `device`, or lets go of it
     * without a request when there is no device or the device will not answer,
     * and unmaps its pages. invalid_parameter for a context not bound. */
    [[nodiscard]] Status unbind(void *context, VirtioGpuDevice *device);
    /* DISPATCH. Publishes the batch the submission's private data describes and
     * rings its doorbell, answering in `awaited` the host fence the node waits
     * for. invalid_parameter for a context not live or bytes that are not a
     * descriptor of its ring; the binding's refusal otherwise. */
    [[nodiscard]] Status submit(void *context, const uint8_t *private_data, size_t size, uint64_t &awaited);
    /* DISPATCH, from the DPC: reads every live context's fence page and
     * finishes on `node` what each one's host completed. A context the host
     * faulted, or whose page no device could write, finishes nothing more:
     * what it still has queued is dropped and its loss recorded in `losses`
     * against the first of it, for the DPC to tell dxgkrnl (FR-006). */
    void                 serve(ExecutionNode &node, ContextLoss &losses);
    /* PASSIVE. The device was reset or destroyed: no context survives on it,
     * so each is let go of and serves nothing until unbound. */
    void                 abandon_all();
    /* DISPATCH. The context whose record named `allocation`, or null: an
     * allocation a context runs on may not be freed under it. */
    void                *user_of(const Allocation *allocation);
    uint32_t             bound() const;

    bool invariants_hold() const;

private:
    enum : uint32_t { ring_role = 0u, fence_role = 1u, reply_role = 2u, roles = 3u };
    static constexpr uint32_t no_slot = max_contexts;

    uint32_t             slot_of(const void *context) const;
    /* Claims a free slot for `context` under the lock; no_slot when full or
     * when `context` already holds one (in `duplicate`). */
    uint32_t             claim(void *context, bool &duplicate);
    [[nodiscard]] Status resolve(uint32_t handle, uint32_t role, uint8_t order, Allocation *&allocation);
    [[nodiscard]] Status set_up(uint32_t slot, const RvkContextRecord &record, VirtioGpuDevice &device,
                                RegisterWindow &doorbells, const CapsetInfo &capset);
    void                 release(uint32_t slot);
    void                 log_binding(uint32_t slot, uint32_t message, Status status);

    Platform                        &platform_;
    DxgkrnlInterface                &dxgkrnl_;
    AllocationTable                 &allocations_;
    SpinLock                         lock_;
    SubmissionReader                 reader_;
    ObjectStorage<RvkContextBinding> bindings_[max_contexts];
    void                            *contexts_[max_contexts];
    const Allocation                *uses_[max_contexts][roles];
    uint8_t                         *ring_views_[max_contexts];
    uint8_t                         *fence_views_[max_contexts];
    uint8_t                          orders_[max_contexts];
    bool                             live_[max_contexts];
    /* Lent to whichever binding is being set up or torn down: dxgkrnl creates
     * and destroys contexts one at a time under its adapter lock. */
    uint8_t                          request_[RvkContextBinding::request_bytes];

    friend struct RvkContextTableProbe;
};

}  // namespace av

#endif  // AV_RVK_CONTEXT_TABLE_HPP
