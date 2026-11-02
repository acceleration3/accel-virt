#include "miniport/av_context/av_context_table.hpp"

#include "miniport/scheduling/hw_queue_submissions.hpp"

#include "miniport/memory/allocation.hpp"
#include "miniport/memory/allocation_table.hpp"
#include "miniport/memory/host_visible_blob.hpp"
#include "miniport/memory/host_window.hpp"
#include "miniport/driver/acquired_handle_data.hpp"
#include "core/platform/register_window.hpp"
#include "core/platform/spin_lock_guard.hpp"
#include "core/platform/wait_mutex_guard.hpp"
#include "miniport/av_context/context_pages.hpp"
#include "miniport/scheduling/execution_node.hpp"
#include "core/platform/invariant_scope.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/platform/spin_lock_try_guard.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "miniport/memory/shared_object_release.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/escape/host_window_record.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

namespace av {

AvContextTable::AvContextTable(Platform &platform, DxgkrnlInterface &dxgkrnl, AllocationTable &allocations,
                                 HostWindow &window)
    : platform_(platform), dxgkrnl_(dxgkrnl), allocations_(allocations), window_(window),
      lock_(platform, LockRank::av_context_slots),
      reader_(platform), bindings_(), contexts_{}, names_{}, memories_(), chunks_(platform), ids_(platform, HostBlobId::most), held_chunks_{}, orders_{},
      wakes_{}, raised_{}, told_lost_{}, generations_{}, live_{}, request_{}, requests_(platform, LockRank::av_control_requests)
      {
          AV_SCOPE("AvContextTable::AvContextTable");
      }

AvContextTable::~AvContextTable()
{
    AV_SCOPE("AvContextTable::~AvContextTable");
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        if (contexts_[slot] == nullptr) continue;
        release_chunks(slot, nullptr, true);
        release(slot);
    }
}

uint32_t AvContextTable::slot_of(const void *context) const
{
    AV_SCOPE("AvContextTable::slot_of");
    for (uint32_t slot = 0u; slot < max_contexts; slot++)
        if (context != nullptr && contexts_[slot] == context) return slot;
    return no_slot;
}

