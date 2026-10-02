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
#include "core/support/invariant_scope.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/platform/spin_lock_try_guard.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "miniport/memory/shared_object_release.hpp"
#include "proto/context/context_views_record.hpp"
#include "proto/context/av_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/host_window_record.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

namespace av {

AvContextTable::AvContextTable(Platform &platform, DxgkrnlInterface &dxgkrnl, AllocationTable &allocations,
                                 HostWindow &window)
    : platform_(platform), dxgkrnl_(dxgkrnl), allocations_(allocations), window_(window),
      lock_(platform, LockRank::av_context_slots),
      reader_(platform), bindings_(), contexts_{}, names_{}, memories_(), chunks_(), ram_chunks_(platform), orders_{},
      generations_{}, live_{}, request_{}, requests_(platform, LockRank::av_control_requests)
      {
          AV_SCOPE("AvContextTable::AvContextTable");
      }

AvContextTable::~AvContextTable()
{
    AV_SCOPE("AvContextTable::~AvContextTable");
    /* The device stopped before the adapter goes, and holds nothing. */
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

/* The handle is the process's, so it is resolved through dxgkrnl and borrowed
 * only for the lookup; what outlives it is the driver's own Allocation, whose
 * freeing the adapter asks this table about first. */
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
        /* A slot with no context names no ring, so a handle that is no allocation matches nothing. */
        if (other == slot || names_[other] != ring || !live_[other]) continue;
        const AvContextBinding &binding = bindings_[other].object();
        /* An OpenGL context shares within its own device; an OpenCL one comes
         * from another ICD, on another device of the same process. */
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

/* A setup the device was asked for and would not undo may leave it holding
 * pages, which are then never handed out again; one refused before anything
 * was sent leaves the device holding nothing. */
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
    /* The context before its chunks: the device keeps a chunk it holds imported
     * and refuses to release it, and destroying the context is what ends every
     * import it made. */
    const bool torn_down = held && device != nullptr && binding.tear_down(*device) == Status::ok;
    /* A device reset or gone holds nothing of the context, window chunks
     * included; one that stopped answering mid-teardown may still hold them
     * placed, and the context's own pages, and all of them are kept out of use
     * for good. */
    const bool let_go = torn_down || !held || device == nullptr;
    release_chunks(slot, torn_down ? device : nullptr, let_go);
    if (!torn_down) binding.abandon();
    if (!let_go) memories_[slot].object().abandon();
    release(slot);
    return Status::ok;
}

/* Everything a slot holds, in the reverse of how bind took it; the slot is
 * given back last, so nobody claims it while its pages are still mapped. */
void AvContextTable::release(uint32_t slot)
{
    AV_SCOPE("AvContextTable::release");
    bindings_[slot].destroy();
    memories_[slot].destroy();
    SpinLockGuard locked(lock_);
    names_[slot] = nullptr;
    for (ContextChunk &chunk : chunks_[slot]) chunk.clear();
    orders_[slot] = 0u;
    contexts_[slot] = nullptr;
}

void AvContextTable::release_chunks(uint32_t slot, VirtioGpuDevice *device, bool host_let_go)
{
    AV_SCOPE("AvContextTable::release_chunks");
    for (ContextChunk &chunk : chunks_[slot]) {
        if (!chunk.bound()) continue;
        if (chunk.in_window()) take_down(chunk, device, host_let_go);
        else take_down_ram(slot, chunk, device, host_let_go);
    }
}

uint32_t AvContextTable::free_chunk(uint32_t slot) const
{
    AV_SCOPE("AvContextTable::free_chunk");
    uint32_t index = 0u;
    while (index < AvContextBinding::chunk_slots && chunks_[slot][index].bound()) index++;
    return index;
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
    record = record.answered(memory.ring_view(), memory.reply_view());
    return Status::ok;
}

/* A window this adapter lacks, one too full for the chunk, a device that will
 * not place a blob there, or a process that cannot be given a view of it within
 * its reach: the chunk is guest RAM instead, which the host imports as it does
 * the window's pages. */
Status AvContextTable::open_window(uint32_t slot, HostWindowRecord &record, VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::open_window");
    const uint32_t index = free_chunk(slot);
    if (index == AvContextBinding::chunk_slots) return Status::no_space;
    if (place_in_window(slot, index, record, device) == Status::ok) return Status::ok;
    return place_in_ram(slot, index, record, device);
}

/* Each step is undone in reverse if a later one fails. The pages go back only
 * once the device holds no blob over them: a blob it would not release may
 * still be placed there, and the next chunk must not land on it. */
Status AvContextTable::place_in_window(uint32_t slot, uint32_t index, HostWindowRecord &record,
                                        VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::place_in_window");
    const uint64_t wanted_pages = record.bytes() / HostWindow::page_bytes +
                                  (record.bytes() % HostWindow::page_bytes != 0u ? 1u : 0u);
    if (wanted_pages > UINT32_MAX) return Status::no_space;
    const auto     pages = static_cast<uint32_t>(wanted_pages);
    const uint64_t bytes = wanted_pages * HostWindow::page_bytes;
    uint32_t       first = 0u;
    const Status   taken = window_.take(pages, first);
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

/* A device that answered anything but a refusal may have made the blob over
 * the pages, so they are then kept out of use rather than freed. */
Status AvContextTable::place_in_ram(uint32_t slot, uint32_t index, HostWindowRecord &record,
                                     VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::place_in_ram");
    const uint64_t page_mask = GuestPageList::page_bytes - 1u;
    if (record.bytes() > UINT64_MAX - page_mask) return Status::no_space;
    const uint64_t bytes = (record.bytes() + page_mask) & ~page_mask;
    GuestPages    *pages = nullptr;
    if (ram_chunks_.create(pages, platform_) != Status::ok) return Status::no_memory;
    ScopeExit drop([this, pages] { static_cast<void>(ram_chunks_.destroy(pages)); });
    Status    status = pages->allocate(bytes);
    if (status == Status::ok) status = pages->map_into_process();
    if (status != Status::ok) return status == Status::invalid_parameter ? Status::no_space : Status::no_memory;
    AvContextBinding &binding = bindings_[slot].object();
    const uint32_t     resource = binding.chunk_resource(index);
    status = binding.publish_chunk(device, resource, pages->pages());
    if (status != Status::ok) {
        if (status != Status::not_supported) pages->abandon();
        return status;
    }
    drop.dismiss();
    {
        SpinLockGuard locked(lock_);
        chunks_[slot][index].take_ram(resource, *pages);
    }
    record = record.answered(resource, bytes, pages->view());
    return Status::ok;
}

/* A RAM chunk the device will not let go of - an import still holds it -
 * stays bound, so a process that closes one early cannot have the driver keep
 * page after page out of use; the context's teardown ends the import. */
Status AvContextTable::close_window(uint32_t slot, uint32_t resource_id, VirtioGpuDevice &device)
{
    AV_SCOPE("AvContextTable::close_window");
    for (ContextChunk &chunk : chunks_[slot]) {
        if (!chunk.bound() || chunk.resource_id() != resource_id) continue;
        if (chunk.in_window()) {
            take_down(chunk, &device, false);
            return Status::ok;
        }
        const Status released = bindings_[slot].object().release_chunk(device, resource_id);
        if (released != Status::ok) return released;
        static_cast<void>(ram_chunks_.destroy(chunk.ram()));
        SpinLockGuard locked(lock_);
        chunk.clear();
        return Status::ok;
    }
    return Status::invalid_parameter;
}

/* The process's view first, so it never reads a page the host no longer
 * backs; the pages last, once the device answered both the unmap and the
 * release - handed out before that, they could be placed over a mapping QEMU
 * still holds. Without a device to ask, `host_let_go` alone says whether one
 * could still be holding them. */
void AvContextTable::take_down(ContextChunk &chunk, VirtioGpuDevice *device, bool host_let_go)
{
    AV_SCOPE("AvContextTable::take_down");
    platform_.unmap_from_process(chunk.mapping());
    HostVisibleBlob placed(platform_, chunk.resource_id(), uint64_t{chunk.first_page()} * HostWindow::page_bytes);
    const bool      released = device != nullptr ? placed.destroy(*device) == Status::ok : host_let_go;
    if (released) window_.give_back(chunk.first_page(), chunk.pages());
    SpinLockGuard locked(lock_);
    chunk.clear();
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
    const Status status = bindings_[slot].object().submit(submission);
    if (status == Status::ok) awaited = submission.fence_id();
    return status;
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

/* What a lost context submitted is finished rather than dropped: a packet
 * left on the node would hold every context's work queued behind it until a
 * timeout reset. Serving it on every DPC also finishes a packet whose batch
 * was published before the fault was seen and which reached the node after. */
void AvContextTable::serve(ExecutionNode &node, HwQueueSubmissions &queues)
{
    AV_SCOPE("AvContextTable::serve");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::serve");
    SpinLockGuard locked(lock_);
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        if (!live_[slot]) continue;
        AvContextBinding &binding = bindings_[slot].object();
        static_cast<void>(binding.serve());
        const uint64_t completed = lost(slot) ? UINT64_MAX : binding.completed();
        node.finish(contexts_[slot], completed);
        /* What a hardware queue of this context submitted waits on the same
         * fence, and its progress is where the process reads it (feature 049). */
        queues.completed_through(contexts_[slot], completed);
    }
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
        }
        bindings_[slot].object().abandon();
    }
}

