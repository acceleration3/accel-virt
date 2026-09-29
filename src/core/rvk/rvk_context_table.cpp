#include "core/rvk/rvk_context_table.hpp"

#include "core/scheduling/hw_queue_submissions.hpp"

#include "core/memory/allocation.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/host_visible_blob.hpp"
#include "core/memory/host_window.hpp"
#include "core/miniport/acquired_handle_data.hpp"
#include "core/platform/register_window.hpp"
#include "core/platform/spin_lock_guard.hpp"
#include "core/platform/wait_mutex_guard.hpp"
#include "core/rvk/context_pages.hpp"
#include "core/scheduling/execution_node.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/platform/spin_lock_try_guard.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "core/memory/shared_object_release.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/host_visible_bind_record.hpp"
#include "proto/resources/host_window_record.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

namespace av {

RvkContextTable::RvkContextTable(Platform &platform, DxgkrnlInterface &dxgkrnl, AllocationTable &allocations,
                                 HostWindow &window)
    : platform_(platform), dxgkrnl_(dxgkrnl), allocations_(allocations), window_(window),
      lock_(platform, LockRank::rvk_context_slots),
      reader_(platform), bindings_(), contexts_{}, uses_{}, chunks_(), ring_views_{}, fence_views_{}, orders_{}, generations_{},
      live_{}, request_{}, requests_(platform, LockRank::rvk_control_requests)
      {
          AV_SCOPE("RvkContextTable::RvkContextTable");
      }

RvkContextTable::~RvkContextTable()
{
    AV_SCOPE("RvkContextTable::~RvkContextTable");
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        if (contexts_[slot] == nullptr) continue;
        release_chunks(slot, nullptr, false);
        release(slot);
    }
}

uint32_t RvkContextTable::slot_of(const void *context) const
{
    AV_SCOPE("RvkContextTable::slot_of");
    for (uint32_t slot = 0u; slot < max_contexts; slot++)
        if (context != nullptr && contexts_[slot] == context) return slot;
    return no_slot;
}

uint32_t RvkContextTable::claim(void *context, bool &duplicate)
{
    AV_SCOPE("RvkContextTable::claim");
    SpinLockGuard locked(lock_);
    duplicate = slot_of(context) != no_slot;
    if (duplicate) return no_slot;
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        if (contexts_[slot] != nullptr) continue;
        contexts_[slot] = context;
        return slot;
    }
    return no_slot;
}

/* The handle is the process's, so it is resolved through dxgkrnl and borrowed
 * only for the lookup; what outlives it is the driver's own Allocation, whose
 * freeing the adapter asks this table about first. */
Status RvkContextTable::resolve(uint32_t handle, uint32_t role, uint8_t order, Allocation *&allocation)
{
    AV_SCOPE("RvkContextTable::resolve");
    static constexpr AllocationKind kinds[chunk_role + 1u] = {AllocationKind::context_ring, AllocationKind::fence_page,
                                                              AllocationKind::reply_slot,
                                                              AllocationKind::host_visible_memory};
    allocation = nullptr;
    AcquiredHandleData acquired(dxgkrnl_, handle);
    if (acquired.acquire() != Status::ok) return Status::invalid_parameter;
    Allocation *const found = allocations_.find(acquired.data());
    if (found == nullptr || found->descriptor().kind() != kinds[role]) return Status::invalid_parameter;
    if (role == ring_role && found->size() < RingHeader::encoded_size + (uint64_t{1} << order))
        return Status::invalid_parameter;
    if (!found->pages().complete() || user_of(found) != nullptr) return Status::invalid_parameter;
    allocation = found;
    return Status::ok;
}