uint32_t AvContextTable::claim(void *context, bool &duplicate)
{
    AV_SCOPE("AvContextTable::claim");
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

Status AvContextTable::resolve(uint32_t handle, AllocationKind kind, Allocation *&allocation)
{
    AV_SCOPE("AvContextTable::resolve");
    allocation = nullptr;
    AcquiredHandleData acquired(dxgkrnl_, handle);
    if (acquired.acquire() != Status::ok) return Status::invalid_parameter;
    Allocation *const found = allocations_.find(acquired.data());
    if (found == nullptr || found->descriptor().kind() != kind || user_of(found) != nullptr)
        return Status::invalid_parameter;
    allocation = found;
    return Status::ok;
}

Status AvContextTable::resolve_share(uint32_t slot, SubApi asking, uint32_t handle, uint32_t &context_id)
{
    AV_SCOPE("AvContextTable::resolve_share");
    context_id = 0u;
    if (handle == 0u) return Status::ok;
    AcquiredHandleData acquired(dxgkrnl_, handle);
    if (acquired.acquire() != Status::ok) return Status::invalid_parameter;
    const Allocation *const ring = allocations_.find(acquired.data());
    SpinLockGuard           locked(lock_);
    for (uint32_t other = 0u; other < max_contexts; other++) {
        if (other == slot || names_[other] != ring || !live_[other]) continue;
        const AvContextBinding &binding = bindings_[other].object();
        const Allocation &own = *names_[slot];
        const bool same_owner = asking == SubApi::opengl ? ring->held_with(own) : ring->held_in_process_of(own);
        if (binding.sub_api() != SubApi::opengl || !same_owner) return Status::invalid_parameter;
        context_id = binding.context_id();
        return Status::ok;
    }
    return Status::invalid_parameter;
}

Status AvContextTable::bind(void *context, const AvContextRecord &record, VirtioGpuDevice &device,
                             RegisterWindow &doorbells, const CapsetInfo &capset)
{
    AV_SCOPE("AvContextTable::bind");
    platform_.require_irql_at_most(Irql::passive, "AvContextTable::bind");
    WaitMutexGuard                  sequence(requests_);
    InvariantScope<AvContextTable> guard(platform_, *this, "bind");
    if (context == nullptr) return Status::invalid_state;
    bool           duplicate = false;
    const uint32_t slot = claim(context, duplicate);
    if (duplicate) return Status::invalid_state;
    if (slot == no_slot) return Status::no_space;
    ScopeExit unwind([this, slot] { release(slot); });
    if (doorbells.length() < (uint64_t{slot} + 1u) * RingSubmitter::doorbell_page_bytes) return Status::no_space;
    Allocation  *name = nullptr;
    const Status named = resolve(record.ring(), AllocationKind::context_ring, name);
    if (named != Status::ok) return named;
    {
        SpinLockGuard locked(lock_);
        names_[slot] = name;
        orders_[slot] = record.ring_order();
    }
    uint32_t     share = 0u;
    const Status shared =
        resolve_share(slot, record.sub_api(), record.options().share_context(), share);
    if (shared != Status::ok) return shared;
    static_cast<void>(memories_[slot].construct(platform_));
    ContextMemory &memory = memories_[slot].object();
    Status         status = memory.allocate(record.ring_order(), record.reply_bytes());
    if (status == Status::ok) status = memory.map_into_process();
    if (status != Status::ok) return status == Status::invalid_parameter ? status : Status::no_memory;
    const AvContextRecord sharing(record.sub_api(), record.ring_order(), record.ring(), record.reply_bytes(),
                                   record.options().sharing(share));
    status = set_up(slot, sharing, device, doorbells, capset);
    if (status != Status::ok) return status;
    unwind.dismiss();
    SpinLockGuard locked(lock_);
    live_[slot] = true;
    return Status::ok;
}

Status AvContextTable::set_up(uint32_t slot, const AvContextRecord &record, VirtioGpuDevice &device,
                               RegisterWindow &doorbells, const CapsetInfo &capset)
{
    AV_SCOPE("AvContextTable::set_up");
    uint32_t message = 0u;
    Status   status = device.connect_fence_queue(slot, message);
    if (status == Status::ok)
        status = bindings_[slot].construct(platform_, doorbells, record, slot, generations_[slot]++, request_,
                                           sizeof request_);
    if (status == Status::ok)
        status = bindings_[slot].object().set_up(device, capset, memories_[slot].object().pages());
    log_binding(slot, message, status);
    if (status == Status::ok || !bindings_[slot].constructed()) return status;
    AvContextBinding &binding = bindings_[slot].object();
    const bool         asked = binding.state() != GuestContext::State::idle;
    if (binding.tear_down(device) != Status::ok) {
        binding.abandon();
        if (asked) memories_[slot].object().abandon();
    }
    return status;
}

Status AvContextTable::unbind(void *context, VirtioGpuDevice *device)
{
    AV_SCOPE("AvContextTable::unbind");
    platform_.require_irql_at_most(Irql::passive, "AvContextTable::unbind");
    WaitMutexGuard                  sequence(requests_);
    InvariantScope<AvContextTable> guard(platform_, *this, "unbind");
    uint32_t slot = no_slot;
    {
        SpinLockGuard locked(lock_);
        slot = slot_of(context);
        if (slot == no_slot) return Status::invalid_parameter;
        live_[slot] = false;
    }
    AvContextBinding &binding = bindings_[slot].object();
    const bool held = binding.state() == GuestContext::State::established ||
                      binding.state() == GuestContext::State::failed;
    const bool torn_down = held && device != nullptr && binding.tear_down(*device) == Status::ok;
    const bool let_go = torn_down || !held || device == nullptr;
    release_chunks(slot, torn_down ? device : nullptr, let_go);
    if (!torn_down) binding.abandon();
    if (!let_go) memories_[slot].object().abandon();
    release(slot);
    return Status::ok;
}

void AvContextTable::release(uint32_t slot)
{
    AV_SCOPE("AvContextTable::release");
    bindings_[slot].destroy();
    memories_[slot].destroy();
    void *wake = nullptr;
    {
        SpinLockGuard locked(lock_);
        releases_[slot].clear();
        names_[slot] = nullptr;
        orders_[slot] = 0u;
        wake = wakes_[slot];
        wakes_[slot] = nullptr;
        raised_[slot] = 0u;
        told_lost_[slot] = false;
        contexts_[slot] = nullptr;
    }
    if (wake != nullptr) {
        platform_.signal_event(wake);
        platform_.release_event(wake);
    }
}

void AvContextTable::release_chunks(uint32_t slot, VirtioGpuDevice *device, bool host_let_go)
{
    AV_SCOPE("AvContextTable::release_chunks");
    for (ContextChunk *chunk = nullptr;
         (chunk = chunks_.find_if([slot](const ContextChunk &held) { return held.slot() == slot; })) != nullptr;)
        take_down(*chunk, device, host_let_go);
}

ContextChunk *AvContextTable::find_chunk(uint32_t slot, uint32_t resource_id)
{
    AV_SCOPE("AvContextTable::find_chunk");
    return chunks_.find_if([slot, resource_id](const ContextChunk &held) {
        return held.slot() == slot && held.resource_id() == resource_id;
    });
}

Status AvContextTable::release_shared(uint64_t object_id, VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::release_shared");
    platform_.require_irql_at_most(Irql::passive, "AvContextTable::release_shared");
    WaitMutexGuard      sequence(requests_);
    SharedObjectRelease release(platform_);
    return release.send(device, object_id);
}

Status AvContextTable::window_chunk(void *context, HostWindowRecord &record, VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::window_chunk");
    platform_.require_irql_at_most(Irql::passive, "AvContextTable::window_chunk");
    WaitMutexGuard                  sequence(requests_);
    InvariantScope<AvContextTable> guard(platform_, *this, "window_chunk");
    uint32_t                        slot = no_slot;
    {
        SpinLockGuard locked(lock_);
        slot = slot_of(context);
        if (slot == no_slot || !live_[slot] || lost(slot)) return Status::invalid_parameter;
    }
    if (record.adopts()) return adopt_pages(slot, record, device);
    if (!record.opens()) return close_window(slot, record.resource_id(), device);
    return open_window(slot, record, device);
}

Status AvContextTable::views(void *context, ContextViewsRecord &record)
{
    AV_SCOPE("AvContextTable::views");
    platform_.require_irql_at_most(Irql::passive, "AvContextTable::views");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot]) return Status::invalid_parameter;
    const ContextMemory &memory = memories_[slot].object();
    record = record.answered(memory.ring_view(), memory.reply_view(), memory.fence_view());
    return Status::ok;
}

