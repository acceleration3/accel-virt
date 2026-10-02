#include "executor/context_session.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"
#include "executor/gpu_clock_hold.hpp"
#include "executor/imported_blob.hpp"
#include "executor/session_backend.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ContextSession::ContextSession()
    : backend_(nullptr), clocks_(nullptr), arena_(), imports_(), starved_(0u), faulted_(false)
{
    AV_SCOPE("ContextSession::ContextSession");
}

Status ContextSession::initialize(SessionBackend &backend, uint8_t *block, size_t block_bytes)
{
    AV_SCOPE("ContextSession::initialize");
    if (backend_ != nullptr) return Status::invalid_state;
    backend_ = &backend;
    arena_.attach(block, block_bytes);
    starved_ = 0u;
    faulted_ = false;
    return Status::ok;
}

void ContextSession::release()
{
    AV_SCOPE("ContextSession::release");
    /* Whatever the guest never released is released here: an imported host
     * pointer must not outlive the mapping the device keeps for it, and an
     * adopted descriptor is this library's to close. */
    /* An import exists only while there is a backend - import_blob needs one
     * and this is the only place that drops it - so every blob here has one to
     * go back to. */
    for (ImportedBlob *blob = imports_.any(); blob != nullptr; blob = imports_.any()) {
        backend_->abandon(*blob);
        (void)imports_.drop(blob->id());
        blob->give_back();
    }
    backend_ = nullptr;
    clocks_ = nullptr;
    /* The block goes back to the library, so the slot holds no pointer into
     * memory the next context will be given. */
    arena_.attach(nullptr, 0u);
    faulted_ = false;
}

bool ContextSession::live() const
{
    AV_SCOPE("ContextSession::live");
    return backend_ != nullptr;
}

void ContextSession::bind_signal(void (*signal)(void *context), void *context)
{
    AV_SCOPE("ContextSession::bind_signal");
    if (backend_ != nullptr) backend_->bind_signal(signal, context);
}

bool ContextSession::poll()
{
    AV_SCOPE("ContextSession::poll");
    return backend_ != nullptr && backend_->poll();
}

void ContextSession::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("ContextSession::gpu_progress");
    marked = 0u;
    reached = 0u;
    if (backend_ != nullptr) backend_->gpu_progress(marked, reached);
}

void ContextSession::end_batch()
{
    AV_SCOPE("ContextSession::end_batch");
    if (backend_ != nullptr) backend_->end_batch();
    if (clocks_ != nullptr) clocks_->busy();
}

void ContextSession::hold_clocks(GpuClockHold &clocks)
{
    AV_SCOPE("ContextSession::hold_clocks");
    clocks_ = &clocks;
}

uint32_t ContextSession::execute(uint32_t command_id, const uint8_t *payload, size_t size,
                                 uint8_t *reply, size_t reply_capacity, size_t &answer)
{
    AV_SCOPE("ContextSession::execute");
    answer = 0u;
    /* Once the host device is gone nothing more will run on it, and saying so
     * again is cheaper than finding out again. */
    if (backend_ == nullptr || faulted_) return AV_DEVICE_LOST;
    if (!backend_->knows(command_id)) return AV_UNKNOWN;

    arena_.reset();
    const size_t refused = arena_.refusals();
    WireReader reader(payload, size);
    WireWriter writer(reply, reply_capacity);
    const bool ran = backend_->run(command_id, reader, writer, arena_.arena());
    if (!ran) {
        /* The block being too small and the frame being malformed are the same
         * answer on the wire on purpose, and two different problems here. */
        if (arena_.refusals() != refused) starved_++;
        return AV_UNDECODABLE;
    }
    if (backend_->lost()) {
        faulted_ = true;
        return AV_DEVICE_LOST;
    }
    /* What the answer wanted, not what fitted: an answer longer than the
     * capacity writes nothing and reports its length, so the device can tell
     * the guest what to allocate. */
    answer = writer.wanted();
    return AV_EXECUTED;
}

uint32_t ContextSession::import_blob(uint64_t blob_id, const AvBlobHandle &handle)
{
    AV_SCOPE("ContextSession::import_blob");
    if (backend_ == nullptr || faulted_) return AV_UNDECODABLE;
    ImportedBlob *taken = nullptr;
    if (imports_.take(blob_id, handle, taken) != Status::ok) return AV_UNDECODABLE;
    if (!backend_->adopt(*taken)) {
        /* An API that cannot take this kind is not a fault: the device is told
         * no and the guest is copied through instead. The slot goes back so a
         * later handle of a kind it can take still fits. */
        (void)imports_.drop(blob_id);
        taken->give_back();
        return AV_UNDECODABLE;
    }
    return AV_EXECUTED;
}

bool ContextSession::export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind, int &fd)
{
    AV_SCOPE("ContextSession::export_object");
    handle_kind = 0u;
    fd = -1;
    if (backend_ == nullptr || faulted_) return false;
    return backend_->export_object(kind, wire_handle, handle_kind, fd);
}

bool ContextSession::import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                   uint64_t &wire_handle)
{
    AV_SCOPE("ContextSession::import_object");
    wire_handle = 0u;
    if (backend_ == nullptr || faulted_) return false;
    return backend_->import_object(kind, handle_kind, fd, size, wire_handle);
}

void ContextSession::release_object(uint32_t kind, uint64_t wire_handle)
{
    AV_SCOPE("ContextSession::release_object");
    if (backend_ == nullptr) return;
    backend_->release_object(kind, wire_handle);
}

void ContextSession::release_blob(uint64_t blob_id)
{
    AV_SCOPE("ContextSession::release_blob");
    ImportedBlob *const blob = imports_.drop(blob_id);
    if (blob == nullptr) return;
    backend_->abandon(*blob);
    blob->give_back();
}

uint64_t ContextSession::imported_handle(uint64_t blob_id)
{
    AV_SCOPE("ContextSession::imported_handle");
    const ImportedBlob *const blob = imports_.find(blob_id);
    return blob != nullptr ? blob->wire_handle() : 0u;
}

uint32_t ContextSession::imports() const
{
    AV_SCOPE("ContextSession::imports");
    return imports_.count();
}

size_t ContextSession::starved() const
{
    AV_SCOPE("ContextSession::starved");
    return starved_;
}

bool ContextSession::invariants_hold() const
{
    AV_SCOPE("ContextSession::invariants_hold");
    return arena_.invariants_hold() && imports_.invariants_hold() &&
           (backend_ != nullptr || !faulted_) && (backend_ != nullptr || imports_.count() == 0u);
}

}  // namespace av