Status RvkContextTable::resolve_share(uint32_t slot, SubApi asking, uint32_t handle, uint32_t &context_id)
{
    AV_SCOPE("RvkContextTable::resolve_share");
    context_id = 0u;
    if (handle == 0u) return Status::ok;
    AcquiredHandleData acquired(dxgkrnl_, handle);
    if (acquired.acquire() != Status::ok) return Status::invalid_parameter;
    const Allocation *const ring = allocations_.find(acquired.data());
    SpinLockGuard           locked(lock_);
    for (uint32_t other = 0u; other < max_contexts; other++) {
        /* A slot with no context names no ring, so a handle that is no allocation matches nothing. */
        if (other == slot || uses_[other][ring_role] != ring || !live_[other]) continue;
        const RvkContextBinding &binding = bindings_[other].object();
        /* An OpenGL context shares within its own device; an OpenCL one comes
         * from another ICD, on another device of the same process. */
        const Allocation &own = *uses_[slot][ring_role];
        const bool same_owner = asking == SubApi::opengl ? ring->held_with(own) : ring->held_in_process_of(own);
        if (binding.sub_api() != SubApi::opengl || !same_owner) return Status::invalid_parameter;
        context_id = binding.context_id();
        return Status::ok;
    }
    return Status::invalid_parameter;
}

Status RvkContextTable::bind(void *context, const RvkContextRecord &record, VirtioGpuDevice &device,
                             RegisterWindow &doorbells, const CapsetInfo &capset)
{
    AV_SCOPE("RvkContextTable::bind");
    platform_.require_irql_at_most(Irql::passive, "RvkContextTable::bind");
    WaitMutexGuard                  sequence(requests_);
    InvariantScope<RvkContextTable> guard(platform_, *this, "bind");
    if (context == nullptr) return Status::invalid_state;
    bool           duplicate = false;
    const uint32_t slot = claim(context, duplicate);
    if (duplicate) return Status::invalid_state;
    if (slot == no_slot) return Status::no_space;
    ScopeExit unwind([this, slot] { release(slot); });
    if (doorbells.length() < (uint64_t{slot} + 1u) * RingSubmitter::doorbell_page_bytes) return Status::no_space;
    const uint32_t handles[roles] = {record.ring(), record.fence(), record.reply()};
    Allocation    *resolved[roles] = {};
    for (uint32_t role = 0u; role < roles; role++) {
        const Status status = resolve(handles[role], role, record.ring_order(), resolved[role]);
        if (status != Status::ok) return status;
    }
    {
        SpinLockGuard locked(lock_);
        for (uint32_t role = 0u; role < roles; role++) uses_[slot][role] = resolved[role];
        orders_[slot] = record.ring_order();
    }
    uint32_t     share = 0u;
    const Status shared =
        resolve_share(slot, record.sub_api(), record.options().share_context(), share);
    if (shared != Status::ok) return shared;
    const RvkContextRecord sharing(record.sub_api(), record.ring_order(), record.ring(), record.fence(),
                                   record.reply(), record.options().sharing(share));
    const Status status = set_up(slot, sharing, device, doorbells, capset);
    if (status != Status::ok) return status;
    unwind.dismiss();
    SpinLockGuard locked(lock_);
    live_[slot] = true;
    return Status::ok;
}

Status RvkContextTable::set_up(uint32_t slot, const RvkContextRecord &record, VirtioGpuDevice &device,
                               RegisterWindow &doorbells, const CapsetInfo &capset)
{
    AV_SCOPE("RvkContextTable::set_up");
    const GuestPageList ring = uses_[slot][ring_role]->pages();
    const GuestPageList fence = uses_[slot][fence_role]->pages();
    ring_views_[slot] = platform_.map_frame(ring.frame(0u));
    fence_views_[slot] = platform_.map_frame(fence.frame(0u));
    if (ring_views_[slot] == nullptr || fence_views_[slot] == nullptr) return Status::no_memory;
    uint32_t message = 0u;
    Status   status = device.connect_fence_queue(slot, message);
    if (status == Status::ok)
        status = bindings_[slot].construct(platform_, doorbells, record, slot, generations_[slot]++, request_,
                                           sizeof request_);
    if (status == Status::ok)
        status = bindings_[slot].object().set_up(
            device, capset,
            ContextPages(ring, fence, uses_[slot][reply_role]->pages(), ring_views_[slot], fence_views_[slot]));
    log_binding(slot, message, status);
    if (status != Status::ok && bindings_[slot].constructed() &&
        bindings_[slot].object().tear_down(device) != Status::ok)
        bindings_[slot].object().abandon();
    return status;
}