Status AvContextTable::wake_on_raise(void *context, uint64_t handle)
{
    AV_SCOPE("AvContextTable::wake_on_raise");
    platform_.require_irql_at_most(Irql::passive, "AvContextTable::wake_on_raise");
    void *event = nullptr;
    if (platform_.reference_event(handle, event) != Status::ok) return Status::invalid_parameter;
    void *previous = nullptr;
    bool  stored = false;
    {
        SpinLockGuard  locked(lock_);
        const uint32_t slot = slot_of(context);
        if (slot != no_slot && live_[slot]) {
            previous = wakes_[slot];
            wakes_[slot] = event;
            stored = true;
        }
    }
    if (!stored) {
        platform_.release_event(event);
        return Status::invalid_parameter;
    }
    if (previous != nullptr) platform_.release_event(previous);
    return Status::ok;
}

Status AvContextTable::open_window(uint32_t slot, HostWindowRecord &record, VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::open_window");
    uint32_t     resource = 0u;
    const Status taken = ids_.take(resource);
    if (taken != Status::ok) return taken;
    return place_in_window(slot, resource, record, device);
}

Status AvContextTable::place_in_window(uint32_t slot, uint32_t resource, HostWindowRecord &record,
                                        VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::place_in_window");
    bool      keep_id = false;
    ScopeExit id_back([this, resource, &keep_id] {
        if (!keep_id) ids_.give_back(resource);
    });
    const uint64_t wanted_pages = record.bytes() / HostWindow::page_bytes +
                                  (record.bytes() % HostWindow::page_bytes != 0u ? 1u : 0u);
    if (wanted_pages > UINT32_MAX) return Status::no_space;
    const auto     pages = static_cast<uint32_t>(wanted_pages);
    const uint64_t bytes = wanted_pages * HostWindow::page_bytes;
    uint32_t       first = 0u;
    const Status   taken = window_.take(pages, first);
    if (taken != Status::ok) return taken;
    HostVisibleBlob blob(platform_);
    ScopeExit       unwind([this, &blob, &device, &keep_id, first, pages] {
        if (blob.created()) static_cast<void>(blob.destroy(device));
        keep_id = blob.created();
        if (!keep_id) window_.give_back(first, pages);
    });
    uint64_t        view = 0u;
    void           *mapping = nullptr;
    const uint32_t  context = bindings_[slot].object().context_id();
    Status          status = blob.create(device, context, resource, record.memory(), bytes);
    if (status == Status::ok) status = blob.map(device, uint64_t{first} * HostWindow::page_bytes);
    if (status == Status::ok)
        status = platform_.map_to_process(window_.physical(first), bytes, blob.caching(), view, mapping);
    ContextChunk *chunk = nullptr;
    if (status == Status::ok && chunks_.create(chunk, slot, resource, first, pages, mapping) != Status::ok) {
        platform_.unmap_from_process(mapping);
        status = Status::no_memory;
    }
    if (status != Status::ok) return status;
    unwind.dismiss();
    keep_id = true;
    {
        SpinLockGuard locked(lock_);
        held_chunks_[slot]++;
    }
    record = record.answered(resource, bytes, view);
    return Status::ok;
}

