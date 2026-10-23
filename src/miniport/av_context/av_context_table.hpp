/* The adapter's av contexts (docs/protocol.md; feature 043): for each dxgkrnl
 * context a process created with an AvContextRecord, the binding that runs it
 * on the device, the doorbell slot it holds, the allocation its record names
 * it by, and the memory it runs on - guest RAM the driver owns, which VidMm
 * never moves (docs/protocol.md, "Context memory"). A process's
 * submission, the DPC's serve and two contexts' creation or teardown can meet,
 * so a slot is claimed, made live and given back under a spin lock, and the
 * device is waited on outside it. dxgkrnl holds the adapter's DDI lock only
 * shared around CreateContext, DestroyContext and Escape (dxgkrnl.sys
 * DXGADAPTER::AcquireDdiSync @140028080, level 1 = ExAcquireResourceSharedLite),
 * so every request sequence - and the request buffer and single control
 * exchange they share - runs under a mutex that keeps PASSIVE, which a stall
 * between the device's polls needs. A power transition or a reset, which abandon
 * every context, run under dxgkrnl's exclusive hold of the adapter and so never
 * meet a creation or a teardown. One per adapter. */
#ifndef AV_AV_CONTEXT_TABLE_HPP
#define AV_AV_CONTEXT_TABLE_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/platform/wait_mutex.hpp"
#include "miniport/av_context/chunk_ids.hpp"
#include "miniport/av_context/context_chunk.hpp"
#include "miniport/av_context/context_memory.hpp"
#include "miniport/av_context/release_queue.hpp"
#include "miniport/av_context/ring_submitter.hpp"
#include "miniport/av_context/av_context_binding.hpp"
#include "miniport/av_context/submission_reader.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "proto/resources/allocation_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class StateWriter;

class Allocation;
class AllocationTable;
class CapsetInfo;
class ContextViewsRecord;
class DxgkrnlInterface;
class ExecutionNode;
class HwQueueSubmissions;
class GuestPageList;
class HostWindow;
class HostWindowRecord;
class RegisterWindow;
class SubmissionDescriptor;
class AvContextRecord;
class VirtioGpuDevice;
struct AvContextTableProbe;

class AvContextTable {
public:
    /* One per doorbell page. */
    static constexpr uint32_t max_contexts = RingSubmitter::doorbell_pages;
    /* The most one adoption may lend: every page of it is a mapping of the
     * QEMU process at worst. */
    static constexpr uint64_t max_adopted_bytes = uint64_t{1} << 30u;

    /* `window` is the device's host window, which window chunks are placed in
     * and which must outlive the table. */
    AvContextTable(Platform &platform, DxgkrnlInterface &dxgkrnl, AllocationTable &allocations,
                    HostWindow &window);
    ~AvContextTable();
    AvContextTable(const AvContextTable &) = delete;
    AvContextTable &operator=(const AvContextTable &) = delete;