Status RvkContextTable::unbind(void *context, VirtioGpuDevice *device)
{
    AV_SCOPE("RvkContextTable::unbind");
    platform_.require_irql_at_most(Irql::passive, "RvkContextTable::unbind");
    WaitMutexGuard                  sequence(requests_);
    InvariantScope<RvkContextTable> guard(platform_, *this, "unbind");
    uint32_t slot = no_slot;
    {
        SpinLockGuard locked(lock_);
        slot = slot_of(context);
        if (slot == no_slot) return Status::invalid_parameter;
        live_[slot] = false;
    }
    RvkContextBinding &binding = bindings_[slot].object();
    const bool held = binding.state() == GuestContext::State::established ||
                      binding.state() == GuestContext::State::failed;
    /* The context before its chunks: the device keeps a chunk it holds imported
     * and refuses to release it, and destroying the context is what ends every
     * import it made. */
    const bool torn_down = held && device != nullptr && binding.tear_down(*device) == Status::ok;
    /* A device reset or gone holds nothing of the context, window chunks
     * included; one that stopped answering mid-teardown may still hold them
     * placed, and their pages are kept out of the window for good. */
    release_chunks(slot, torn_down ? device : nullptr, torn_down || !held || device == nullptr);
    if (!torn_down) binding.abandon();
    release(slot);
    return Status::ok;
}

/* Everything a slot holds, in the reverse of how bind took it; the slot is
 * given back last, so nobody claims it while its pages are still mapped. */
void RvkContextTable::release(uint32_t slot)
{
    AV_SCOPE("RvkContextTable::release");
    bindings_[slot].destroy();
    if (fence_views_[slot] != nullptr) platform_.unmap_frame(fence_views_[slot]);
    if (ring_views_[slot] != nullptr) platform_.unmap_frame(ring_views_[slot]);
    fence_views_[slot] = nullptr;
    ring_views_[slot] = nullptr;
    SpinLockGuard locked(lock_);
    for (const Allocation *&use : uses_[slot]) use = nullptr;
    for (ContextChunk &chunk : chunks_[slot]) chunk.clear();
    orders_[slot] = 0u;
    contexts_[slot] = nullptr;
}

void RvkContextTable::release_chunks(uint32_t slot, VirtioGpuDevice *device, bool host_let_go)
{
    AV_SCOPE("RvkContextTable::release_chunks");
    for (ContextChunk &chunk : chunks_[slot]) {
        if (!chunk.bound()) continue;
        if (chunk.in_window()) {
            take_down(chunk, device, host_let_go);
            continue;
        }
        if (device != nullptr) bindings_[slot].object().release_chunk(*device, chunk.resource_id());
        SpinLockGuard locked(lock_);
        chunk.clear();
    }
}

Status RvkContextTable::bind_chunk(void *context, HostVisibleBindRecord &record, VirtioGpuDevice &device)
{
    AV_SCOPE("RvkContextTable::bind_chunk");
    platform_.require_irql_at_most(Irql::passive, "RvkContextTable::bind_chunk");
    WaitMutexGuard                  sequence(requests_);
    InvariantScope<RvkContextTable> guard(platform_, *this, "bind_chunk");
    uint32_t                        slot = no_slot;
    {
        SpinLockGuard locked(lock_);
        slot = slot_of(context);
        if (slot == no_slot || !live_[slot] || lost(slot)) return Status::invalid_parameter;
    }
    if (!record.binds()) return unpublish_chunk(slot, record.resource_id(), device);
    return publish_chunk(slot, record, device);
}

