#include "harness/executor/mock_session_backend.hpp"

#include "device/abi/av_executor.h"

#include <unistd.h>

#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

MockSessionBackend::MockSessionBackend()
    : answer_bytes_(0u), ran_(0u), adopted_(0u), abandoned_(0u), unbound_(0u), unbound_when_retired_(0u), lost_(false), losable_(true), adopts_(true), marked_(0u), reached_(0u),
      shares_(true), exported_objects_(0u), imported_objects_(0u), released_objects_(0u),
      descriptor_(-1), signal_(nullptr), signal_context_(nullptr), during_export_(nullptr),
      during_export_context_(nullptr)
{}

void MockSessionBackend::set_answer_bytes(std::size_t bytes) { answer_bytes_ = bytes; }

std::uint32_t MockSessionBackend::ran() const { return ran_; }

bool MockSessionBackend::knows(std::uint32_t command_id) const
{
    return command_id == echo_id || command_id == long_answer_id || command_id == malformed_id ||
           command_id == greedy_id || command_id == lose_id || command_id == roomy_id;
}

bool MockSessionBackend::run(std::uint32_t command_id, WireReader &payload, WireWriter &reply,
                             WireArena &arena)
{
    ran_++;
    if (command_id == malformed_id) return false;
    if (command_id == greedy_id) {
        return arena.allocate<std::uint64_t>(~std::size_t{0} / 4u) != nullptr;
    }
    if (command_id == roomy_id) {
        std::uint8_t *const room = arena.allocate<std::uint8_t>(4u * arena.capacity());
        if (room == nullptr) return false;
        for (std::size_t index = 0u; index < 4u * arena.capacity(); index++) room[index] = 0x5Au;
        return true;
    }
    if (command_id == long_answer_id) {
        reply.write_zeros(answer_bytes_);
        return true;
    }
    if (command_id == lose_id) {
        lost_ = true;
        return true;
    }
    while (payload.remaining() != 0u) {
        std::uint8_t byte = 0u;
        if (!payload.read_pod(byte)) return false;
        reply.write_pod(byte);
    }
    return true;
}

bool MockSessionBackend::lost() const { return lost_; }

bool MockSessionBackend::lose_device()
{
    if (!losable_) return false;
    lost_ = true;
    return true;
}

void MockSessionBackend::set_losable(bool losable) { losable_ = losable; }

void MockSessionBackend::lose_elsewhere() { lost_ = true; }

void MockSessionBackend::end_batch() {}

bool MockSessionBackend::drain(TeardownBudget &)
{
    __atomic_fetch_add(&drains_, 1u, __ATOMIC_RELAXED);
    drained_on_ = pthread_self();
    __atomic_store_n(&drained_, true, __ATOMIC_RELEASE);
    while (__atomic_load_n(&drains_held_, __ATOMIC_ACQUIRE)) usleep(200);
    abandoned_before_drain_ = abandoned_ + unbound_;
    const bool in_time = __atomic_load_n(&drains_in_time_, __ATOMIC_RELAXED);
    if (in_time) __atomic_fetch_add(&finished_drains_, 1u, __ATOMIC_RELAXED);
    return in_time;
}

void MockSessionBackend::hold_drains(bool held) { __atomic_store_n(&drains_held_, held, __ATOMIC_RELEASE); }

bool MockSessionBackend::drained_on(pthread_t thread) const
{
    return __atomic_load_n(&drained_, __ATOMIC_ACQUIRE) && pthread_equal(drained_on_, thread) != 0;
}

void MockSessionBackend::set_drains_in_time(bool in_time) { __atomic_store_n(&drains_in_time_, in_time, __ATOMIC_RELAXED); }

std::uint32_t MockSessionBackend::finished_drains() const { return __atomic_load_n(&finished_drains_, __ATOMIC_RELAXED); }

std::uint32_t MockSessionBackend::drains() const { return __atomic_load_n(&drains_, __ATOMIC_RELAXED); }

std::uint32_t MockSessionBackend::drains_before_first_release() const { return drains_before_first_release_; }

std::uint32_t MockSessionBackend::abandoned_before_drain() const { return abandoned_before_drain_; }

bool MockSessionBackend::poll() { return false; }

void MockSessionBackend::bind_signal(void (*signal)(void *context), void *context)
{
    signal_ = signal;
    signal_context_ = context;
}

bool MockSessionBackend::signal()
{
    if (signal_ == nullptr) return false;
    signal_(signal_context_);
    return true;
}

void MockSessionBackend::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    marked = marked_ + (__atomic_load_n(&drains_held_, __ATOMIC_ACQUIRE) ? 1u : 0u);
    reached = reached_;
}

void          MockSessionBackend::set_progress(std::uint64_t marked, std::uint64_t reached)
{
    marked_ = marked;
    reached_ = reached;
}

