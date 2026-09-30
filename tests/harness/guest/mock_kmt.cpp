#include "harness/guest/mock_kmt.hpp"

#include "core/ddi/dxgkarg_doorbell_records.hpp"

#include "core/support/nt_status.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/umd_adapter_info.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/reply_header.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/context/context_views_record.hpp"
#include "proto/resources/host_window_record.hpp"

#include <sys/mman.h>

namespace av {

namespace {

constexpr std::uint32_t first_handle = 0x40000000u;
constexpr std::uint32_t handle_step = 4u;

}  // namespace

MockKmt::MockKmt()
    : identities_{}, versions_{}, luids_{}, budgets_{default_local_budget, default_non_local_budget}, usages_{},
      adapter_count_(0u), claimed_(0u), claim_(false), failures_{}, countdown_{}, failing_run_{},
      zeroing_{}, calls_{}, objects_{}, views_{}, locked_{}, large_views_{}, large_bytes_{}, escape_{},
      escape_size_(0u), escape_answer_{}, escape_answer_size_(0u), escape_scripted_(false), adapter_escapes_(nullptr), window_(nullptr), window_pages_(0u), window_chunks_{}, window_opens_(0u), window_refusal_(0u), play_countdown_(0u), play_(nullptr), play_context_(nullptr), private_{},
      private_size_(0u), submission_{}, submission_size_(0u), shared_{},
      next_shared_handle_(first_shared_handle),
      next_object_id_(first_object_id), scribble_record_(false), keep_record_(false), short_record_(false), carried_next_(),
      carried_next_size_(0u), carry_next_(false), next_handle_(first_handle),
      next_address_(first_address), next_fence_(0u), success_code_(0u), last_code_(0u), misuses_(0u), ticks_(0u), read_ticks_(0u), idle_reads_(0u), clock_step_(0u), clock_reader_(nullptr),
      blank_period_(ticks_per_second / 60u), slip_nth_(0u), slip_ticks_(0u), wait_cost_(0u), timeouts_while_consuming_(0u), submit_cost_(0u), slow_nth_(0u), slow_ticks_(0u), frequency_(ticks_per_second),
      answer_{}, answer_size_(0u), answering_(false), queued_{}, queued_sizes_{}, queued_count_(0u),
      queued_taken_(0u), withheld_countdown_(0u), hardware_scheduling_(false), doorbell_page_missing_(0u), doorbells_{},
      doorbell_count_(0u), queue_progress_{}, queue_progress_taken_(0u), journal_ids_{}, journal_sizes_{}, journal_bytes_{},
      journal_count_(0u) {}

/* ---- feature 049: hardware queues and doorbells -------------------------- */

Status MockKmt::create_hw_queue(std::uint32_t device, std::uint32_t context, GuestHwQueue &queue)
{
    if (!begin(Call::create_hw_queue)) return refuse(last_code_);
    queue = GuestHwQueue{};
    const Object *const on = find(Kind::context, context);
    if (on == nullptr || on->owner != device) return misuse();
    /* Hardware scheduling is the OS's decision, not the driver's
     * (049 research.md §11): a machine without it gives out no queue, and the
     * transport submits the way it always has. */
    if (!hardware_scheduling_) return refuse(not_supported);
    /* Where dxgkrnl writes what this queue has finished, read-only to the
     * process, as the real mapping is; the monitored fence it is reported on
     * is named by the queue itself here. */
    const std::uint32_t slot = queue_progress_taken_++ % max_doorbells;
    const std::uint32_t handle = open(Kind::hw_queue, context, slot, 0u);
    queue = GuestHwQueue(handle, handle, &queue_progress_[slot]);
    return succeed();
}

/* What a hardware-scheduled context submits through: the same batch the
 * kernel path carries, and the progress fence value the queue reaches when the
 * device has run it (feature 049, research.md §13). */
Status MockKmt::submit_to_hw_queue(std::uint32_t queue, std::uint64_t commands, std::uint64_t length,
                                   const std::uint8_t *private_data, std::uint32_t private_size,
                                   std::uint64_t progress_fence_id)
{
    if (!begin(Call::submit_to_hw_queue)) return refuse(last_code_);
    ticks_ += submit_cost_;
    const Object *const on = find(Kind::hw_queue, queue);
    const Object *const context = on != nullptr ? find(Kind::context, on->owner) : nullptr;
    if (context == nullptr || progress_fence_id == 0u || (private_data == nullptr) != (private_size == 0u))
        return misuse();
    return carry(*context, commands, length, private_data, private_size);
}

/* Until the queue's progress fence reaches `value`: the device runs what was
 * queued while the process waits, as it does for a wait on a context. */
Status MockKmt::wait_for_hw_queue(std::uint32_t device, std::uint32_t progress_fence, std::uint64_t value)
{
    if (!begin(Call::wait_for_hw_queue)) return refuse(last_code_);
    Object *const on = find(Kind::hw_queue, progress_fence);
    Object *const context = on != nullptr ? find(Kind::context, on->owner) : nullptr;
    if (find(Kind::device, device) == nullptr || context == nullptr || context->owner != device) return misuse();
    ticks_ += wait_cost_;
    if (played()) play_(play_context_);
    take_doorbell_store(*context);
    if (withheld()) context->lost = true;
    else write_answer(*context);
    consume(*context);
    /* The driver writes what the device finished where the process reads it. */
    queue_progress_[on->address % max_doorbells] = value;
    return succeed();
}

Status MockKmt::destroy_hw_queue(std::uint32_t queue)
{
    if (!begin(Call::destroy_hw_queue)) return refuse(last_code_);
    Object *const found = find(Kind::hw_queue, queue);
    if (found == nullptr) return misuse();
    /* A queue destroyed takes its doorbells, as dxgkrnl's does. */
    for (std::uint32_t index = 0u; index < doorbell_count_; index++)
        if (doorbells_[index].queue == queue) doorbells_[index].handle = 0u;
    found->kind = Kind::none;
    return succeed();
}

Status MockKmt::create_doorbell(std::uint32_t queue, std::uint32_t ring, std::uint32_t &doorbell, void *&page,
                               const std::uint32_t *&status, std::uint64_t *&queued)
{
    if (!begin(Call::create_doorbell)) return refuse(last_code_);
    doorbell = 0u;
    page = nullptr;
    status = nullptr;
    queued = nullptr;
    const Object *const on = find(Kind::hw_queue, queue);
    if (on == nullptr || find(Kind::allocation, ring) == nullptr) return misuse();
    if (doorbell_count_ == max_doorbells) return refuse(invalid_parameter);
    DoorbellPages &made = doorbells_[doorbell_count_++];
    made = DoorbellPages{};
    made.handle = open(Kind::doorbell, queue, 0u, 0u);
    made.queue = queue;
    /* Created is not connected: what a process reads says to submit the other
     * way until it connects. */
    made.status = static_cast<std::uint32_t>(DoorbellStatus::disconnected_retry);
    doorbell = made.handle;
    if (doorbell_page_missing_ != 1u) page = &made.page;
    if (doorbell_page_missing_ != 2u) status = &made.status;
    if (doorbell_page_missing_ != 3u) queued = &made.queued;
    return succeed();
}

Status MockKmt::connect_doorbell(std::uint32_t doorbell)
{
    if (!begin(Call::connect_doorbell)) return refuse(last_code_);
    DoorbellPages *const found = doorbell_of(doorbell);
    if (found == nullptr) return misuse();
    found->connected = true;
    found->status = static_cast<std::uint32_t>(DoorbellStatus::connected);
    return succeed();
}

Status MockKmt::destroy_doorbell(std::uint32_t doorbell)
{
    if (!begin(Call::destroy_doorbell)) return refuse(last_code_);
    DoorbellPages *const found = doorbell_of(doorbell);
    if (found == nullptr) return misuse();
    Object *const object = find(Kind::doorbell, doorbell);
    if (object != nullptr) object->kind = Kind::none;
    found->handle = 0u;
    return succeed();
}

Status MockKmt::notify_work_submission(std::uint32_t doorbell)
{
    if (!begin(Call::notify_work_submission)) return refuse(last_code_);
    return doorbell_of(doorbell) != nullptr ? succeed() : misuse();
}

void MockKmt::allow_hardware_scheduling()
{
    hardware_scheduling_ = true;
}

void MockKmt::answer_doorbell_without_pages(std::uint32_t which)
{
    doorbell_page_missing_ = which;
}

void MockKmt::refuse_hardware_scheduling()
{
    hardware_scheduling_ = false;
}

void MockKmt::set_doorbell_status(std::uint32_t doorbell, std::uint32_t status)
{
    DoorbellPages *const found = doorbell_of(doorbell);
    if (found != nullptr) found->status = status;
}

std::uint32_t MockKmt::doorbell_page(std::uint32_t doorbell) const
{
    for (std::uint32_t index = 0u; index < doorbell_count_; index++)
        if (doorbells_[index].handle == doorbell) return doorbells_[index].page;
    return 0u;
}

void MockKmt::clear_doorbell_page(std::uint32_t doorbell)
{
    DoorbellPages *const found = doorbell_of(doorbell);
    if (found != nullptr) found->page = 0u;
}

std::uint64_t MockKmt::queued_fence(std::uint32_t doorbell) const
{
    for (std::uint32_t index = 0u; index < doorbell_count_; index++)
        if (doorbells_[index].handle == doorbell) return doorbells_[index].queued;
    return 0u;
}

std::uint32_t MockKmt::doorbell_handle(std::uint32_t index) const
{
    return index < doorbell_count_ ? doorbells_[index].handle : 0u;
}

MockKmt::DoorbellPages *MockKmt::doorbell_of(std::uint32_t handle)
{
    for (std::uint32_t index = 0u; index < doorbell_count_; index++)
        if (doorbells_[index].handle == handle && handle != 0u) return &doorbells_[index];
    return nullptr;
}

MockKmt::~MockKmt()
{
    for (std::uint32_t slot = 0u; slot < max_locks; slot++)
        if (large_views_[slot] != nullptr) munmap(large_views_[slot], large_bytes_[slot]);
    for (Object &object : objects_) unmap_context_memory(object);
    for (WindowChunk &chunk : window_chunks_) clear_chunk(chunk);
    if (window_ != nullptr) munmap(window_, window_pages_ * page_bytes);
}

bool MockKmt::begin(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    calls_[index]++;
    idle_reads_ = 0u;
    success_code_ = 0u;
    if (failing_run_[index] != 0u) {
        failing_run_[index]--;
        last_code_ = failures_[index];
        if (!NtStatus(failures_[index]).succeeded()) return false;
        success_code_ = failures_[index];
        return true;
    }
    if (countdown_[index] == 0u || --countdown_[index] != 0u) return true;
    last_code_ = failures_[index];
    if (!NtStatus(failures_[index]).succeeded()) return false;
    success_code_ = failures_[index];
    return true;
}

Status MockKmt::refuse(std::uint32_t code)
{
    last_code_ = code;
    return NtStatus(code).status();
}

Status MockKmt::succeed()
{
    last_code_ = success_code_;
    return Status::ok;
}

Status MockKmt::misuse()
{
    misuses_++;
    return refuse(invalid_parameter);
}

bool MockKmt::zeroed(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    const bool     counted = zero_countdown_[index] != 0u && --zero_countdown_[index] == 0u;
    const bool     zero = zeroing_[index] || counted;
    zeroing_[index] = false;
    return zero;
}

std::uint32_t MockKmt::open(Kind kind, std::uint32_t owner, std::uint64_t address, std::uint64_t pages)
{
    for (Object &object : objects_) {
        if (object.kind != Kind::none) continue;
        object = Object{kind, next_handle_, owner, address, pages, 0u, 0u, false, false, {}, 0u, false};
        next_handle_ += handle_step;
        return object.handle;
    }
    return 0u;
}

MockKmt::Object *MockKmt::find(Kind kind, std::uint32_t handle)
{
    for (Object &object : objects_)
        if (object.kind == kind && object.handle == handle) return &object;
    return nullptr;
}

MockKmt::Object *MockKmt::find_mapping(std::uint64_t address)
{
    for (Object &object : objects_)
        if (object.kind == Kind::mapping && address >= object.address &&
            address - object.address < object.pages * page_bytes)
            return &object;
    return nullptr;
}

Status MockKmt::close(Kind kind, std::uint32_t handle)
{
    Object *const object = find(kind, handle);
    if (object == nullptr) return misuse();
    object->kind = Kind::none;
    return succeed();
}

Status MockKmt::enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count)
{
    count = 0u;
    if (!begin(Call::enumerate_adapters)) return refuse(last_code_);
    const std::uint32_t claimed = claim_ ? claimed_ : adapter_count_;
    claim_ = false;
    const bool zero = zeroed(Call::enumerate_adapters);
    for (std::uint32_t index = 0u; index < claimed && index < capacity; index++)
        adapters[index] = zero && index == 0u ? 0u : open(Kind::adapter, index, 0u, 0u);
    count = claimed;
    return succeed();
}