Status RvkContextTable::publish_chunk(uint32_t slot, HostVisibleBindRecord &record, VirtioGpuDevice &device)
{
    AV_SCOPE("RvkContextTable::publish_chunk");
    Allocation  *found = nullptr;
    const Status resolved = resolve(record.allocation(), chunk_role, 0u, found);
    if (resolved != Status::ok) return resolved;
    const uint32_t index = free_chunk(slot);
    if (index == RvkContextBinding::chunk_slots) return Status::no_space;
    RvkContextBinding &binding = bindings_[slot].object();
    const uint32_t     resource = binding.chunk_resource(index);
    const Status       published = binding.publish_chunk(device, resource, found->pages());
    if (published != Status::ok) return published;
    {
        SpinLockGuard locked(lock_);
        chunks_[slot][index].take_guest(*found, resource);
    }
    record = record.answered(resource, found->size() / GuestPageList::page_bytes);
    return Status::ok;
}

Status RvkContextTable::unpublish_chunk(uint32_t slot, uint32_t resource_id, VirtioGpuDevice &device)
{
    AV_SCOPE("RvkContextTable::unpublish_chunk");
    for (ContextChunk &chunk : chunks_[slot]) {
        if (!chunk.bound() || chunk.in_window() || chunk.resource_id() != resource_id) continue;
        bindings_[slot].object().release_chunk(device, resource_id);
        SpinLockGuard locked(lock_);
        chunk.clear();
        return Status::ok;
    }
    return Status::invalid_parameter;
}

uint32_t RvkContextTable::free_chunk(uint32_t slot) const
{
    AV_SCOPE("RvkContextTable::free_chunk");
    uint32_t index = 0u;
    while (index < RvkContextBinding::chunk_slots && chunks_[slot][index].bound()) index++;
    return index;
}

Status RvkContextTable::release_shared(uint64_t object_id, VirtioGpuDevice &device)
{
    AV_SCOPE("RvkContextTable::release_shared");
    platform_.require_irql_at_most(Irql::passive, "RvkContextTable::release_shared");
    WaitMutexGuard      sequence(requests_);
    SharedObjectRelease release(platform_);
    return release.send(device, object_id);
}

Status RvkContextTable::window_chunk(void *context, HostWindowRecord &record, VirtioGpuDevice &device)
{
    AV_SCOPE("RvkContextTable::window_chunk");
    platform_.require_irql_at_most(Irql::passive, "RvkContextTable::window_chunk");
    WaitMutexGuard                  sequence(requests_);
    InvariantScope<RvkContextTable> guard(platform_, *this, "window_chunk");
    uint32_t                        slot = no_slot;
    {
        SpinLockGuard locked(lock_);
        slot = slot_of(context);
        if (slot == no_slot || !live_[slot] || lost(slot)) return Status::invalid_parameter;
    }
    if (!record.opens()) return close_window(slot, record.resource_id(), device);
    return open_window(slot, record, device);
}

/* Each step is undone in reverse if a later one fails. The pages go back only
 * once the device holds no blob over them: a blob it would not release may
 * still be placed there, and the next chunk must not land on it. */