/* The pages go once the device let go of the blob over them. One that would
 * not, or that could not be asked and may still hold it, has them kept out of
 * use for good. */
void AvContextTable::take_down_ram(uint32_t slot, ContextChunk &chunk, VirtioGpuDevice *device, bool host_let_go)
{
    AV_SCOPE("AvContextTable::take_down_ram");
    GuestPages *const pages = chunk.ram();
    const bool        released =
        device != nullptr ? bindings_[slot].object().release_chunk(*device, chunk.resource_id()) == Status::ok
                          : host_let_go;
    if (!released) pages->abandon();
    static_cast<void>(ram_chunks_.destroy(pages));
    SpinLockGuard locked(lock_);
    chunk.clear();
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
    /* The slot is the doorbell page: a context takes one of each together
     * (AvContextTable::set_up). */
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

/* Unlocked, as doorbell_of is: a caller asking whether the table holds its own
 * context is the only thing that could be unbinding it. */
bool AvContextTable::holds(const void *context) const
{
    AV_SCOPE("AvContextTable::holds");
    platform_.require_irql_at_most(Irql::dispatch, "AvContextTable::holds");
    return slot_of(context) != no_slot;
}

/* One line per context set up or refused: which doorbell, and which message
 * its fence interrupts arrive on, is what a guest that never wakes needs. */
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

/* A live slot is bound and set up on the memory it holds, mapped into its
 * process; a bound slot is named by an allocation; a free slot holds
 * nothing. */
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
            for (uint32_t chunk = 0u; chunk < AvContextBinding::chunk_slots; chunk++) {
                const ContextChunk &held = chunks_[slot][chunk];
                if (!held.bound()) continue;
                writer.open("chunk", chunk);
                writer.number("resource", held.resource_id());
                if (held.in_window()) {
                    writer.number("window_page", held.first_page());
                    writer.number("pages", held.pages());
                } else {
                    writer.number("ram_bytes", held.ram()->bytes());
                }
                writer.close();
            }
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
        if (!bound_slot && (bindings_[slot].constructed() || memories_[slot].constructed() || names_[slot] != nullptr))
            return false;
        if (bound_slot && names_[slot] == nullptr) return false;
        if (memories_[slot].constructed() && !memories_[slot].object().invariants_hold()) return false;
        for (const ContextChunk &chunk : chunks_[slot])
            if (!chunk.invariants_hold() || (chunk.bound() && !bound_slot)) return false;
    }
    return true;
}

}  // namespace av
