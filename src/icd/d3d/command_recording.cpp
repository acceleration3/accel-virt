#include "icd/d3d/command_recording.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"

namespace av::d3d {

CommandRecording::CommandRecording(BlockSource &memory, size_t first, size_t most)
    : memory_(memory), first_(first), most_(most < first ? first : most), bytes_(nullptr), capacity_(0u)
{
    AV_SCOPE("CommandRecording::CommandRecording");
}

CommandRecording::~CommandRecording()
{
    AV_SCOPE("CommandRecording::~CommandRecording");
    if (bytes_ != nullptr) memory_.release(bytes_);
}

Status CommandRecording::initialize()
{
    AV_SCOPE("CommandRecording::initialize");
    if (bytes_ != nullptr) return Status::invalid_state;
    if (first_ == 0u) return Status::invalid_parameter;
    bytes_ = static_cast<uint8_t *>(memory_.allocate(first_, Pool::paged));
    if (bytes_ == nullptr) return Status::no_memory;
    capacity_ = first_;
    return Status::ok;
}

bool CommandRecording::hold(size_t kept, size_t wanted)
{
    AV_SCOPE("CommandRecording::hold");
    if (bytes_ == nullptr || kept > capacity_) return false;
    if (capacity_ - kept >= wanted) return true;
    if (wanted > most_ - kept) return false;
    size_t grown = capacity_;
    while (grown - kept < wanted) grown = grown > most_ / 2u ? most_ : grown * 2u;
    uint8_t *const larger = static_cast<uint8_t *>(memory_.allocate(grown, Pool::paged));
    if (larger == nullptr) return false;
    if (kept != 0u) __builtin_memcpy(larger, bytes_, kept);
    memory_.release(bytes_);
    bytes_ = larger;
    capacity_ = grown;
    return true;
}

uint8_t *CommandRecording::bytes() const
{
    AV_SCOPE("CommandRecording::bytes");
    return bytes_;
}

size_t CommandRecording::capacity() const
{
    AV_SCOPE("CommandRecording::capacity");
    return capacity_;
}

}