Status RvkContextTable::open_window(uint32_t slot, HostWindowRecord &record, VirtioGpuDevice &device)
{
    AV_SCOPE("RvkContextTable::open_window");
    const uint64_t wanted_pages = record.bytes() / HostWindow::page_bytes +
                                  (record.bytes() % HostWindow::page_bytes != 0u ? 1u : 0u);
    if (wanted_pages > UINT32_MAX) return Status::no_space;
    const auto     pages = static_cast<uint32_t>(wanted_pages);
    const uint64_t bytes = wanted_pages * HostWindow::page_bytes;
    const uint32_t index = free_chunk(slot);
    if (index == RvkContextBinding::chunk_slots) return Status::no_space;
    uint32_t     first = 0u;
    const Status taken = window_.take(pages, first);
    if (taken != Status::ok) return taken;
    const uint32_t  resource = bindings_[slot].object().chunk_resource(index);
    HostVisibleBlob blob(platform_);
    /* What destroy answers is read back from the blob: still created is a
     * device that did not let go of it. */
    ScopeExit       unwind([this, &blob, &device, first, pages] {
        if (blob.created()) static_cast<void>(blob.destroy(device));
        if (!blob.created()) window_.give_back(first, pages);
    });
    uint64_t view = 0u;
    void    *mapping = nullptr;
    Status   status = blob.create(device, resource, bytes);
    if (status == Status::ok) status = blob.map(device, uint64_t{first} * HostWindow::page_bytes);
    if (status == Status::ok) status = platform_.map_to_process(window_.physical(first), bytes, view, mapping);
    if (status != Status::ok) return status;
    unwind.dismiss();
    {
        SpinLockGuard locked(lock_);
        chunks_[slot][index].take_window(resource, first, pages, mapping);
    }
    record = record.answered(resource, bytes, view);
    return Status::ok;
}

Status RvkContextTable::close_window(uint32_t slot, uint32_t resource_id, VirtioGpuDevice &device)
{
    AV_SCOPE("RvkContextTable::close_window");
    for (ContextChunk &chunk : chunks_[slot]) {
        if (!chunk.in_window() || chunk.resource_id() != resource_id) continue;
        take_down(chunk, &device, false);
        return Status::ok;
    }
    return Status::invalid_parameter;
}

/* The process's view first, so it never reads a page the host no longer
 * backs; the pages last, once the device answered both the unmap and the
 * release - handed out before that, they could be placed over a mapping QEMU
 * still holds. Without a device to ask, `host_let_go` alone says whether one
 * could still be holding them. */
void RvkContextTable::take_down(ContextChunk &chunk, VirtioGpuDevice *device, bool host_let_go)
{
    AV_SCOPE("RvkContextTable::take_down");
    platform_.unmap_from_process(chunk.mapping());
    HostVisibleBlob placed(platform_, chunk.resource_id(), uint64_t{chunk.first_page()} * HostWindow::page_bytes);
    const bool      released = device != nullptr ? placed.destroy(*device) == Status::ok : host_let_go;
    if (released) window_.give_back(chunk.first_page(), chunk.pages());
    SpinLockGuard locked(lock_);
    chunk.clear();
}

Status RvkContextTable::submit(void *context, const uint8_t *private_data, size_t size, uint64_t &awaited)
{
    AV_SCOPE("RvkContextTable::submit");
    platform_.require_irql_at_most(Irql::dispatch, "RvkContextTable::submit");
    awaited = 0u;
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot] || lost(slot)) return Status::invalid_parameter;
    SubmissionDescriptor submission;
    if (reader_.read(private_data, size, RingHeader(orders_[slot], 0u, 0u), submission) != Status::ok)
        return Status::invalid_parameter;
    const Status status = bindings_[slot].object().submit(submission);
    if (status == Status::ok) awaited = submission.fence_id();
    return status;
}

void RvkContextTable::kick(void *context)
{
    AV_SCOPE("RvkContextTable::kick");
    platform_.require_irql_at_most(Irql::dispatch, "RvkContextTable::kick");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot] || lost(slot)) return;
    bindings_[slot].object().kick();
}

bool RvkContextTable::completed_already(void *context, uint64_t fence)
{
    AV_SCOPE("RvkContextTable::completed_already");
    platform_.require_irql_at_most(Irql::dispatch, "RvkContextTable::completed_already");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot] || lost(slot)) return true;
    RvkContextBinding &binding = bindings_[slot].object();
    static_cast<void>(binding.serve());
    return binding.completed() >= fence;
}

