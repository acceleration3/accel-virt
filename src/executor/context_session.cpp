#include "executor/context_session.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"
#include "executor/gpu_clock_hold.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/imported_blob.hpp"
#include "executor/session_backend.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"


namespace av {

ContextSession::ContextSession(DriverLog &lines)
    : backend_(nullptr), clocks_(nullptr), source_(nullptr), domain_(0u), heap_(), arena_(nullptr, 0u), answer_(heap_),
      answer_length_(0u), imports_(heap_), starved_(0u), channel_(), faulted_(false), parked_{}, parked_count_(0u),
      lines_(lines)
{
    AV_SCOPE("ContextSession::ContextSession");
    arena_.overflow_into(heap_);
}

Status ContextSession::initialize(SessionBackend &backend, uint8_t *block, size_t block_bytes)
{
    AV_SCOPE("ContextSession::initialize");
    if (backend_ != nullptr) return Status::invalid_state;
    backend_ = &backend;
    arena_.attach(block, block_bytes);
    starved_ = 0u;
    channel_.reset();
    faulted_ = false;
    return Status::ok;
}

void ContextSession::release()
{
    AV_SCOPE("ContextSession::release");
    for (ImportedBlob *blob = imports_.any(); blob != nullptr; blob = imports_.any()) retire_blob(blob->id());
    parked_count_ = 0u;
    if (backend_ != nullptr) backend_->shut_down();
    backend_ = nullptr;
    clocks_ = nullptr;
    source_ = nullptr;
    domain_ = 0u;
    arena_.attach(nullptr, 0u);
    answer_.release();
    answer_length_ = 0u;
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

uint32_t ContextSession::poll()
{
    AV_SCOPE("ContextSession::poll");
    if (backend_ == nullptr) return AV_POLL_IDLE;
    drain_parked();
    if (faulted_ || backend_->lost()) {
        if (!faulted_) lines_.write("a session's host device was lost while it was idle");
        faulted_ = true;
        return AV_POLL_LOST;
    }
    return backend_->poll() ? AV_POLL_PENDING : AV_POLL_IDLE;
}

void ContextSession::channel_stats(uint32_t context_id, const uint8_t *body, size_t size)
{
    AV_SCOPE("ContextSession::channel_stats");
    if (backend_ != nullptr) channel_.accept(context_id, body, size);
}

void ContextSession::report_channel(StateWriter &writer) const
{
    AV_SCOPE("ContextSession::report_channel");
    if (channel_.received()) channel_.report(writer, parked_count_);
}

uint32_t ContextSession::lose_device()
{
    AV_SCOPE("ContextSession::lose_device");
    return backend_ != nullptr && backend_->lose_device() ? AV_EXECUTED : AV_UNKNOWN;
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
    drain_parked();
}

void ContextSession::hold_clocks(GpuClockHold &clocks)
{
    AV_SCOPE("ContextSession::hold_clocks");
    clocks_ = &clocks;
}

void ContextSession::draw_from(HostMemorySource &source, uint32_t domain)
{
    AV_SCOPE("ContextSession::draw_from");
    source_ = &source;
    domain_ = domain;
}

uint32_t ContextSession::execute(uint32_t command_id, const uint8_t *payload, size_t size,
                                 uint8_t *reply, size_t reply_capacity, size_t &answer)
{
    AV_SCOPE("ContextSession::execute");
    answer = 0u;
    answer_.release();
    answer_length_ = 0u;
    if (backend_ == nullptr || faulted_) return AV_DEVICE_LOST;
    if (!backend_->knows(command_id)) return AV_UNKNOWN;

    arena_.reset();
    const size_t refused = arena_.refusals();
    WireReader reader(payload, size);
    WireWriter writer = reply != nullptr ? WireWriter(reply, reply_capacity, answer_) : WireWriter(nullptr, 0u);
    const bool ran = backend_->run(command_id, reader, writer, arena_);
    arena_.reset();
    if (!ran) {
        const bool starved = arena_.refusals() != refused;
        if (starved) starved_++;
        lines_.note_hex(starved ? "a command the decode arena had no room for, id" : "a command that would not decode, id",
                        command_id, 8u);
        return AV_UNDECODABLE;
    }
    if (backend_->lost()) {
        faulted_ = true;
        return AV_DEVICE_LOST;
    }
    answer = writer.wanted();
    if (writer.spilled()) answer_length_ = writer.offset();
    else answer_.release();
    return AV_EXECUTED;
}

size_t ContextSession::answer_part(uint64_t offset, uint8_t *out, size_t capacity, uint64_t &length) const
{
    AV_SCOPE("ContextSession::answer_part");
    length = answer_length_;
    if (offset >= answer_length_ || out == nullptr) return 0u;
    const size_t left = answer_length_ - static_cast<size_t>(offset);
    const size_t copied = left < capacity ? left : capacity;
    __builtin_memcpy(out, answer_.bytes() + offset, copied);
    return copied;
}

uint32_t ContextSession::import_blob(uint64_t blob_id, uint64_t memory_key, const BlobRole &role)
{
    AV_SCOPE("ContextSession::import_blob");
    if (backend_ == nullptr || faulted_ || source_ == nullptr) return AV_UNDECODABLE;
    SourceMemory *const memory = source_->acquire(domain_, memory_key);
    if (memory == nullptr) {
        lines_.note("an import of memory the source has not got, key", memory_key);
        return AV_UNDECODABLE;
    }
    ImportedBlob *taken = nullptr;
    if (imports_.take(blob_id, *memory, taken) != Status::ok) {
        source_->release(*memory);
        lines_.note("a blob the import table would not take, id", blob_id);
        return AV_UNDECODABLE;
    }
    uint64_t handle = 0u;
    if (!backend_->adopt(*memory, role, handle)) {
        source_->release(*imports_.forget(blob_id));
        return AV_UNDECODABLE;
    }
    if (imports_.name(*taken, handle) != Status::ok) {
        lines_.note("an import answered a handle another import is named by, handle", handle);
        backend_->abandon(*memory, handle);
        source_->release(*imports_.forget(blob_id));
        return AV_UNDECODABLE;
    }
    if (role.is_arena()) taken->keep_as_arena();
    return AV_EXECUTED;
}

bool ContextSession::faulted() const
{
    AV_SCOPE("ContextSession::faulted");
    return faulted_;
}

void ContextSession::release_blob(uint64_t blob_id)
{
    AV_SCOPE("ContextSession::release_blob");
    ImportedBlob *const blob = imports_.find(blob_id);
    if (blob == nullptr) return;
    for (uint32_t index = 0u; index < parked_count_; index++)
        if (parked_[index].blob_id_ == blob_id) return;
    uint64_t marked = 0u;
    uint64_t reached = 0u;
    if (blob->arena() && backend_ != nullptr) backend_->gpu_progress(marked, reached);
    if (reached < marked && parked_count_ < parked_most) {
        parked_[parked_count_++] = Parked{blob_id, marked};
        return;
    }
    retire_blob(blob_id);
}

void ContextSession::retire_blob(uint64_t blob_id)
{
    AV_SCOPE("ContextSession::retire_blob");
    ImportedBlob *const blob = imports_.find(blob_id);
    if (blob == nullptr) return;
    if (blob->handle() != 0u) backend_->abandon(blob->memory(), blob->handle());
    source_->release(*imports_.forget(blob_id));
}

void ContextSession::drain_parked()
{
    AV_SCOPE("ContextSession::drain_parked");
    if (parked_count_ == 0u || backend_ == nullptr) return;
    uint64_t marked = 0u;
    uint64_t reached = 0u;
    backend_->gpu_progress(marked, reached);
    uint32_t kept = 0u;
    for (uint32_t index = 0u; index < parked_count_; index++) {
        if (reached < parked_[index].mark_) {
            parked_[kept++] = parked_[index];
            continue;
        }
        retire_blob(parked_[index].blob_id_);
    }
    parked_count_ = kept;
}

uint32_t ContextSession::parked() const
{
    AV_SCOPE("ContextSession::parked");
    return parked_count_;
}

uint64_t ContextSession::imported_handle(uint64_t blob_id)
{
    AV_SCOPE("ContextSession::imported_handle");
    const ImportedBlob *const blob = imports_.find(blob_id);
    return blob != nullptr ? blob->handle() : 0u;
}

ImportTable &ContextSession::imports()
{
    AV_SCOPE("ContextSession::imports");
    return imports_;
}

size_t ContextSession::starved() const
{
    AV_SCOPE("ContextSession::starved");
    return starved_;
}

bool ContextSession::invariants_hold() const
{
    AV_SCOPE("ContextSession::invariants_hold");
    if (parked_count_ > parked_most || parked_count_ > imports_.count()) return false;
    for (uint32_t index = 0u; index < parked_count_; index++)
        if (parked_[index].blob_id_ == 0u) return false;
    return arena_.invariants_hold() && imports_.invariants_hold() && (backend_ != nullptr || !faulted_) &&
           (backend_ != nullptr || imports_.count() == 0u) && (source_ != nullptr || imports_.count() == 0u);
}

}
