#include "harness/guest/mock_kmt.hpp"

#include "core/support/nt_status.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/reply_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

namespace av {

namespace {

constexpr std::uint32_t first_handle = 0x40000000u;
constexpr std::uint32_t handle_step = 4u;
/* What a wait on a fence no one will reach ends in. */
constexpr std::uint32_t io_timeout = 0xC00000B5u;

}  // namespace

MockKmt::MockKmt()
    : identities_{}, versions_{}, adapter_count_(0u), claimed_(0u), claim_(false), failures_{}, countdown_{},
      zeroing_{}, calls_{}, objects_{}, views_{}, locked_{}, private_{}, private_size_(0u), submission_{}, submission_size_(0u), next_handle_(first_handle),
      next_address_(first_address), next_fence_(0u), success_code_(0u), last_code_(0u), misuses_(0u), ticks_(0u),
      blank_period_(ticks_per_second / 60u), slip_nth_(0u), slip_ticks_(0u), wait_cost_(0u), slow_nth_(0u), slow_ticks_(0u), frequency_(ticks_per_second),
      answer_{}, answer_size_(0u), answering_(false), queued_{}, queued_sizes_{}, queued_count_(0u),
      queued_taken_(0u), withheld_countdown_(0u), journal_ids_{}, journal_sizes_{}, journal_bytes_{},
      journal_count_(0u) {}

bool MockKmt::begin(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    calls_[index]++;
    success_code_ = 0u;
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
    const bool zero = zeroing_[static_cast<unsigned>(call)];
    zeroing_[static_cast<unsigned>(call)] = false;
    return zero;
}

std::uint32_t MockKmt::open(Kind kind, std::uint32_t owner, std::uint64_t address, std::uint64_t pages)
{
    for (Object &object : objects_) {
        if (object.kind != Kind::none) continue;
        object = Object{kind, next_handle_, owner, address, pages, 0u, 0u};
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
    if (size != AllocationDescriptor::encoded_size || decoded.decode(descriptor, size) != Status::ok)
        return refuse(invalid_parameter);
    if (!zeroed(Call::create_allocation))
        allocation = open(Kind::allocation, device, 0u, (decoded.size() + page_bytes - 1u) / page_bytes);
    Object *const created = find(Kind::allocation, allocation);
    if (created != nullptr) created->allocation_kind = static_cast<std::uint16_t>(decoded.kind());
    return succeed();
}

Status MockKmt::destroy_allocation(std::uint32_t device, std::uint32_t allocation)
{
    if (!begin(Call::destroy_allocation)) return refuse(last_code_);
    const Object *const object = find(Kind::allocation, allocation);
    if (object == nullptr || object->owner != device || is_locked(allocation)) return misuse();
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
            locked_[slot] = allocation;
            view = views_[slot];
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
    if (!zeroed(Call::create_context_virtual)) context = open(Kind::context, device, decoded ? record.reply() : 0u, 0u);
    Object *const created = find(Kind::context, context);
    if (created != nullptr && decoded) created->ring = record.ring();
    return succeed();
}

Status MockKmt::destroy_context(std::uint32_t context)
{
    if (!begin(Call::destroy_context)) return refuse(last_code_);
    return close(Kind::context, context);
}

Status MockKmt::submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length,
                       const std::uint8_t *private_data, std::uint32_t private_size)
{
    if (!begin(Call::submit)) return refuse(last_code_);
    const Object *const submitting = find(Kind::context, context);
    if (submitting == nullptr || (private_data == nullptr) != (private_size == 0u)) return misuse();
    submission_size_ = private_size < max_private ? private_size : max_private;
    for (std::uint32_t i = 0u; i < submission_size_; i++) submission_[i] = private_data[i];
    const Object *const mapping = find_mapping(commands);
    const Object *const allocation = mapping != nullptr ? find(Kind::allocation, mapping->owner) : nullptr;
    if (length == 0u || allocation == nullptr || allocation->owner != submitting->owner ||
        commands - mapping->address + length > mapping->pages * page_bytes)
        return refuse(invalid_parameter);
    journal(*submitting);
    return succeed();
}

Status MockKmt::wait_for_context(std::uint32_t device, std::uint32_t context)
{
    if (!begin(Call::wait_for_context)) return refuse(last_code_);
    const Object *const object = find(Kind::context, context);
    if (find(Kind::device, device) == nullptr || object == nullptr || object->owner != device) return misuse();
    ticks_ += wait_cost_;
    if (slow_nth_ != 0u && calls_[static_cast<unsigned>(Call::wait_for_context)] % slow_nth_ == 0u) ticks_ += slow_ticks_;
    if (!withheld()) write_answer(*object);
    consume(*object);
    return succeed();
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
    std::uint8_t *const  slot =
        context.address != 0u ? view_of(static_cast<std::uint32_t>(context.address)) : nullptr;
    if (slot == nullptr || submitted.decode(submission_, submission_size_) != Status::ok) return;
    static_cast<void>(ReplyHeader(submitted.fence_id(), size).encode(slot, ReplyHeader::encoded_size));
    for (std::uint32_t i = 0u; i < size; i++) slot[ReplyHeader::encoded_size + i] = answer[i];
}

/* The command at the front of the batch the last submission named, read from
 * the ring as the device would; one command per batch is what a transport sends. */
void MockKmt::journal(const Object &context)
{
    SubmissionDescriptor submitted;
    RingHeader           header;
    const std::uint8_t *const ring = context.ring != 0u ? view_of(context.ring) : nullptr;
    if (ring == nullptr || journal_count_ == max_journal ||
        submitted.decode(submission_, submission_size_) != Status::ok ||
        header.decode(ring, RingHeader::encoded_size) != Status::ok)
        return;
    std::uint8_t *const leading = journal_bytes_[journal_count_];
    const std::uint64_t mask = header.capacity() - 1u;
    for (std::uint32_t i = 0u; i < journal_bytes; i++)
        leading[i] = ring[RingHeader::encoded_size + ((submitted.begin() + BatchHeader::encoded_size + i) & mask)];
    CommandHeader command;
    if (command.decode(leading, CommandHeader::encoded_size) != Status::ok) return;
    journal_ids_[journal_count_] = command.id();
    journal_sizes_[journal_count_] = command.size();
    journal_count_++;
}

/* Everything submitted is finished once the context is: the head moves to the
 * last submission's end, freeing the ring for the next. */
void MockKmt::consume(const Object &context)
{
    SubmissionDescriptor submitted;
    RingHeader           header;
    std::uint8_t *const  ring = context.ring != 0u ? view_of(context.ring) : nullptr;
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

std::uint64_t MockKmt::performance_counter(std::uint64_t &frequency) const
{
    frequency = frequency_;
    return ticks_;
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
    if (queued_count_ == max_queued) return;
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

void MockKmt::zero_next(Call call) { zeroing_[static_cast<unsigned>(call)] = true; }

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
    std::uint32_t filled = 0u;
    for (const auto &view : views_)
        for (const std::uint8_t byte : view) filled += byte != 0u ? 1u : 0u;
    return filled;
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
        if (locked_[slot] == allocation) return views_[slot];
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

}  // namespace av