/* What a lost context submitted is finished rather than dropped: a packet
 * left on the node would hold every context's work queued behind it until a
 * timeout reset. Serving it on every DPC also finishes a packet whose batch
 * was published before the fault was seen and which reached the node after. */
void RvkContextTable::serve(ExecutionNode &node, HwQueueSubmissions &queues)
{
    AV_SCOPE("RvkContextTable::serve");
    platform_.require_irql_at_most(Irql::dispatch, "RvkContextTable::serve");
    SpinLockGuard locked(lock_);
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        if (!live_[slot]) continue;
        RvkContextBinding &binding = bindings_[slot].object();
        static_cast<void>(binding.serve());
        const uint64_t completed = lost(slot) ? UINT64_MAX : binding.completed();
        node.finish(contexts_[slot], completed);
        /* What a hardware queue of this context submitted waits on the same
         * fence, and its progress is where the process reads it (feature 049). */
        queues.completed_through(contexts_[slot], completed);
    }
}

bool RvkContextTable::lost(uint32_t slot) const
{
    AV_SCOPE("RvkContextTable::lost");
    const RvkContextBinding &binding = bindings_[slot].object();
    return binding.state() == GuestContext::State::failed || binding.faulted();
}

void RvkContextTable::abandon_all()
{
    AV_SCOPE("RvkContextTable::abandon_all");
    platform_.require_irql_at_most(Irql::passive, "RvkContextTable::abandon_all");
    WaitMutexGuard                  sequence(requests_);
    InvariantScope<RvkContextTable> guard(platform_, *this, "abandon_all");
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        {
            SpinLockGuard locked(lock_);
            if (!live_[slot]) continue;
            live_[slot] = false;
        }
        bindings_[slot].object().abandon();
    }
}

void *RvkContextTable::user_of(const Allocation *allocation)
{
    AV_SCOPE("RvkContextTable::user_of");
    platform_.require_irql_at_most(Irql::dispatch, "RvkContextTable::user_of");
    SpinLockGuard locked(lock_);
    if (allocation == nullptr) return nullptr;
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        for (const Allocation *use : uses_[slot])
            if (use == allocation) return contexts_[slot];
        for (const ContextChunk &chunk : chunks_[slot])
            if (chunk.allocation() == allocation) return contexts_[slot];
    }
    return nullptr;
}

uint32_t RvkContextTable::doorbell_of(const void *context) const
{
    AV_SCOPE("RvkContextTable::doorbell_of");
    /* The slot is the doorbell page: a context takes one of each together
     * (RvkContextTable::set_up). */
    return slot_of(context);
}

Status RvkContextTable::user_mode_submits(const void *context, uint64_t *progress)
{
    AV_SCOPE("RvkContextTable::user_mode_submits");
    platform_.require_irql_at_most(Irql::dispatch, "RvkContextTable::user_mode_submits");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot]) return Status::invalid_parameter;
    bindings_[slot].object().user_mode_submits(progress);
    return Status::ok;
}

Status RvkContextTable::driver_submits(const void *context)
{
    AV_SCOPE("RvkContextTable::driver_submits");
    platform_.require_irql_at_most(Irql::dispatch, "RvkContextTable::driver_submits");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot]) return Status::invalid_parameter;
    bindings_[slot].object().driver_submits();
    return Status::ok;
}

bool RvkContextTable::forget_progress(const void *context, const uint64_t *progress)
{
    AV_SCOPE("RvkContextTable::forget_progress");
    platform_.require_irql_at_most(Irql::dispatch, "RvkContextTable::forget_progress");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    return slot != no_slot && bindings_[slot].object().forget_progress(progress);
}

uint32_t RvkContextTable::bound() const
{
    AV_SCOPE("RvkContextTable::bound");
    uint32_t count = 0u;
    for (const void *context : contexts_) count += context != nullptr ? 1u : 0u;
    return count;
}