Status MockKmt::query_identity(std::uint32_t adapter, PciIdentity &identity)
{
    if (!begin(Call::query_identity)) return refuse(last_code_);
    const Object *const object = find(Kind::adapter, adapter);
    if (object == nullptr) return misuse();
    identity = object->owner < adapter_count_ ? identities_[object->owner] : PciIdentity();
    return succeed();
}

Status MockKmt::query_driver_version(std::uint32_t adapter, std::uint32_t &version)
{
    if (!begin(Call::query_driver_version)) return refuse(last_code_);
    const Object *const object = find(Kind::adapter, adapter);
    if (object == nullptr) return misuse();
    version = object->owner < adapter_count_ ? versions_[object->owner] : 0u;
    return succeed();
}

Status MockKmt::close_adapter(std::uint32_t adapter)
{
    if (!begin(Call::close_adapter)) return refuse(last_code_);
    return close(Kind::adapter, adapter);
}

Status MockKmt::adapter_luid(std::uint32_t adapter, std::uint64_t &luid)
{
    luid = 0u;
    if (!begin(Call::adapter_luid)) return refuse(last_code_);
    const Object *const object = find(Kind::adapter, adapter);
    if (object == nullptr) return misuse();
    if (!zeroed(Call::adapter_luid) && object->owner < adapter_count_) luid = luids_[object->owner];
    return succeed();
}

Status MockKmt::query_video_memory(std::uint32_t adapter, MemorySegmentGroup group, std::uint64_t &budget,
                                   std::uint64_t &usage)
{
    budget = 0u;
    usage = 0u;
    if (!begin(Call::query_video_memory)) return refuse(last_code_);
    if (find(Kind::adapter, adapter) == nullptr) return misuse();
    const auto index = static_cast<std::uint32_t>(group);
    if (index >= segment_groups) return refuse(invalid_parameter);
    if (zeroed(Call::query_video_memory)) return succeed();
    budget = budgets_[index];
    usage = usages_[index];
    return succeed();
}