Status AvContextTable::adopt_pages(uint32_t slot, HostWindowRecord &record, VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::adopt_pages");
    if (record.bytes() > max_adopted_bytes) return Status::no_space;
    void        *locked = nullptr;
    const Status lock = platform_.lock_process_pages(record.address(), record.bytes(), locked);
    if (lock != Status::ok) return lock;
    const Status adopted = adopt_locked(slot, locked, record, device);
    if (adopted != Status::ok) platform_.unlock_process_pages(locked);
    return adopted;
}

Status AvContextTable::adopt_locked(uint32_t slot, void *locked, HostWindowRecord &record, VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::adopt_locked");
    uint32_t resource = 0u;
    const Status taken = ids_.take(resource);
    if (taken != Status::ok) return taken;
    bool            keep_id = false;
    HostVisibleBlob blob(platform_);
    ScopeExit       unwind([this, &blob, &device, &keep_id, resource] {
        if (blob.created()) static_cast<void>(blob.destroy(device));
        keep_id = blob.created();
        if (!keep_id) ids_.give_back(resource);
    });
    const uint32_t context = bindings_[slot].object().context_id();
    Status status = blob.create_over(device, context, resource, record.memory(), locked, record.bytes());
    ContextChunk *chunk = nullptr;
    if (status == Status::ok && chunks_.create(chunk, slot, resource, locked) != Status::ok) status = Status::no_memory;
    if (status != Status::ok) return status;
    unwind.dismiss();
    {
        SpinLockGuard guard(lock_);
        held_chunks_[slot]++;
    }
    record = record.answered(resource, record.bytes(), record.address());
    return Status::ok;
}