/* Unlocked, as doorbell_of is: a caller asking whether the table holds its own
 * context is the only thing that could be unbinding it. */
bool RvkContextTable::holds(const void *context) const
{
    AV_SCOPE("RvkContextTable::holds");
    platform_.require_irql_at_most(Irql::dispatch, "RvkContextTable::holds");
    return slot_of(context) != no_slot;
}

/* One line per context set up or refused: which doorbell, and which message
 * its fence interrupts arrive on, is what a guest that never wakes needs. */
void RvkContextTable::log_binding(uint32_t slot, uint32_t message, Status status)
{
    AV_SCOPE("RvkContextTable::log_binding");
    char       bytes[96];
    TextBuffer line(bytes, sizeof bytes);
    line.append("rvkgpu: rvk context on doorbell ");
    line.append_decimal(slot);
    line.append(" message ");
    line.append_decimal(message);
    line.append(" status 0x");
    line.append_hex(static_cast<uint32_t>(NtStatus(status).value()), 8u);
    platform_.log(line.text());
}

/* A live slot is bound and set up on both of its pages; a bound slot names
 * every allocation it runs on; a free slot holds nothing. */
void RvkContextTable::report(StateWriter &writer)
{
    AV_SCOPE("RvkContextTable::report");
    platform_.require_irql_at_most(Irql::dispatch, "RvkContextTable::report");
    const SpinLockTryGuard guard(lock_);
    writer.open("rvk_context_table", 0u);
    writer.flag("busy", !guard.held());
    if (guard.held()) {
        writer.number("bound", bound());
        for (uint32_t slot = 0u; slot < max_contexts; slot++) {
            if (contexts_[slot] == nullptr || !bindings_[slot].constructed()) continue;
            const RvkContextBinding &binding = bindings_[slot].object();
            writer.open("rvk_context", slot);
            writer.address("handle", contexts_[slot]);
            writer.number("doorbell", slot);
            writer.number("ring_order", orders_[slot]);
            writer.address("ring", uses_[slot][ring_role]);
            writer.address("fence_page", uses_[slot][fence_role]);
            writer.address("reply", uses_[slot][reply_role]);
            writer.number("state", static_cast<uint8_t>(binding.state()));
            writer.number("context_id", binding.context_id());
            writer.number("completed", binding.completed());
            writer.flag("live", live_[slot]);
            writer.flag("lost", lost(slot));
            for (uint32_t chunk = 0u; chunk < RvkContextBinding::chunk_slots; chunk++) {
                const ContextChunk &held = chunks_[slot][chunk];
                if (!held.bound()) continue;
                writer.open("chunk", chunk);
                writer.number("resource", held.resource_id());
                if (held.in_window()) {
                    writer.number("window_page", held.first_page());
                    writer.number("pages", held.pages());
                } else {
                    writer.address("allocation", held.allocation());
                }
                writer.close();
            }
            writer.close();
        }
    }
    writer.close();
}

bool RvkContextTable::invariants_hold() const
{
    AV_SCOPE("RvkContextTable::invariants_hold");
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        const bool bound_slot = contexts_[slot] != nullptr;
        const bool mapped = ring_views_[slot] != nullptr && fence_views_[slot] != nullptr;
        if (live_[slot] && !(bound_slot && mapped && bindings_[slot].constructed())) return false;
        if (!bound_slot && (bindings_[slot].constructed() || ring_views_[slot] != nullptr ||
                            fence_views_[slot] != nullptr || uses_[slot][ring_role] != nullptr))
            return false;
        if (bound_slot && (uses_[slot][ring_role] == nullptr || uses_[slot][fence_role] == nullptr ||
                           uses_[slot][reply_role] == nullptr))
            return false;
        for (const ContextChunk &chunk : chunks_[slot])
            if (!chunk.invariants_hold() || (chunk.bound() && !bound_slot)) return false;
    }
    return true;
}

}  // namespace av