void MockKmt::set_adapter_luid(std::uint32_t index, std::uint64_t luid)
{
    if (index < max_adapters) luids_[index] = luid;
}

void MockKmt::set_video_memory(MemorySegmentGroup group, std::uint64_t budget, std::uint64_t usage)
{
    const auto index = static_cast<std::uint32_t>(group);
    if (index >= segment_groups) return;
    budgets_[index] = budget;
    usages_[index] = usage;
}

Status MockKmt::create_device(std::uint32_t adapter, std::uint32_t &device)
{
    device = 0u;
    if (!begin(Call::create_device)) return refuse(last_code_);
    if (find(Kind::adapter, adapter) == nullptr) return misuse();
    if (!zeroed(Call::create_device)) device = open(Kind::device, adapter, 0u, 0u);
    return succeed();
}

Status MockKmt::destroy_device(std::uint32_t device)
{
    if (!begin(Call::destroy_device)) return refuse(last_code_);
    for (const Object &object : objects_)
        if (object.kind != Kind::none && object.kind != Kind::mapping && object.owner == device) return misuse();
    return close(Kind::device, device);
}

Status MockKmt::create_allocation(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                                  std::uint32_t &allocation)
{
    allocation = 0u;
    if (!begin(Call::create_allocation)) return refuse(last_code_);
    if (find(Kind::device, device) == nullptr) return misuse();
    AllocationDescriptor decoded;
    /* Either length, as the real one takes either: the descriptor is the first
     * 48 bytes and a record may follow it. What this one cannot do is hand the
     * minted id back - its buffer is const, and the real kernel copies the
     * caller's bytes before the driver writes into them, so a caller that
     * wants the id calls create_shared_allocation instead (feature 053). */
    const std::uint32_t shareable =
        AllocationDescriptor::encoded_size + static_cast<std::uint32_t>(SharedObjectRecord::encoded_size);
    /* A length the driver has no shape for is not_supported; data of a length
     * it knows but cannot read is invalid_parameter. The miniport tells them
     * apart and so must this, or a guest sweep cannot say whether dxgkrnl
     * refused a create before the driver saw it (feature 053). */
    if (size != AllocationDescriptor::encoded_size && size != shareable) return refuse(not_supported);
    if (decoded.decode(descriptor, AllocationDescriptor::encoded_size) != Status::ok)
        return refuse(invalid_parameter);
    if (!zeroed(Call::create_allocation))
        allocation = open(Kind::allocation, device, 0u, (decoded.size() + page_bytes - 1u) / page_bytes);
    Object *const created = find(Kind::allocation, allocation);
    if (created != nullptr) {
        created->allocation_kind = static_cast<std::uint16_t>(decoded.kind());
        /* Kept as the runtime keeps it: these are the bytes dxgkrnl hands the
         * opening process, and nothing else crosses with the allocation. The
         * miniport mints into *this* copy, which is why a one-way caller never
         * sees an id. */
        created->created_size = size;
        for (std::uint32_t index = 0u; index < size; index++) created->created_with[index] = descriptor[index];
        if (size == shareable && !keep_record_) {
            SharedObjectRecord record;
            if (record.decode(created->created_with + AllocationDescriptor::encoded_size,
                              SharedObjectRecord::encoded_size) == Status::ok &&
                !record.assigned()) {
                record.assign(next_object_id_, shared_adapter);
                next_object_id_++;
                static_cast<void>(record.encode(created->created_with + AllocationDescriptor::encoded_size,
                                                SharedObjectRecord::encoded_size));
            }
        }
    }
    return succeed();
}

/* The miniport's half, as MockKmt plays it: a create that carries an unassigned
 * record has an id and an adapter number minted into the caller's own bytes,
 * which is what the opening process is later handed (feature 053). */
/* The carrier is one object standing for a resource and the single allocation
 * in it, because nothing here ever needs them apart: what a create answers, a
 * share names and a destroy takes is the same number (research.md §16). */
Status MockKmt::create_shared_allocation(std::uint32_t device, std::uint8_t *private_data, std::uint32_t size,
                                         std::uint32_t &resource)
{
    std::uint32_t &allocation = resource;
    allocation = 0u;
    if (!begin(Call::create_shared_allocation)) return refuse(last_code_);
    if (find(Kind::device, device) == nullptr) return misuse();
    const std::uint32_t shareable =
        AllocationDescriptor::encoded_size + static_cast<std::uint32_t>(SharedObjectRecord::encoded_size);
    if (private_data == nullptr || size != shareable) return refuse(invalid_parameter);
    AllocationDescriptor decoded;
    SharedObjectRecord   record;
    if (decoded.decode(private_data, AllocationDescriptor::encoded_size) != Status::ok ||
        record.decode(private_data + AllocationDescriptor::encoded_size, SharedObjectRecord::encoded_size) !=
            Status::ok)
        return refuse(invalid_parameter);
    /* A record that already names an object is one the miniport would refuse:
     * a creator can know neither number. Its size is the *object's* and need
     * not be the allocation's - an ICD sharing host memory mints over a
     * one-page carrier (feature 053, T031). */
    if (record.assigned()) return refuse(invalid_parameter);
    if (scribble_record_) {
        scribble_record_ = false;
        for (std::uint32_t index = AllocationDescriptor::encoded_size; index < size; index++)
            private_data[index] = 0xFFu;
        return succeed();
    }
    if (!zeroed(Call::create_shared_allocation))
        allocation = open(Kind::allocation, device, 0u, (decoded.size() + page_bytes - 1u) / page_bytes);
    Object *const created = find(Kind::allocation, allocation);
    if (created == nullptr) return succeed();
    record.assign(next_object_id_, shared_adapter);
    next_object_id_++;
    if (record.encode(private_data + AllocationDescriptor::encoded_size, SharedObjectRecord::encoded_size) !=
        Status::ok)
        return refuse(invalid_parameter);
    created->allocation_kind = static_cast<std::uint16_t>(decoded.kind());
    created->created_size = size;
    created->resource = true;
    for (std::uint32_t index = 0u; index < size; index++) created->created_with[index] = private_data[index];
    return succeed();
}

MockKmt::SharedHandle *MockKmt::shared_row(std::uint64_t handle)
{
    if (handle == 0u) return nullptr;
    for (SharedHandle &row : shared_)
        if (row.handle == handle) return &row;
    return nullptr;
}

Status MockKmt::share_object(std::uint32_t device, std::uint32_t resource, std::uint64_t &handle)
{
    handle = 0u;
    if (!begin(Call::share_object)) return refuse(last_code_);
    const std::uint32_t allocation = resource;
    const Object *const object = find(Kind::allocation, allocation);
    if (object == nullptr || object->owner != device) return misuse();
    /* An allocation created without asking for an identity is not a resource,
     * and D3DKMTShareObjects takes nothing else: the handle type check falls
     * through to STATUS_INVALID_PARAMETER with no message at all
     * (research.md §16). */
    if (!object->resource) return refuse(invalid_parameter);
    for (SharedHandle &row : shared_) {
        if (row.handle != 0u) continue;
        row.handle = next_shared_handle_;
        next_shared_handle_ += 4u;
        row.allocation = allocation;
        row.carried_size = carry_next_ ? carried_next_size_ : object->created_size;
        for (std::uint32_t index = 0u; index < row.carried_size; index++)
            row.carried[index] = carry_next_ ? carried_next_[index] : object->created_with[index];
        carry_next_ = false;
        handle = row.handle;
        return succeed();
    }
    return refuse(invalid_parameter);
}