Status AvContextTable::close_window(uint32_t slot, uint32_t resource_id, VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::close_window");
    ContextChunk *const chunk = find_chunk(slot, resource_id);
    if (chunk == nullptr) return Status::invalid_parameter;
    take_down(*chunk, &device, false);
    return Status::ok;
}

void AvContextTable::take_down(ContextChunk &chunk, VirtioGpuDevice *device, bool host_let_go)
{
    AV_SCOPE("AvContextTable::take_down");
    if (chunk.locked() == nullptr) platform_.unmap_from_process(chunk.mapping());
    HostVisibleBlob placed(platform_, chunk.resource_id(), uint64_t{chunk.first_page()} * HostWindow::page_bytes);
    HostVisibleBlob lent(platform_, chunk.resource_id());
    HostVisibleBlob &blob = chunk.locked() == nullptr ? placed : lent;
    const bool       released = device != nullptr ? blob.destroy(*device) == Status::ok : host_let_go;
    if (released) {
        if (chunk.locked() == nullptr) window_.give_back(chunk.first_page(), chunk.pages());
        ids_.give_back(chunk.resource_id());
    }
    if (chunk.locked() != nullptr) platform_.unlock_process_pages(chunk.locked());
    const uint32_t slot = chunk.slot();
    static_cast<void>(chunks_.destroy(&chunk));
    SpinLockGuard locked(lock_);
    held_chunks_[slot]--;
}

Status AvContextTable::submit(void *context, const uint8_t *private_data, size_t size, uint64_t &awaited)
{
    AV_SCOPE("AvContextTable::submit");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::submit");
    awaited = 0u;
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot] || lost(slot)) return Status::invalid_parameter;
    SubmissionDescriptor submission;
    if (reader_.read(private_data, size, RingHeader(orders_[slot], 0u, 0u), submission) != Status::ok)
        return Status::invalid_parameter;
    const Status status = releases_[slot].active() ? in_turn(slot, submission) : bindings_[slot].object().submit(submission);
    if (status == Status::ok) awaited = submission.fence_id();
    return status;
}

Status AvContextTable::release_batch(void *context, const SubmissionDescriptor &batch, uint64_t &awaited)
{
    AV_SCOPE("AvContextTable::release_batch");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::release_batch");
    awaited = 0u;
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot] || lost(slot)) return Status::invalid_parameter;
    if (batch.validate_against(RingHeader(orders_[slot], 0u, 0u)) != Status::ok) return Status::invalid_parameter;
    const Status status = in_turn(slot, batch);
    if (status == Status::ok) awaited = batch.fence_id();
    return status;
}

Status AvContextTable::in_turn(uint32_t slot, const SubmissionDescriptor &batch)
{
    AV_SCOPE("AvContextTable::in_turn");
    ReleaseQueue &queue = releases_[slot];
    AvContextBinding &binding = bindings_[slot].object();
    const ReleaseQueue::Turn turn = queue.arrive(batch, binding.next_begin());
    if (turn == ReleaseQueue::Turn::held || turn == ReleaseQueue::Turn::released_already) return Status::ok;
    if (turn == ReleaseQueue::Turn::refused || binding.submit(batch) != Status::ok) return lose(slot);
    queue.released(batch);
    SubmissionDescriptor next;
    while (queue.next_held(next)) {
        if (binding.submit(next) != Status::ok) return lose(slot);
        queue.released(next);
    }
    return Status::ok;
}

Status AvContextTable::lose(uint32_t slot)
{
    AV_SCOPE("AvContextTable::lose");
    bindings_[slot].object().fail();
    releases_[slot].clear();
    platform_.log("avgpu: a context's batches cannot be released in order: the context is lost");
    return Status::ok;
}