    /* PASSIVE, inside the CreateContext of the process that made the record's
     * allocation, whose handle only resolves there. Takes a free doorbell,
     * resolves the allocation that names the context, allocates the ring,
     * fence page and reply slot and maps the ring and reply slot into the
     * process, connects the doorbell's fence queue and sets the host context
     * up. invalid_state for a context already bound; no_space when no doorbell
     * is free or the device has none for the slot; invalid_parameter for a
     * handle naming no ring allocation of the process or one another context
     * is already named by, a share context that is not another live OpenGL
     * context of the process on the same device (named by its ring's handle,
     * which only resolves in the process that made it; feature 050), or
     * options the device's protocol cannot carry; no_memory when there is no
     * memory to run on or the process cannot be given a view of it; the
     * device's refusal otherwise. Nothing stays bound on failure. */
    [[nodiscard]] Status bind(void *context, const AvContextRecord &record, VirtioGpuDevice &device,
                              RegisterWindow &doorbells, const CapsetInfo &capset);
    /* PASSIVE. Tears the host context down on `device`, or lets go of it
     * without a request when there is no device or the device will not answer,
     * and takes its memory back - keeping it out of use for good when a device
     * that did not answer may still write it. invalid_parameter for a context
     * not bound. */
    [[nodiscard]] Status unbind(void *context, VirtioGpuDevice *device);
    /* PASSIVE, inside the Escape of the process that created `context`: the
     * addresses there of its ring and of its reply slot, in `record`.
     * invalid_parameter for a context not live. */
    [[nodiscard]] Status views(void *context, ContextViewsRecord &record);
    /* PASSIVE, inside the Escape of the process that created `context`: the
     * event `handle` names there is set whenever the context's host raises
     * (its fence page's `raised` moves), once when the context is lost, when
     * the device is reset, and when the context is unbound - so a thread
     * asleep on it always wakes to ask (docs/protocol.md, "Wake event
     * record"). The event is held until the context is unbound; a second
     * replaces the first. invalid_parameter for a context not live or a
     * handle that is no event the process may set. */
    [[nodiscard]] Status wake_on_raise(void *context, uint64_t handle);
    /* The shared object `object_id` is gone from the guest: the device is
     * told, in turn with this table's own sequences, because the control
     * queue answers one exchange at a time (SharedObjectRelease). PASSIVE. */
    [[nodiscard]] Status release_shared(uint64_t object_id, VirtioGpuDevice &device);
    /* DISPATCH. Publishes the batch the submission's private data describes and
     * rings its doorbell, answering in `awaited` the host fence the node waits
     * for. invalid_parameter for a context not live, one the host faulted or
     * whose fence page failed it, or bytes that are not a descriptor of its
     * ring; the binding's refusal otherwise. */
    [[nodiscard]] Status submit(void *context, const uint8_t *private_data, size_t size, uint64_t &awaited);
    /* DISPATCH. A batch its process wrote into the ring and releases by
     * packet (ReleaseRecord): published, with everything held behind it that
     * now follows, when every batch before it in the ring is; held until
     * then. `awaited` is the host fence the packet waits for. Once a context
     * releases this way its kernel and hardware-queue submissions take the
     * same turn (ReleaseQueue). invalid_parameter for a context not live, one
     * the host faulted, a batch longer than its ring or one overlapping what
     * was released, or when too many are held; the binding's refusal
     * otherwise. */
    [[nodiscard]] Status release_batch(void *context, const SubmissionDescriptor &batch, uint64_t &awaited);
    /* DISPATCH, from the DPC: reads every live context's fence page and
     * finishes on `node` what each one's host completed. A context the host
     * faulted, or whose page no device could write, has everything it queued
     * finished, so the node completes it in order and its process wakes to a
     * fence with no reply behind it (FR-006). dxgkrnl is not told of the
     * fault: DMA_FAULTED bugchecks a GPU MMU adapter (docs/kb/05-scheduling.md). */
    void                 serve(ExecutionNode &node, HwQueueSubmissions &queues);
    /* DISPATCH. A packet owned by `context` passed its deadline while batches
     * of it were still held for a predecessor that never came: the context is
     * failed, so its process learns what its fences cannot say. False when
     * nothing was held. */
    bool                 fault_if_held(void *context);
    /* DISPATCH. Whether the device has already finished `fence` on `context`
     * - so its interrupt may have come before the packet was queued - and
     * true for a context this table cannot answer for. */
    bool                 completed_already(void *context, uint64_t fence);
    /* DISPATCH. `context`'s device is told to look at its ring: what a packet
     * that follows it asks, because the batch it follows may have been
     * published by the process alone, with the host's worker asleep and the
     * kernel's packet that would wake it held back by the scheduler behind the
     * very packet waiting for it (FollowRecord). */
    void                 kick(void *context);
    /* PASSIVE. The device was reset or destroyed: no context survives on it,
     * so each is let go of and serves nothing until unbound. */
    void                 abandon_all();
    /* DISPATCH. The context `allocation` names, or null: a context whose name
     * goes, goes with it. */
    void                *user_of(const Allocation *allocation);
    /* PASSIVE, inside the Escape of the process that asks (docs/host-memory.md).
     * An open takes the first run of window pages that holds `record`'s bytes
     * rounded up to pages and a free host blob id, has the device make a host
     * blob of the memory `record` names for the context and place it there,
     * maps those pages into the calling process with the caching the device
     * answers, and answers the resource, the bytes granted and the address in
     * `record`. A close unmaps the process's view, has the device unmap and
     * release the blob, and only then frees the pages and the id. Nothing
     * falls back to guest RAM: invalid_parameter for a context not live or a
     * resource that is none of its chunks; no_space when every host blob id
     * is taken or the window has no run that long - the window's pages, not
     * a count of chunks, are what bounds them; not_supported with no window;
     * no_memory when the ids cannot be noted or the process cannot be given a
     * view; the device's refusal otherwise. Nothing is held on a failed open.
     *
     * An adoption locks the process's pages `record` names, takes a free host
     * blob id and has the device make a host blob of the memory `record`
     * names over them (docs/host-memory.md, "Application memory"), answering
     * the resource with the request's own bytes and address; its close has
     * the device release the blob and then lets the pages go. no_space for a
     * range over max_adopted_bytes, and the platform's refusal of the lock
     * besides the open's. */
    [[nodiscard]] Status window_chunk(void *context, HostWindowRecord &record, VirtioGpuDevice &device);
    uint32_t             bound() const;
    /* DISPATCH. Whether the table still holds `context`. Destroying an
     * allocation a context runs on unbinds it there and then, so a context
     * whose memory went first is already gone from here by the time dxgkrnl
     * destroys it - which is the order a process exit uses. */
    bool                 holds(const void *context) const;
    /* DISPATCH. The doorbell page `context` was bound to, or max_contexts for
     * a context that is not bound: what a doorbell of that context's hardware
     * queue names (feature 049). */
    uint32_t             doorbell_of(const void *context) const;
    /* DISPATCH. Which side publishes `context`'s tail and rings its doorbell
     * from now on (feature 049): user mode once its doorbell is connected, the
     * driver again once it is not. invalid_parameter for a context that is not
     * bound. */
    [[nodiscard]] Status user_mode_submits(const void *context, uint64_t *progress);
    [[nodiscard]] Status driver_submits(const void *context);
    /* DISPATCH. `context` stops writing completions into `progress`, a
     * hardware queue's that is going: true when it still did. */
    bool                 forget_progress(const void *context, const uint64_t *progress);