Status MockKmt::destroy_shared_allocation(std::uint32_t device, std::uint32_t resource)
{
    if (!begin(Call::destroy_shared_allocation)) return refuse(last_code_);
    if (resource == 0u) return refuse(invalid_parameter);
    const Object *const object = find(Kind::allocation, resource);
    if (object == nullptr || object->owner != device) return misuse();
    /* hResource takes a handle of the resource type and nothing else: an
     * ordinary allocation named here is the same mismatch the other way, and
     * is refused having destroyed nothing (research.md §17). */
    if (!object->resource) return refuse(invalid_parameter);
    return close(Kind::allocation, resource);
}

Status MockKmt::open_shared_object(std::uint32_t device, std::uint64_t handle, std::uint32_t &resource,
                                   std::uint8_t *private_data, std::uint32_t capacity, std::uint32_t &written)
{
    resource = 0u;
    written = 0u;
    if (!begin(Call::open_shared_object)) return refuse(last_code_);
    if (find(Kind::device, device) == nullptr) return misuse();
    /* A handle this mock never minted is a handle from nowhere, which is the
     * case the whole table exists for. */
    const SharedHandle *const row = shared_row(handle);
    if (row == nullptr) return refuse(not_found);
    if (private_data == nullptr || row->carried_size > capacity) return refuse(invalid_parameter);
    const Object *const created = find(Kind::allocation, row->allocation);
    if (created == nullptr) return refuse(not_found);
    const std::uint16_t kind = created->allocation_kind;
    const std::uint64_t pages = created->pages;
    const std::uint32_t opened = open(Kind::allocation, device, 0u, pages);
    if (opened == 0u) return refuse(invalid_parameter);
    Object *const made = find(Kind::allocation, opened);
    made->allocation_kind = kind;
    made->created_size = row->carried_size;
    /* D3DKMTOpenResourceFromNtHandle answers a resource of this process, which
     * is what the opener gives back. */
    made->resource = true;
    for (std::uint32_t index = 0u; index < row->carried_size; index++) {
        made->created_with[index] = row->carried[index];
        private_data[index] = row->carried[index];
    }
    written = row->carried_size;
    if (short_record_) {
        short_record_ = false;
        written = AllocationDescriptor::encoded_size;
    }
    resource = opened;
    return succeed();
}

Status MockKmt::close_shared_object(std::uint64_t handle)
{
    if (!begin(Call::close_shared_object)) return refuse(last_code_);
    SharedHandle *const row = shared_row(handle);
    if (row == nullptr) return refuse(not_found);
    *row = SharedHandle{};
    return succeed();
}

Status MockKmt::destroy_allocation(std::uint32_t device, std::uint32_t allocation)
{
    if (!begin(Call::destroy_allocation)) return refuse(last_code_);
    const Object *const object = find(Kind::allocation, allocation);
    if (object == nullptr || object->owner != device || is_locked(allocation)) return misuse();
    /* The list D3DKMTDestroyAllocation2 walks holds allocation handles: a
     * resource named in it is a handle type mismatch, refused with nothing
     * destroyed, and the caller wanted destroy_shared_allocation
     * (research.md §17). */
    if (object->resource) return refuse(invalid_parameter);
    for (const Object &mapping : objects_)
        if (mapping.kind == Kind::mapping && mapping.owner == allocation) return misuse();
    return close(Kind::allocation, allocation);
}

/* Several allocations can be locked at once, each its own view; a second lock of
 * the same one, or more than this mock owns, is refused. */
Status MockKmt::lock(std::uint32_t device, std::uint32_t allocation, void *&view)
{
    view = nullptr;
    if (!begin(Call::lock)) return refuse(last_code_);
    const Object *const object = find(Kind::allocation, allocation);
    if (object == nullptr || object->owner != device) return misuse();
    for (std::uint32_t slot = 0u; slot < max_locks; slot++)
        if (locked_[slot] == allocation) return misuse();   /* already locked */
    if (zeroed(Call::lock)) return succeed();
    for (std::uint32_t slot = 0u; slot < max_locks; slot++)
        if (locked_[slot] == 0u) {
            const std::uint64_t bytes = object->pages * page_bytes;
            if (bytes > view_capacity) {
                /* Anonymous pages are zero and cost nothing until touched, so a
                 * 64 MiB chunk's view is only what a test writes into it. */
                void *const mapped = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (mapped == MAP_FAILED) return refuse(invalid_parameter);
                large_views_[slot] = static_cast<std::uint8_t *>(mapped);
                large_bytes_[slot] = bytes;
            }
            locked_[slot] = allocation;
            view = view_of(allocation);
            return succeed();
        }
    return misuse();   /* more views held than this mock owns */
}

/* The view keeps what was written through it, as the host's memory does. */
Status MockKmt::unlock(std::uint32_t device, std::uint32_t allocation)
{
    if (!begin(Call::unlock)) return refuse(last_code_);
    const Object *const object = find(Kind::allocation, allocation);
    if (object == nullptr || object->owner != device) return misuse();
    for (std::uint32_t slot = 0u; slot < max_locks; slot++)
        if (locked_[slot] == allocation) {
            locked_[slot] = 0u;
            if (large_views_[slot] != nullptr) {
                for (std::uint64_t at = 0u; at < large_bytes_[slot]; at++)
                    released_filled_ += large_views_[slot][at] != 0u ? 1u : 0u;
                munmap(large_views_[slot], large_bytes_[slot]);
            }
            large_views_[slot] = nullptr;
            large_bytes_[slot] = 0u;
            return succeed();
        }
    return misuse();
}

Status MockKmt::create_paging_queue(std::uint32_t device, std::uint32_t &queue, std::uint32_t &fence_object)
{
    queue = 0u;
    fence_object = 0u;
    if (!begin(Call::create_paging_queue)) return refuse(last_code_);
    if (find(Kind::device, device) == nullptr) return misuse();
    if (!zeroed(Call::create_paging_queue)) {
        fence_object = next_handle_;
        next_handle_ += handle_step;
    }
    queue = open(Kind::queue, device, fence_object, 0u);
    return succeed();
}

Status MockKmt::destroy_paging_queue(std::uint32_t queue)
{
    if (!begin(Call::destroy_paging_queue)) return refuse(last_code_);
    return close(Kind::queue, queue);
}

Status MockKmt::make_resident(std::uint32_t queue, std::uint32_t allocation, std::uint64_t &paging_fence)
{
    paging_fence = 0u;
    if (!begin(Call::make_resident)) return refuse(last_code_);
    const Object *const paging = find(Kind::queue, queue);
    const Object *const object = find(Kind::allocation, allocation);
    if (paging == nullptr || object == nullptr || object->owner != paging->owner) return misuse();
    paging_fence = ++next_fence_;
    return succeed();
}

Status MockKmt::map_gpu_virtual_address(std::uint32_t queue, std::uint32_t allocation, std::uint64_t pages,
                                        std::uint64_t &address, std::uint64_t &paging_fence)
{
    address = 0u;
    paging_fence = 0u;
    if (!begin(Call::map_gpu_virtual_address)) return refuse(last_code_);
    const Object *const paging = find(Kind::queue, queue);
    const Object *const object = find(Kind::allocation, allocation);
    if (paging == nullptr || object == nullptr || object->owner != paging->owner) return misuse();
    if (pages == 0u) return refuse(invalid_parameter);
    if (zeroed(Call::map_gpu_virtual_address)) return succeed();
    address = next_address_;
    next_address_ += pages * page_bytes;
    static_cast<void>(open(Kind::mapping, allocation, address, pages));
    paging_fence = ++next_fence_;
    return succeed();
}

