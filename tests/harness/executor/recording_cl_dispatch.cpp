#include "harness/executor/recording_cl_dispatch.hpp"

#include "proto/wire/wire_arena.hpp"

namespace av {

RecordingClDispatch::RecordingClDispatch(const std::uint8_t *block, const WireArena &arena)
    : block_(block), arena_(arena), handles_(), objects_{}, produced_(0u), calls_(0u),
      last_called_(0u), strangers_(0u), strays_(0u), refused_(0u)
{}

void *RecordingClDispatch::handle_for(clproto::HandleKind kind, std::uint64_t wire) const
{
    return handles_.resolve(kind, wire);
}

void RecordingClDispatch::made(clproto::HandleKind kind, void *object)
{
    if (!handles_.remember(kind, object)) refused_++;
}

void RecordingClDispatch::retained(clproto::HandleKind kind, void *object)
{
    handles_.retain(kind, object);
}

void RecordingClDispatch::released(clproto::HandleKind kind, void *object)
{
    handles_.release(kind, object);
}

std::uint64_t RecordingClDispatch::wire_of(const void *object) const
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(object));
}

std::uint64_t RecordingClDispatch::wire_of_made(std::size_t index) const
{
    return index < produced_ ? wire_of(&objects_[index]) : 0u;
}

std::uint32_t RecordingClDispatch::calls() const { return calls_; }
std::uint32_t RecordingClDispatch::last_called() const { return last_called_; }
std::uint32_t RecordingClDispatch::strangers() const { return strangers_; }
std::uint32_t RecordingClDispatch::strays() const { return strays_; }
std::uint32_t RecordingClDispatch::refused() const { return refused_; }
const ClHandleRegistry &RecordingClDispatch::handles() const { return handles_; }

void RecordingClDispatch::called(std::uint32_t command_id)
{
    calls_++;
    last_called_ = command_id;
}

void RecordingClDispatch::handed(clproto::HandleKind kind, void *object)
{
    /* Null is the API's own and the ICD refuses it; anything else has to be
     * an object this dispatch holds as exactly this kind. */
    if (object == nullptr) return;
    if (handles_.resolve(kind, wire_of(object)) != object) strangers_++;
}

void RecordingClDispatch::touched(const void *bytes, std::size_t length)
{
    if (length == 0u) return;
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(bytes);
    const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(block_);
    const std::uintptr_t end = first + arena_.used();
    if (start < first || start > end || length > end - start) strays_++;
}

void *RecordingClDispatch::produce(clproto::HandleKind)
{
    if (produced_ == max_objects) return nullptr;
    return &objects_[produced_++];
}

}  // namespace av