    /* One line per live av context and its bound chunks; `busy=true` and
     * nothing more while a submission or the DPC holds the slots. */
    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    static constexpr uint32_t no_slot = max_contexts;

    uint32_t             slot_of(const void *context) const;
    /* The host faulted the slot's context, or its fence page failed it. */
    bool                 lost(uint32_t slot) const;
    /* Sets the slot's wake when its raises moved or it was just lost. */
    void                 wake(uint32_t slot, uint64_t raised, bool lost_now);
    /* Claims a free slot for `context` under the lock; no_slot when full or
     * when `context` already holds one (in `duplicate`). */
    uint32_t             claim(void *context, bool &duplicate);
    /* The process's allocation `handle`, of `kind`, that no context names or
     * runs a chunk on. */
    [[nodiscard]] Status resolve(uint32_t handle, AllocationKind kind, Allocation *&allocation);
    /* The device id of the live OpenGL context whose ring `handle` names, other
     * than `slot`'s own: of this device for an OpenGL context asking, of this
     * process for an OpenCL one; 0 for a handle of 0. */
    [[nodiscard]] Status resolve_share(uint32_t slot, SubApi asking, uint32_t handle, uint32_t &context_id);
    [[nodiscard]] Status set_up(uint32_t slot, const AvContextRecord &record, VirtioGpuDevice &device,
                                RegisterWindow &doorbells, const CapsetInfo &capset);
    void                 release(uint32_t slot);
    /* `batch` in its turn on slot `slot`, under the lock (ReleaseQueue). */
    [[nodiscard]] Status in_turn(uint32_t slot, const SubmissionDescriptor &batch);
    /* The slot's order can never be whole: its context is failed. ok. */
    [[nodiscard]] Status lose(uint32_t slot);
    /* Releases every chunk the slot holds, on `device` when it answers.
     * `host_let_go` says a device that is not asked holds nothing of the
     * context any more, so its window pages are free again. */
    void                 release_chunks(uint32_t slot, VirtioGpuDevice *device, bool host_let_go);
    [[nodiscard]] Status open_window(uint32_t slot, HostWindowRecord &record, VirtioGpuDevice &device);
    [[nodiscard]] Status adopt_pages(uint32_t slot, HostWindowRecord &record, VirtioGpuDevice &device);
    /* The blob over the pages `locked` holds, under a free id. */
    [[nodiscard]] Status adopt_locked(uint32_t slot, void *locked, HostWindowRecord &record, VirtioGpuDevice &device);
    /* The chunk as a run of window pages under host blob id `resource`. */
    [[nodiscard]] Status place_in_window(uint32_t slot, uint32_t resource, HostWindowRecord &record,
                                         VirtioGpuDevice &device);
    [[nodiscard]] Status close_window(uint32_t slot, uint32_t resource_id, VirtioGpuDevice &device);
    /* Slot `slot`'s chunk of blob `resource_id`, or null. */
    ContextChunk        *find_chunk(uint32_t slot, uint32_t resource_id);
    /* Takes a chunk down: the process's view, then the blob on `device` when
     * there is one to ask, then the pages and the id - kept out of use for
     * good when the device may still hold the blob placed. */
    void                 take_down(ContextChunk &chunk, VirtioGpuDevice *device, bool host_let_go);
    void                 log_binding(uint32_t slot, uint32_t message, Status status);