Status MockKmt::free_gpu_virtual_address(std::uint32_t adapter, std::uint64_t address, std::uint64_t pages)
{
    if (!begin(Call::free_gpu_virtual_address)) return refuse(last_code_);
    Object *const mapping = find_mapping(address);
    if (find(Kind::adapter, adapter) == nullptr || mapping == nullptr || mapping->address != address ||
        mapping->pages != pages)
        return misuse();
    mapping->kind = Kind::none;
    return succeed();
}

Status MockKmt::wait_for_fence(std::uint32_t device, std::uint32_t fence_object, std::uint64_t fence)
{
    if (!begin(Call::wait_for_fence)) return refuse(last_code_);
    if (find(Kind::device, device) == nullptr) return misuse();
    bool owned = false;
    for (const Object &object : objects_)
        owned = owned || (object.kind == Kind::queue && object.owner == device && object.address == fence_object);
    if (!owned) return misuse();
    if (fence > next_fence_) return refuse(io_timeout);
    return succeed();
}

Status MockKmt::create_context(std::uint32_t device, std::uint32_t &context)
{
    context = 0u;
    if (!begin(Call::create_context)) return refuse(last_code_);
    if (find(Kind::device, device) == nullptr) return misuse();
    if (!zeroed(Call::create_context)) context = open(Kind::context, device, 0u, 0u);
    return succeed();
}

Status MockKmt::create_context_virtual(std::uint32_t device, const std::uint8_t *private_data,
                                       std::uint32_t size, std::uint32_t &context)
{
    context = 0u;
    if (!begin(Call::create_context_virtual)) return refuse(last_code_);
    if (find(Kind::device, device) == nullptr || private_data == nullptr || size == 0u) return misuse();
    private_size_ = size < max_private ? size : max_private;
    for (std::uint32_t i = 0u; i < private_size_; i++) private_[i] = private_data[i];
    RvkContextRecord    record;
    const bool          decoded = record.decode(private_data, size) == Status::ok;
    if (!zeroed(Call::create_context_virtual)) context = open(Kind::context, device, 0u, 0u);
    Object *const created = find(Kind::context, context);
    if (created != nullptr && decoded) {
        created->ring = record.ring();
        map_context_memory(*created, record.ring_order(), record.reply_bytes());
    }
    return succeed();
}

/* A context a hardware queue may run on, which is what dxgkrnl gives only
 * where hardware scheduling is on (feature 049, research.md §13): refused here
 * otherwise, exactly as a machine without it refuses. */
Status MockKmt::create_hw_queue_context(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                        std::uint32_t &context)
{
    context = 0u;
    if (!begin(Call::create_hw_queue_context)) return refuse(last_code_);
    if (!hardware_scheduling_) return refuse(not_supported);
    const Status created = create_context_virtual(device, private_data, size, context);
    Object *const object = find(Kind::context, context);
    if (object != nullptr) object->hardware_scheduled = true;
    return created == Status::ok ? succeed() : created;
}

Status MockKmt::destroy_context(std::uint32_t context)
{
    if (!begin(Call::destroy_context)) return refuse(last_code_);
    Object *const destroyed = find(Kind::context, context);
    if (destroyed != nullptr) {
        close_window_chunks_of(context);
        unmap_context_memory(*destroyed);
    }
    return close(Kind::context, context);
}

Status MockKmt::submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length,
                       const std::uint8_t *private_data, std::uint32_t private_size)
{
    if (!begin(Call::submit)) return refuse(last_code_);
    ticks_ += submit_cost_;
    const Object *const submitting = find(Kind::context, context);
    if (submitting == nullptr || (private_data == nullptr) != (private_size == 0u)) return misuse();
    return carry(*submitting, commands, length, private_data, private_size);
}

/* The batch both submission paths carry: the descriptor is kept for the answer
 * a wait writes, and the command at its front is journalled. */
Status MockKmt::carry(const Object &context, std::uint64_t commands, std::uint64_t length,
                      const std::uint8_t *private_data, std::uint32_t private_size)
{
    submission_size_ = private_size < max_private ? private_size : max_private;
    for (std::uint32_t i = 0u; i < submission_size_; i++) submission_[i] = private_data[i];
    const Object *const mapping = find_mapping(commands);
    const Object *const allocation = mapping != nullptr ? find(Kind::allocation, mapping->owner) : nullptr;
    if (length == 0u || allocation == nullptr || allocation->owner != context.owner ||
        commands - mapping->address + length > mapping->pages * page_bytes)
        return refuse(invalid_parameter);
    journal(context);
    return succeed();
}

Status MockKmt::wait_for_context(std::uint32_t device, std::uint32_t context)
{
    if (!begin(Call::wait_for_context)) return refuse(last_code_);
    Object *const object = find(Kind::context, context);
    if (find(Kind::device, device) == nullptr || object == nullptr || object->owner != device) return misuse();
    ticks_ += wait_cost_;
    if (timeouts_while_consuming_ != 0u) {
        timeouts_while_consuming_--;
        consume(*object);
        return refuse(io_timeout);
    }
    if (slow_nth_ != 0u && calls_[static_cast<unsigned>(Call::wait_for_context)] % slow_nth_ == 0u) ticks_ += slow_ticks_;
    if (played()) play_(play_context_);
    take_doorbell_store(*object);
    if (withheld()) object->lost = true;
    else write_answer(*object);
    consume(*object);
    return succeed();
}

/* A store the process made to a doorbell of this context, which this mock
 * never saw made: the device does the work while the process waits, so the
 * wait is where it is noticed. What it queued is in the fence word the process
 * wrote, which is what the answer is written against (feature 049). */
void MockKmt::take_doorbell_store(const Object &context)
{
    for (DoorbellPages &doorbell : doorbells_) {
        if (doorbell.handle == 0u || doorbell.page == 0u) continue;
        const Object *const queue = find(Kind::hw_queue, doorbell.queue);
        if (queue == nullptr || queue->owner != context.handle) continue;
        doorbell.page = 0u;
        /* What the process published is what the device consumes: the ring's
         * own tail, since nothing of this went through a submission. */
        RingHeader                header;
        const std::uint8_t *const ring = context.ring_memory;
        if (ring == nullptr || header.decode(ring, RingHeader::encoded_size) != Status::ok ||
            header.tail() == 0u)
            return;
        submission_size_ = SubmissionDescriptor::encoded_size;
        static_cast<void>(
            SubmissionDescriptor(doorbell.queued, 0u, header.tail()).encode(submission_, submission_size_));
        return;
    }
}

/* A queued answer is taken by the wait that finds it whether or not the slot
 * can be written, so a script stays in step with the calls that awaited it. */
void MockKmt::write_answer(const Object &context)
{
    if (queued_taken_ < queued_count_) {
        const std::uint32_t index = queued_taken_++;
        write_bytes(context, queued_[index], queued_sizes_[index]);
        return;
    }
    if (answering_) write_bytes(context, answer_, answer_size_);
}

