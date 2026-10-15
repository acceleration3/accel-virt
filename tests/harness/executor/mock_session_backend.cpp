#include "harness/executor/mock_session_backend.hpp"

#include "device/abi/av_executor.h"

#include <unistd.h>

#include "executor/imported_blob.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

MockSessionBackend::MockSessionBackend()
    : answer_bytes_(0u), ran_(0u), adopted_(0u), abandoned_(0u), lost_(false), losable_(true), adopts_(true),
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
        /* More than an address space holds, whatever the heap behind it. */
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
    /* echo_id: the payload is the answer. */
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
    marked = 0u;
    reached = 0u;
}

void          MockSessionBackend::set_adopts(bool adopts) { adopts_ = adopts; }
std::uint32_t MockSessionBackend::adopted() const { return adopted_; }
std::uint32_t MockSessionBackend::abandoned() const { return abandoned_; }

bool MockSessionBackend::adopt(ImportedBlob &blob)
{
    if (!adopts_) return false;
    adopted_++;
    /* Whatever this API holds memory as, and a different number the guest
     * names it by, so a test can tell which of the two it was answered. */
    blob.record(blob.id() + 0x1000u, blob.id() + wire_offset);
    return true;
}

void MockSessionBackend::abandon(ImportedBlob &blob)
{
    abandoned_++;
    blob.record(0u, 0u);
}

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

/* A real descriptor, because whoever takes it closes it: a made-up number
 * would be a close of some other file of this process. */
bool MockSessionBackend::export_object(std::uint32_t kind, std::uint64_t wire_handle,
                                       std::uint32_t &handle_kind, int &fd)
{
    handle_kind = 0u;
    fd = -1;
    if (!shares_ || wire_handle == 0u || kind == 0u) return false;
    int ends[2] = {-1, -1};
    if (::pipe(ends) != 0) return false;
    ::close(ends[1]);
    fd = ends[0];
    handle_kind = AV_SHARED_HANDLE_DMA_BUF;
    exported_objects_++;
    void (*const during)(void *) = during_export_;
    during_export_ = nullptr;
    if (during != nullptr) during(during_export_context_);
    return true;
}

bool MockSessionBackend::import_object(std::uint32_t kind, std::uint32_t handle_kind, int fd,
                                       std::uint64_t size, const av::SharedImageShape &shape,
                                       std::uint64_t &wire_handle, std::uint64_t &detail)
{
    imported_shape_ = shape;
    wire_handle = 0u;
    detail = 0u;
    if (!shares_ || fd < 0 || kind == 0u || handle_kind == 0u || size == 0u) return false;
    descriptor_ = fd;
    imported_objects_++;
    wire_handle = object_handle + imported_objects_;
    detail = wire_handle ^ detail_mask;
    return true;
}

void MockSessionBackend::release_object(std::uint32_t, std::uint64_t wire_handle, std::uint64_t detail)
{
    if (wire_handle == 0u) return;
    released_objects_++;
    last_released_detail_ = detail;
}

}  // namespace av
