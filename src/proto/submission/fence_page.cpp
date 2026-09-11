#include "proto/submission/fence_page.hpp"

#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

FencePage::FencePage() : flags_(0u), completed_fence_(0u), progress_(0u), fault_code_(0u) {}

FencePage::FencePage(uint64_t completed_fence, uint64_t progress)
    : flags_(0u), completed_fence_(completed_fence), progress_(progress), fault_code_(0u) {}

FencePage::FencePage(uint64_t completed_fence, uint64_t progress, uint32_t fault_code)
    : flags_(faulted_flag), completed_fence_(completed_fence), progress_(progress),
      fault_code_(fault_code) {}

bool     FencePage::faulted() const         { return (flags_ & faulted_flag) != 0u; }
uint64_t FencePage::completed_fence() const { return completed_fence_; }
uint64_t FencePage::progress() const        { return progress_; }
uint32_t FencePage::fault_code() const      { return fault_code_; }

Status FencePage::encode(uint8_t *bytes, size_t capacity) const
{
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(flags_);
    writer.write_u64(completed_fence_);
    writer.write_u64(progress_);
    writer.write_u32(fault_code_);
    writer.write_zeros(4u);
    return writer.status();
}

Status FencePage::decode(const uint8_t *bytes, size_t length)
{
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    FencePage candidate;
    candidate.flags_           = reader.read_u32();
    candidate.completed_fence_ = reader.read_u64();
    candidate.progress_        = reader.read_u64();
    candidate.fault_code_      = reader.read_u32();
    reader.expect_zero(4u);
    if (reader.status() != Status::ok) return reader.status();
    const bool unknown_flags = (candidate.flags_ & ~faulted_flag) != 0u;
    const bool stray_code = !candidate.faulted() && candidate.fault_code_ != 0u;
    if (found_magic != magic || unknown_flags || stray_code) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