void MockKmt::write_bytes(const Object &context, const std::uint8_t *answer, std::uint32_t size)
{
    SubmissionDescriptor submitted;
    std::uint8_t *const  slot = context.reply_memory;
    if (slot == nullptr || submitted.decode(submission_, submission_size_) != Status::ok) return;
    static_cast<void>(ReplyHeader(submitted.fence_id(), size).encode(slot, ReplyHeader::encoded_size));
    for (std::uint32_t i = 0u; i < size; i++) slot[ReplyHeader::encoded_size + i] = answer[i];
}

/* Every command of every batch the last submission named, read from the ring
 * as the device would: a transport frames many commands into one batch. */
void MockKmt::journal(const Object &context)
{
    SubmissionDescriptor submitted;
    RingHeader           header;
    const std::uint8_t *const ring = context.ring_memory;
    if (ring == nullptr || submitted.decode(submission_, submission_size_) != Status::ok ||
        header.decode(ring, RingHeader::encoded_size) != Status::ok)
        return;
    const std::uint64_t mask = header.capacity() - 1u;
    const auto          copy = [&](std::uint64_t at, std::uint8_t *into, std::uint32_t bytes) {
        for (std::uint32_t i = 0u; i < bytes; i++) into[i] = ring[RingHeader::encoded_size + ((at + i) & mask)];
    };
    for (std::uint64_t batch_at = submitted.begin(); batch_at < submitted.end();) {
        std::uint8_t framing[BatchHeader::encoded_size] = {};
        copy(batch_at, framing, BatchHeader::encoded_size);
        BatchHeader batch;
        if (batch.decode(framing, sizeof framing) != Status::ok) return;
        const std::uint64_t first = batch_at + BatchHeader::encoded_size;
        for (std::uint64_t at = first; at < first + batch.length() && journal_count_ < max_journal;) {
            std::uint8_t *const leading = journal_bytes_[journal_count_];
            copy(at, leading, journal_bytes);
            CommandHeader command;
            if (command.decode(leading, CommandHeader::encoded_size) != Status::ok || command.size() == 0u) return;
            journal_ids_[journal_count_] = command.id();
            journal_sizes_[journal_count_] = command.size();
            journal_count_++;
            at += command.size();
        }
        batch_at += batch.span();
    }
}

/* Everything submitted is finished once the context is: the head moves to the
 * last submission's end, freeing the ring for the next. */
void MockKmt::consume(const Object &context)
{
    SubmissionDescriptor submitted;
    RingHeader           header;
    std::uint8_t *const  ring = context.ring_memory;
    if (ring == nullptr || submitted.decode(submission_, submission_size_) != Status::ok ||
        header.decode(ring, RingHeader::encoded_size) != Status::ok)
        return;
    static_cast<void>(RingHeader(header.order(), header.tail(), submitted.end()).encode(ring, RingHeader::encoded_size));
}

/* Time passes only at a vertical blank: the clock is what the scenario
 * measures, so nothing else may move it. */
Status MockKmt::wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source)
{
    if (!begin(Call::wait_for_vertical_blank)) return refuse(last_code_);
    const Object *const object = find(Kind::device, device);
    if (find(Kind::adapter, adapter) == nullptr || object == nullptr || object->owner != adapter || source != 0u)
        return misuse();
    ticks_ += blank_period_;
    if (slip_nth_ != 0u && calls_[static_cast<unsigned>(Call::wait_for_vertical_blank)] % slip_nth_ == 0u)
        ticks_ += slip_ticks_;
    return succeed();
}

Status MockKmt::scan_line(std::uint32_t adapter, std::uint32_t source, BeamPosition &position)
{
    if (!begin(Call::scan_line)) return refuse(last_code_);
    if (find(Kind::adapter, adapter) == nullptr || source != 0u || blank_period_ == 0u) return misuse();
    const std::uint64_t into = (ticks_ + read_ticks_) % blank_period_ * beam_total_lines / blank_period_;
    const auto          line = static_cast<std::uint32_t>((beam_height + into) % beam_total_lines);
    position = BeamPosition(line >= beam_height, line);
    return succeed();
}

Status MockKmt::escape(std::uint32_t adapter, std::uint32_t device, std::uint32_t context, std::uint8_t *data,
                       std::uint32_t size)
{
    if (!begin(Call::escape)) return refuse(last_code_);
    if (device == 0u && context == 0u) {
        if (find(Kind::adapter, adapter) == nullptr || data == nullptr || size == 0u) return misuse();
        escape_size_ = size < max_private ? size : max_private;
        for (std::uint32_t i = 0u; i < escape_size_; i++) escape_[i] = data[i];
        if (adapter_escapes_ == nullptr) return refuse(invalid_parameter);
        const std::uint32_t code = adapter_escapes_->answer(data, size);
        return code == 0u ? succeed() : refuse(code);
    }
    const Object *const owner = find(Kind::device, device);
    const Object *const on = find(Kind::context, context);
    if (find(Kind::adapter, adapter) == nullptr || owner == nullptr || owner->owner != adapter || on == nullptr ||
        on->owner != device || data == nullptr || size == 0u)
        return misuse();
    escape_size_ = size < max_private ? size : max_private;
    for (std::uint32_t i = 0u; i < escape_size_; i++) escape_[i] = data[i];
    if (zeroed(Call::escape)) return succeed();
    if (on->lost) return refuse(invalid_parameter);
    if (escape_scripted_) {
        escape_scripted_ = false;
        for (std::uint32_t i = 0u; i < escape_answer_size_ && i < size; i++) data[i] = escape_answer_[i];
        return succeed();
    }
    std::uint32_t magic = 0u;
    for (std::uint32_t i = 0u; i < sizeof magic && i < size; i++) magic |= std::uint32_t{data[i]} << (8u * i);
    if (magic == ContextViewsRecord::magic) return context_views(context, data, size);
    if (magic == HostWindowRecord::magic) return window_chunk(context, data, size);
    return refuse(not_supported);
}

Status MockKmt::context_views(std::uint32_t context, std::uint8_t *data, std::uint32_t size)
{
    const Object *const on = find(Kind::context, context);
    ContextViewsRecord  record;
    if (on == nullptr || on->ring_memory == nullptr || record.decode(data, size) != Status::ok)
        return refuse(invalid_parameter);
    static_cast<void>(record
                          .answered(reinterpret_cast<std::uintptr_t>(on->ring_memory),
                                    reinterpret_cast<std::uintptr_t>(on->reply_memory))
                          .encode(data, size));
    return succeed();
}

