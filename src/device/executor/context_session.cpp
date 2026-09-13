#include "device/executor/context_session.hpp"

#include "device/abi/rvk_executor.h"
#include "device/executor/imported_blob.hpp"
#include "device/executor/session_backend.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ContextSession::ContextSession()
    : backend_(nullptr), arena_(), imports_(), starved_(0u), faulted_(false)
{}

Status ContextSession::initialize(SessionBackend &backend, uint8_t *block, size_t block_bytes)
{
    if (backend_ != nullptr) return Status::invalid_state;
    backend_ = &backend;
    arena_.attach(block, block_bytes);
    starved_ = 0u;
    faulted_ = false;
    return Status::ok;
}

void ContextSession::release()
{
    /* Whatever the guest never released is released here: an imported host
     * pointer must not outlive the mapping the device keeps for it, and an
     * adopted descriptor is this library's to close. */
    for (ImportedBlob *blob = imports_.any(); blob != nullptr; blob = imports_.any()) {
        if (backend_ != nullptr) backend_->abandon(*blob);
        (void)imports_.drop(blob->id());
        blob->give_back();
    }
    backend_ = nullptr;
    /* The block goes back to the library, so the slot holds no pointer into
     * memory the next context will be given. */
    arena_.attach(nullptr, 0u);
    faulted_ = false;
}

bool ContextSession::live() const { return backend_ != nullptr; }

uint32_t ContextSession::execute(uint32_t command_id, const uint8_t *payload, size_t size,
                                 uint8_t *reply, size_t reply_capacity, size_t &answer)
{
    answer = 0u;
    /* Once the host device is gone nothing more will run on it, and saying so
     * again is cheaper than finding out again. */
    if (backend_ == nullptr || faulted_) return RVK_DEVICE_LOST;
    if (!backend_->knows(command_id)) return RVK_UNKNOWN;

    arena_.reset();
    const size_t refused = arena_.refusals();
    WireReader reader(payload, size);
    WireWriter writer(reply, reply_capacity);
    const bool ran = backend_->run(command_id, reader, writer, arena_.arena());
    if (!ran) {
        /* The block being too small and the frame being malformed are the same
         * answer on the wire on purpose, and two different problems here. */
        if (arena_.refusals() != refused) starved_++;
        return RVK_UNDECODABLE;
    }
    if (backend_->lost()) {
        faulted_ = true;
        return RVK_DEVICE_LOST;
    }
    /* What the answer wanted, not what fitted: an answer longer than the
     * capacity writes nothing and reports its length, so the device can tell
     * the guest what to allocate. */
    answer = writer.wanted();
    return RVK_EXECUTED;
}

uint32_t ContextSession::import_blob(uint64_t blob_id, const RvkBlobHandle &handle)
{
    if (backend_ == nullptr || faulted_) return RVK_UNDECODABLE;
    ImportedBlob *taken = nullptr;
    if (imports_.take(blob_id, handle, taken) != Status::ok) return RVK_UNDECODABLE;
    if (!backend_->adopt(*taken)) {
        /* An API that cannot take this kind is not a fault: the device is told
         * no and the guest is copied through instead. The slot goes back so a
         * later handle of a kind it can take still fits. */
        (void)imports_.drop(blob_id);
        taken->give_back();
        return RVK_UNDECODABLE;
    }
    return RVK_EXECUTED;
}

void ContextSession::release_blob(uint64_t blob_id)
{
    ImportedBlob *const blob = imports_.drop(blob_id);
    if (blob == nullptr) return;
    if (backend_ != nullptr) backend_->abandon(*blob);
    blob->give_back();
}

uint32_t ContextSession::imports() const { return imports_.count(); }

size_t ContextSession::starved() const { return starved_; }

bool ContextSession::invariants_hold() const
{
    return arena_.invariants_hold() && imports_.invariants_hold() &&
           (backend_ != nullptr || !faulted_) && (backend_ != nullptr || imports_.count() == 0u);
}

}  // namespace av