void          MockSessionBackend::set_adopts(bool adopts) { adopts_ = adopts; }
void          MockSessionBackend::set_one_handle(bool one) { one_handle_ = one; }
std::uint32_t MockSessionBackend::adopted() const { return adopted_; }
std::uint32_t MockSessionBackend::abandoned() const { return abandoned_; }

bool MockSessionBackend::adopt(SourceMemory &, const BlobRole &, std::uint64_t &handle)
{
    handle = 0u;
    if (!adopts_) return false;
    adopted_++;
    handle = wire_offset + (one_handle_ ? 0u : adopted_);
    return true;
}

void MockSessionBackend::abandon(const SourceMemory &, std::uint64_t) { abandoned_++; }

void MockSessionBackend::unbind_arena(const SourceMemory &) { unbound_++; }

void MockSessionBackend::retire_arena(const SourceMemory &)
{
    unbound_when_retired_ = unbound_;
    abandoned_++;
}

std::uint32_t MockSessionBackend::unbound() const { return unbound_; }
std::uint32_t MockSessionBackend::unbound_when_retired() const { return unbound_when_retired_; }

void MockSessionBackend::hold_shut_downs(bool held) { __atomic_store_n(&shut_downs_held_, held, __ATOMIC_RELEASE); }

bool MockSessionBackend::inside_shut_down() const { return __atomic_load_n(&inside_shut_down_, __ATOMIC_ACQUIRE); }

void MockSessionBackend::shut_down()
{
    __atomic_store_n(&inside_shut_down_, true, __ATOMIC_RELEASE);
    while (__atomic_load_n(&shut_downs_held_, __ATOMIC_ACQUIRE)) usleep(200);
    __atomic_fetch_add(&shut_downs_, 1u, __ATOMIC_RELAXED);
    abandoned_before_shut_down_ = abandoned_;
    released_before_shut_down_ = released_objects_;
}

void MockSessionBackend::let_go_of_thread() { __atomic_fetch_add(&let_goes_, 1u, __ATOMIC_RELAXED); }

std::uint32_t MockSessionBackend::let_goes() const { return __atomic_load_n(&let_goes_, __ATOMIC_RELAXED); }

std::uint32_t MockSessionBackend::shut_downs() const { return __atomic_load_n(&shut_downs_, __ATOMIC_RELAXED); }
std::uint32_t MockSessionBackend::abandoned_before_shut_down() const { return abandoned_before_shut_down_; }
std::uint32_t MockSessionBackend::released_before_shut_down() const { return released_before_shut_down_; }

void MockSessionBackend::set_shares(bool shares) { shares_ = shares; }

std::uint32_t MockSessionBackend::exported_objects() const { return exported_objects_; }
std::uint32_t MockSessionBackend::imported_objects() const { return imported_objects_; }
std::uint32_t MockSessionBackend::released_objects() const { return released_objects_; }
std::uint64_t MockSessionBackend::last_released_detail() const { return last_released_detail_; }

void MockSessionBackend::before_next_export(void (*during)(void *context), void *context)
{
    during_export_ = during;
    during_export_context_ = context;
}

bool MockSessionBackend::export_object(std::uint32_t kind, std::uint64_t wire_handle,
                                       std::uint32_t &handle_kind, OwnedFd &descriptor)
{
    handle_kind = 0u;
    descriptor.close();
    if (!shares_ || wire_handle == 0u || kind == 0u) return false;
    int ends[2] = {-1, -1};
    if (::pipe(ends) != 0) return false;
    ::close(ends[1]);
    descriptor.hold(ends[0]);
    handle_kind = AV_SHARED_HANDLE_DMA_BUF;
    exported_objects_++;
    void (*const during)(void *) = during_export_;
    during_export_ = nullptr;
    if (during != nullptr) during(during_export_context_);
    return true;
}

bool MockSessionBackend::import_object(std::uint32_t kind, std::uint32_t handle_kind, int fd,
                                       std::uint64_t size, const av::SharedImageShape &shape,
                                       std::uint64_t into, std::uint64_t &wire_handle, std::uint64_t &detail)
{
    imported_shape_ = shape;
    wire_handle = 0u;
    detail = 0u;
    if (!shares_ || fd < 0 || kind == 0u || handle_kind == 0u || size == 0u) return false;
    descriptor_ = fd;
    imported_objects_++;
    wire_handle = into != 0u ? into : object_handle + imported_objects_;
    detail = wire_handle ^ detail_mask;
    return true;
}

void MockSessionBackend::release_object(std::uint32_t, std::uint64_t wire_handle, std::uint64_t detail)
{
    if (wire_handle == 0u) return;
    if (released_objects_ == 0u) drains_before_first_release_ = drains_;
    released_objects_++;
    last_released_detail_ = detail;
}

}