void MockKmt::map_context_memory(Object &context, std::uint8_t order, std::uint32_t reply_bytes)
{
    const std::uint64_t ring_bytes = (RingHeader::encoded_size + (std::uint64_t{1} << order) + page_bytes - 1u) &
                                     ~(page_bytes - 1u);
    const std::uint64_t slot_bytes = (std::uint64_t{reply_bytes} + page_bytes - 1u) & ~(page_bytes - 1u);
    void *const ring = mmap(nullptr, ring_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void *const reply = mmap(nullptr, slot_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ring == MAP_FAILED || reply == MAP_FAILED) {
        if (ring != MAP_FAILED) munmap(ring, ring_bytes);
        if (reply != MAP_FAILED) munmap(reply, slot_bytes);
        return;
    }
    context.ring_memory = static_cast<std::uint8_t *>(ring);
    context.ring_bytes = ring_bytes;
    context.reply_memory = static_cast<std::uint8_t *>(reply);
    context.reply_bytes = slot_bytes;
    static_cast<void>(RingHeader(order, 0u, 0u).encode(context.ring_memory, RingHeader::encoded_size));
}

void MockKmt::unmap_context_memory(Object &context)
{
    if (context.ring_memory != nullptr) munmap(context.ring_memory, context.ring_bytes);
    if (context.reply_memory != nullptr) munmap(context.reply_memory, context.reply_bytes);
    context.ring_memory = nullptr;
    context.ring_bytes = 0u;
    context.reply_memory = nullptr;
    context.reply_bytes = 0u;
}

std::uint8_t *MockKmt::ring_of(std::uint32_t context)
{
    const Object *const on = find(Kind::context, context);
    return on != nullptr ? on->ring_memory : nullptr;
}

std::uint8_t *MockKmt::ring_named(std::uint32_t name)
{
    for (const Object &object : objects_)
        if (object.kind == Kind::context && name != 0u && object.ring == name) return object.ring_memory;
    return nullptr;
}

std::uint8_t *MockKmt::reply_of(std::uint32_t context)
{
    const Object *const on = find(Kind::context, context);
    return on != nullptr ? on->reply_memory : nullptr;
}

/* Every refusal the driver makes of a record is INVALID_PARAMETER. */
Status MockKmt::window_chunk(std::uint32_t context, std::uint8_t *data, std::uint32_t size)
{
    HostWindowRecord record;
    if (record.decode(data, size) != Status::ok) return refuse(invalid_parameter);
    if (record.opens()) return open_window_chunk(context, data, size, record.bytes());
    for (WindowChunk &chunk : window_chunks_) {
        if (chunk.resource != record.resource_id() || chunk.context != context) continue;
        clear_chunk(chunk);
        return succeed();
    }
    return refuse(invalid_parameter);
}

void MockKmt::clear_chunk(WindowChunk &chunk)
{
    if (chunk.ram != nullptr) munmap(chunk.ram, chunk.pages * page_bytes);
    chunk = WindowChunk{};
}

Status MockKmt::open_window_chunk(std::uint32_t context, std::uint8_t *data, std::uint32_t size,
                                  std::uint64_t bytes)
{
    if (window_refusal_ != 0u) {
        const std::uint32_t code = window_refusal_;
        window_refusal_ = 0u;
        return refuse(code);
    }
    const std::uint64_t pages = (bytes + page_bytes - 1u) / page_bytes;
    const std::uint64_t first = bytes <= window_pages_ * page_bytes ? window_fit(pages) : window_pages_;
    const HostWindowRecord request(HostWindowRecord::Operation::open, bytes, 0u);
    for (WindowChunk &chunk : window_chunks_) {
        if (chunk.resource != 0u) continue;
        if (first != window_pages_) {
            chunk = WindowChunk{first_window_resource + window_opens_, context, first, pages};
            window_opens_++;
            const std::uint64_t address = reinterpret_cast<std::uintptr_t>(window_ + first * page_bytes);
            static_cast<void>(request.answered(chunk.resource, answered_bytes(pages), address).encode(data, size));
            return succeed();
        }
        /* Anonymous pages are zero and cost nothing until touched, so a 64 MiB
         * chunk is only what a test writes into it. */
        void *const ram = mmap(nullptr, pages * page_bytes, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (ram == MAP_FAILED) return refuse(invalid_parameter);
        chunk = WindowChunk{first_chunk_resource + ram_opens_, context, 0u, pages,
                            static_cast<std::uint8_t *>(ram), ram_opens_};
        ram_opens_++;
        const std::uint64_t address = reinterpret_cast<std::uintptr_t>(ram);
        static_cast<void>(request.answered(chunk.resource, answered_bytes(pages), address).encode(data, size));
        return succeed();
    }
    return refuse(invalid_parameter);
}

std::uint64_t MockKmt::window_fit(std::uint64_t pages) const
{
    std::uint64_t first = 0u;
    bool          moved = true;
    while (moved) {
        moved = false;
        for (const WindowChunk &chunk : window_chunks_) {
            if (chunk.resource == 0u || chunk.ram != nullptr || chunk.first >= first + pages ||
                chunk.first + chunk.pages <= first)
                continue;
            first = chunk.first + chunk.pages;
            moved = true;
        }
    }
    return window_pages_ - first >= pages ? first : window_pages_;
}

void MockKmt::close_window_chunks_of(std::uint32_t context)
{
    for (WindowChunk &chunk : window_chunks_)
        if (chunk.resource != 0u && chunk.context == context) clear_chunk(chunk);
}

void MockKmt::open_host_window(std::uint64_t bytes)
{
    if (window_ != nullptr) munmap(window_, window_pages_ * page_bytes);
    window_pages_ = (bytes + page_bytes - 1u) / page_bytes;
    void *const mapped = mmap(nullptr, window_pages_ * page_bytes, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    window_ = mapped == MAP_FAILED ? nullptr : static_cast<std::uint8_t *>(mapped);
    window_pages_ = window_ != nullptr ? window_pages_ : 0u;
    for (WindowChunk &chunk : window_chunks_) clear_chunk(chunk);
}

void MockKmt::refuse_next_window_open(std::uint32_t code) { window_refusal_ = code; }

std::uint32_t MockKmt::window_chunks() const
{
    std::uint32_t held = 0u;
    for (const WindowChunk &chunk : window_chunks_) held += chunk.resource != 0u && chunk.ram == nullptr ? 1u : 0u;
    return held;
}

std::uint32_t MockKmt::ram_chunks() const
{
    std::uint32_t held = 0u;
    for (const WindowChunk &chunk : window_chunks_) held += chunk.ram != nullptr ? 1u : 0u;
    return held;
}

std::uint64_t MockKmt::answered_bytes(std::uint64_t pages)
{
    const std::uint64_t bytes = chunk_bytes_answer_ != 0u ? chunk_bytes_answer_ : pages * page_bytes;
    chunk_bytes_answer_ = 0u;
    return bytes;
}

void MockKmt::answer_next_chunk_bytes(std::uint64_t bytes) { chunk_bytes_answer_ = bytes; }

std::uint64_t MockKmt::ram_chunk_pages(std::uint32_t index) const
{
    for (const WindowChunk &chunk : window_chunks_)
        if (chunk.ram != nullptr && chunk.ram_index == index) return chunk.pages;
    return 0u;
}

std::uint8_t *MockKmt::ram_chunk(std::uint32_t index) const
{
    for (const WindowChunk &chunk : window_chunks_)
        if (chunk.ram != nullptr && chunk.ram_index == index) return chunk.ram;
    return nullptr;
}

std::uint32_t MockKmt::window_opens() const { return window_opens_; }
std::uint8_t *MockKmt::window_base() const { return window_; }

bool MockKmt::played()
{
    if (play_countdown_ == 0u) return false;
    return --play_countdown_ == 0u;
}

void MockKmt::play_at_wait(std::uint32_t nth, void (*play)(void *context), void *context)
{
    play_countdown_ = nth;
    play_ = play;
    play_context_ = context;
}

void MockKmt::answer_next_escape(const std::uint8_t *answer, std::uint32_t size)
{
    escape_answer_size_ = size < max_private ? size : max_private;
    for (std::uint32_t i = 0u; i < escape_answer_size_; i++) escape_answer_[i] = answer[i];
    escape_scripted_ = true;
}

void MockKmt::answer_adapter_escapes(AdapterEscapes *driver)
{
    adapter_escapes_ = driver;
}

const std::uint8_t *MockKmt::last_escape() const { return escape_; }
std::uint32_t       MockKmt::last_escape_size() const { return escape_size_; }

std::uint64_t MockKmt::performance_counter(std::uint64_t &frequency) const
{
    frequency = frequency_;
    /* Told before the answer moves, so that what a reader does is seen by the
     * read that told it rather than by the one after. */
    if (clock_reader_ != nullptr) clock_reader_->clock_read();
    read_ticks_ += clock_step_;
    if (clock_step_ == 0u && ++idle_reads_ > spin_reads) {
        const std::uint64_t millisecond = frequency_ / 1000u;
        read_ticks_ += millisecond != 0u ? millisecond : 1u;
    }
    return ticks_ + read_ticks_;
}

void MockKmt::reads_clock(ClockReader *reader, std::uint64_t step)
{
    clock_reader_ = reader;
    clock_step_ = step;
}

void MockKmt::set_blank_period(std::uint64_t ticks) { blank_period_ = ticks; }

void MockKmt::answer_waits(const std::uint8_t *answer, std::uint32_t size)
{
    answer_size_ = size < max_answer ? size : max_answer;
    for (std::uint32_t i = 0u; i < answer_size_; i++) answer_[i] = answer[i];
    answering_ = true;
}

void MockKmt::queue_answer(const std::uint8_t *answer, std::uint32_t size)
{
    /* A script longer than this mock can hold would be answered by silence
     * halfway through, which reads as a driver failure: it is the test's own
     * mistake, and is recorded as one. */
    if (queued_count_ == max_queued) {
        misuses_++;
        return;
    }
    const std::uint32_t kept = size < max_answer ? size : max_answer;
    for (std::uint32_t i = 0u; i < kept; i++) queued_[queued_count_][i] = answer[i];
    queued_sizes_[queued_count_] = kept;
    queued_count_++;
}

std::uint32_t MockKmt::queued_answers() const { return queued_count_ - queued_taken_; }
std::uint32_t MockKmt::commands() const { return journal_count_; }
std::uint32_t MockKmt::command_id(std::uint32_t index) const { return index < journal_count_ ? journal_ids_[index] : 0u; }
const std::uint8_t *MockKmt::command_bytes(std::uint32_t index) const
{
    return index < journal_count_ ? journal_bytes_[index] : nullptr;
}

std::uint32_t MockKmt::command_size(std::uint32_t index) const
{
    return index < journal_count_ ? journal_sizes_[index] : 0u;
}

void MockKmt::spend_on_wait(std::uint64_t ticks) { wait_cost_ = ticks; }

void MockKmt::time_out_while_consuming(std::uint32_t count) { timeouts_while_consuming_ = count; }

void MockKmt::spend_on_submit(std::uint64_t ticks) { submit_cost_ = ticks; }

void MockKmt::slow_every(std::uint32_t nth, std::uint64_t ticks)
{
    slow_nth_ = nth;
    slow_ticks_ = ticks;
}
void MockKmt::set_frequency(std::uint64_t frequency) { frequency_ = frequency; }

void MockKmt::slip_every(std::uint32_t nth, std::uint64_t ticks)
{
    slip_nth_ = nth;
    slip_ticks_ = ticks;
}

std::uint32_t MockKmt::last_code() const { return last_code_; }

void MockKmt::add_adapter(const PciIdentity &identity, std::uint32_t driver_version)
{
    if (adapter_count_ == max_adapters) return;
    identities_[adapter_count_] = identity;
    versions_[adapter_count_] = driver_version;
    luids_[adapter_count_] = first_luid + adapter_count_;
    adapter_count_++;
}

void MockKmt::claim_adapters(std::uint32_t count)
{
    claimed_ = count;
    claim_ = true;
}

void MockKmt::fail_at(Call call, std::uint32_t nth, std::uint32_t code)
{
    countdown_[static_cast<unsigned>(call)] = nth;
    failures_[static_cast<unsigned>(call)] = code;
}

void MockKmt::fail_first(Call call, std::uint32_t count, std::uint32_t code)
{
    failing_run_[static_cast<unsigned>(call)] = count;
    failures_[static_cast<unsigned>(call)] = code;
}

void MockKmt::scribble_the_next_record() { scribble_record_ = true; }

void MockKmt::keep_the_next_record() { keep_record_ = true; }

void MockKmt::hand_back_a_short_record() { short_record_ = true; }

void MockKmt::carry_at_the_next_share(const std::uint8_t *bytes, std::uint32_t size)
{
    carried_next_size_ = size < max_allocation_private ? size : max_allocation_private;
    for (std::uint32_t index = 0u; index < carried_next_size_; index++) carried_next_[index] = bytes[index];
    carry_next_ = true;
}

void MockKmt::zero_next(Call call) { zeroing_[static_cast<unsigned>(call)] = true; }

void MockKmt::zero_at(Call call, std::uint32_t nth) { zero_countdown_[static_cast<unsigned>(call)] = nth; }

void MockKmt::withhold_answer_at(std::uint32_t nth) { withheld_countdown_ = nth; }

bool MockKmt::withheld()
{
    if (withheld_countdown_ == 0u) return false;
    return --withheld_countdown_ == 0u;
}

std::uint32_t MockKmt::calls(Call call) const { return calls_[static_cast<unsigned>(call)]; }

std::uint32_t MockKmt::open_objects() const
{
    std::uint32_t open = 0u;
    for (const Object &object : objects_) open += object.kind != Kind::none ? 1u : 0u;
    return open;
}

std::uint32_t MockKmt::misuses() const { return misuses_; }

std::uint32_t MockKmt::view_filled() const
{
    std::uint64_t filled = released_filled_;
    for (const auto &view : views_)
        for (const std::uint8_t byte : view) filled += byte != 0u ? 1u : 0u;
    return static_cast<std::uint32_t>(filled);
}

bool MockKmt::is_locked(std::uint32_t allocation) const
{
    for (std::uint32_t slot = 0u; slot < max_locks; slot++)
        if (locked_[slot] == allocation) return true;
    return false;
}

std::uint8_t *MockKmt::view_of(std::uint32_t allocation)
{
    for (std::uint32_t slot = 0u; slot < max_locks; slot++)
        if (locked_[slot] == allocation) return large_views_[slot] != nullptr ? large_views_[slot] : views_[slot];
    return nullptr;
}

const std::uint8_t *MockKmt::last_submission() const { return submission_; }
std::uint32_t       MockKmt::last_submission_size() const { return submission_size_; }

std::uint16_t MockKmt::allocation_kind(std::uint32_t allocation)
{
    const Object *const object = find(Kind::allocation, allocation);
    return object != nullptr ? object->allocation_kind : 0u;
}

std::uint64_t MockKmt::allocation_pages(std::uint32_t allocation)
{
    const Object *const object = find(Kind::allocation, allocation);
    return object != nullptr ? object->pages : 0u;
}

const std::uint8_t *MockKmt::last_private() const { return private_; }
std::uint32_t       MockKmt::last_private_size() const { return private_size_; }

Status MockKmt::query_adapter_features(std::uint32_t, UmdAdapterInfo &info)
{
    info = UmdAdapterInfo(0u, adapter_features_);
    return Status::ok;
}

void MockKmt::set_adapter_features(std::uint64_t features) { adapter_features_ = features; }

Status MockKmt::query_native_gpu_fences(std::uint32_t, bool &supported)
{
    supported = true;
    return Status::ok;
}

void MockKmt::sleep_briefly() {}

/* No display here: every search takes the first adapter it finds. */
Status MockKmt::primary_display_luid(std::uint64_t &answered)
{
    answered = 0u;
    return Status::not_supported;
}

}  // namespace av
