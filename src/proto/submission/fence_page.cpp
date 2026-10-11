#include "proto/submission/fence_page.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

FencePage::FencePage() : flags_(0u), completed_fence_(0u), progress_(0u), fault_code_(0u), raised_(0u)
{
    AV_SCOPE("FencePage::FencePage");
}

FencePage::FencePage(uint64_t completed_fence, uint64_t progress)
    : flags_(0u), completed_fence_(completed_fence), progress_(progress), fault_code_(0u), raised_(0u)
    {
        AV_SCOPE("FencePage::FencePage");
    }

FencePage::FencePage(uint64_t completed_fence, uint64_t progress, uint32_t fault_code)
    : flags_(faulted_flag), completed_fence_(completed_fence), progress_(progress),
      fault_code_(fault_code), raised_(0u)
      {
          AV_SCOPE("FencePage::FencePage");
      }

FencePage FencePage::raising(uint64_t raised) const
{
    AV_SCOPE("FencePage::raising");
    FencePage page = *this;
    page.raised_ = raised;
    return page;
}

bool     FencePage::faulted() const
{
    AV_SCOPE("FencePage::faulted");
    return (flags_ & faulted_flag) != 0u;
}
uint64_t FencePage::completed_fence() const
{
    AV_SCOPE("FencePage::completed_fence");
    return completed_fence_;
}
uint64_t FencePage::progress() const
{
    AV_SCOPE("FencePage::progress");
    return progress_;
}
uint32_t FencePage::fault_code() const
{
    AV_SCOPE("FencePage::fault_code");
    return fault_code_;
}
uint64_t FencePage::raised() const
{
    AV_SCOPE("FencePage::raised");
    return raised_;
}

Status FencePage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("FencePage::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(flags_);
    writer.write_u64(completed_fence_);
    writer.write_u64(progress_);
    writer.write_u32(fault_code_);
    writer.write_zeros(4u);
    writer.write_u64(raised_);
    return writer.status();
}

Status FencePage::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("FencePage::decode");
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    FencePage candidate;
    candidate.flags_           = reader.read_u32();
    candidate.completed_fence_ = reader.read_u64();
    candidate.progress_        = reader.read_u64();
    candidate.fault_code_      = reader.read_u32();
    reader.expect_zero(4u);
    candidate.raised_          = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    const bool unknown_flags = (candidate.flags_ & ~faulted_flag) != 0u;
    const bool stray_code = !candidate.faulted() && candidate.fault_code_ != 0u;
    if (found_magic != magic || unknown_flags || stray_code) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