bool AvContextTable::fault_if_held(void *context)
{
    AV_SCOPE("AvContextTable::fault_if_held");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::fault_if_held");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot] || releases_[slot].held() == 0u) return false;
    static_cast<void>(lose(slot));
    return true;
}

void AvContextTable::kick(void *context)
{
    AV_SCOPE("AvContextTable::kick");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::kick");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot] || lost(slot)) return;
    bindings_[slot].object().kick();
}

bool AvContextTable::completed_already(void *context, uint64_t fence)
{
    AV_SCOPE("AvContextTable::completed_already");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::completed_already");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot] || lost(slot)) return true;
    AvContextBinding &binding = bindings_[slot].object();
    static_cast<void>(binding.serve());
    return binding.completed() >= fence;
}

void AvContextTable::serve(ExecutionNode &node, HwQueueSubmissions &queues)
{
    AV_SCOPE("AvContextTable::serve");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::serve");
    SpinLockGuard locked(lock_);
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        if (!live_[slot]) continue;
        AvContextBinding &binding = bindings_[slot].object();
        static_cast<void>(binding.serve());
        const bool     lost_now = lost(slot);
        const uint64_t completed = lost_now ? UINT64_MAX : binding.completed();
        wake(slot, binding.raised(), lost_now);
        node.finish(contexts_[slot], completed);
        queues.completed_through(contexts_[slot], completed);
    }
}

void AvContextTable::wake(uint32_t slot, uint64_t raised, bool lost_now)
{
    AV_SCOPE("AvContextTable::wake");
    const bool moved = raised != raised_[slot] || (lost_now && !told_lost_[slot]);
    raised_[slot] = raised;
    told_lost_[slot] = told_lost_[slot] || lost_now;
    if (moved && wakes_[slot] != nullptr) platform_.signal_event(wakes_[slot]);
}

bool AvContextTable::lost(uint32_t slot) const
{
    AV_SCOPE("AvContextTable::lost");
    const AvContextBinding &binding = bindings_[slot].object();
    return binding.state() == GuestContext::State::failed || binding.faulted();
}

void AvContextTable::abandon_all()
{
    AV_SCOPE("AvContextTable::abandon_all");
    platform_.require_irql_at_most(Irql::passive, "AvContextTable::abandon_all");
    WaitMutexGuard                  sequence(requests_);
    InvariantScope<AvContextTable> guard(platform_, *this, "abandon_all");
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        {
            SpinLockGuard locked(lock_);
            if (!live_[slot]) continue;
            live_[slot] = false;
            releases_[slot].clear();
            if (wakes_[slot] != nullptr) platform_.signal_event(wakes_[slot]);
        }
        bindings_[slot].object().abandon();
    }
}

void *AvContextTable::user_of(const Allocation *allocation)
{
    AV_SCOPE("AvContextTable::user_of");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::user_of");
    SpinLockGuard locked(lock_);
    if (allocation == nullptr) return nullptr;
    for (uint32_t slot = 0u; slot < max_contexts; slot++)
        if (names_[slot] == allocation) return contexts_[slot];
    return nullptr;
}

uint32_t AvContextTable::doorbell_of(const void *context) const
{
    AV_SCOPE("AvContextTable::doorbell_of");
    return slot_of(context);
}

Status AvContextTable::user_mode_submits(const void *context, uint64_t *progress)
{
    AV_SCOPE("AvContextTable::user_mode_submits");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::user_mode_submits");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot]) return Status::invalid_parameter;
    bindings_[slot].object().user_mode_submits(progress);
    return Status::ok;
}

Status AvContextTable::driver_submits(const void *context)
{
    AV_SCOPE("AvContextTable::driver_submits");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::driver_submits");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    if (slot == no_slot || !live_[slot]) return Status::invalid_parameter;
    bindings_[slot].object().driver_submits();
    return Status::ok;
}