    Platform                        &platform_;
    DxgkrnlInterface                &dxgkrnl_;
    AllocationTable                 &allocations_;
    HostWindow                      &window_;
    SpinLock                         lock_;
    SubmissionReader                 reader_;
    ObjectStorage<AvContextBinding> bindings_[max_contexts];
    ReleaseQueue                     releases_[max_contexts];
    void                            *contexts_[max_contexts];
    /* The allocation each slot's context is named by. */
    const Allocation                *names_[max_contexts];
    ObjectStorage<ContextMemory>     memories_[max_contexts];
    /* Every chunk every context holds, the ids they took, and how many each
     * slot holds. The pool and the ids change only inside a request
     * sequence; the counts, which a state dump reads, under the spin lock
     * too. */
    ObjectPool<ContextChunk>         chunks_;
    ChunkIds                         ids_;
    uint32_t                         held_chunks_[max_contexts];
    uint8_t                          orders_[max_contexts];
    /* The event each slot's process asked to be woken by, or null; the raises
     * its fence page had shown when it was last set; and whether it was set
     * for the context being lost, which happens once. */
    void                            *wakes_[max_contexts];
    uint64_t                         raised_[max_contexts];
    bool                             told_lost_[max_contexts];
    /* Bindings each slot has had, so each takes fresh device ids. */
    uint32_t                         generations_[max_contexts];
    bool                             live_[max_contexts];
    /* Lent to whichever binding is being set up or torn down: dxgkrnl creates
     * and destroys contexts one at a time under its adapter lock. */
    uint8_t                          request_[AvContextBinding::request_bytes];
    WaitMutex                        requests_;

    friend struct AvContextTableProbe;
};

}  // namespace av

#endif  // AV_AV_CONTEXT_TABLE_HPP