bool AvContextTable::forget_progress(const void *context, const uint64_t *progress)
{
    AV_SCOPE("AvContextTable::forget_progress");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::forget_progress");
    SpinLockGuard  locked(lock_);
    const uint32_t slot = slot_of(context);
    return slot != no_slot && bindings_[slot].object().forget_progress(progress);
}

uint32_t AvContextTable::bound() const
{
    AV_SCOPE("AvContextTable::bound");
    uint32_t count = 0u;
    for (const void *context : contexts_) count += context != nullptr ? 1u : 0u;
    return count;
}

bool AvContextTable::holds(const void *context) const
{
    AV_SCOPE("AvContextTable::holds");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::holds");
    return slot_of(context) != no_slot;
}

void AvContextTable::log_binding(uint32_t slot, uint32_t message, Status status)
{
    AV_SCOPE("AvContextTable::log_binding");
    char       bytes[96];
    TextBuffer line(bytes, sizeof bytes);
    line.append("avgpu: av context on doorbell ");
    line.append_decimal(slot);
    line.append(" message ");
    line.append_decimal(message);
    line.append(" status 0x");
    line.append_hex(static_cast<uint32_t>(NtStatus(status).value()), 8u);
    platform_.log(line.text());
}

void AvContextTable::report(StateWriter &writer)
{
    AV_SCOPE("AvContextTable::report");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::report");
    const SpinLockTryGuard guard(lock_);
    writer.open("av_context_table", 0u);
    writer.flag("busy", !guard.held());
    if (guard.held()) {
        writer.number("bound", bound());
        for (uint32_t slot = 0u; slot < max_contexts; slot++) {
            if (contexts_[slot] == nullptr || !bindings_[slot].constructed()) continue;
            const AvContextBinding &binding = bindings_[slot].object();
            writer.open("av_context", slot);
            writer.address("handle", contexts_[slot]);
            writer.number("doorbell", slot);
            writer.number("ring_order", orders_[slot]);
            writer.address("name", names_[slot]);
            if (memories_[slot].constructed()) {
                writer.number("ring_view", memories_[slot].object().ring_view());
                writer.number("reply_view", memories_[slot].object().reply_view());
            }
            writer.number("state", static_cast<uint8_t>(binding.state()));
            writer.number("context_id", binding.context_id());
            writer.number("completed", binding.completed());
            writer.flag("live", live_[slot]);
            writer.flag("lost", lost(slot));
            writer.flag("wakes", wakes_[slot] != nullptr);
            writer.number("raised", raised_[slot]);
            writer.number("chunks", held_chunks_[slot]);
            writer.close();
        }
    }
    writer.close();
}

bool AvContextTable::invariants_hold() const
{
    AV_SCOPE("AvContextTable::invariants_hold");
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        const bool bound_slot = contexts_[slot] != nullptr;
        const bool running = memories_[slot].constructed() && memories_[slot].object().allocated() &&
                             memories_[slot].object().ring_view() != 0u;
        if (live_[slot] && !(bound_slot && running && bindings_[slot].constructed())) return false;
        if (!bound_slot && (bindings_[slot].constructed() || memories_[slot].constructed() || names_[slot] != nullptr ||
                            wakes_[slot] != nullptr || raised_[slot] != 0u || told_lost_[slot]))
            return false;
        if (bound_slot && names_[slot] == nullptr) return false;
        if (memories_[slot].constructed() && !memories_[slot].object().invariants_hold()) return false;
        uint32_t held = 0u;
        chunks_.for_each([slot, &held](const ContextChunk &chunk) { held += chunk.slot() == slot ? 1u : 0u; });
        if (held != held_chunks_[slot] || (held != 0u && !bound_slot)) return false;
    }
    return chunks_.all_of([](const ContextChunk &chunk) { return chunk.invariants_hold(); }) &&
           chunks_.live() <= ids_.taken();
}

}
