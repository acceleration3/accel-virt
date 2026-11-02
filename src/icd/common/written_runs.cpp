#include "icd/common/written_runs.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

WrittenRuns::WrittenRuns() : runs_{}, count_(0u)
{
    AV_SCOPE("WrittenRuns::WrittenRuns");
}

bool WrittenRuns::note(uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("WrittenRuns::note");
    if (bytes == 0u || bytes > ~uint64_t{0} - offset) return true;
    const uint64_t end = offset + bytes;
    if (count_ != 0u) {
        Run           &last = runs_[count_ - 1u];
        const uint64_t last_end = last.offset_ + last.bytes_;
        if (offset < last_end) return false;
        if (offset - last_end < merge_gap) {
            last.bytes_ = end - last.offset_;
            return true;
        }
    }
    if (count_ == max_runs) return false;
    runs_[count_++] = Run{offset, bytes};
    return true;
}

uint32_t WrittenRuns::count() const
{
    AV_SCOPE("WrittenRuns::count");
    return count_;
}

WrittenRuns::Run WrittenRuns::run(uint32_t index) const
{
    AV_SCOPE("WrittenRuns::run");
    return index < count_ ? runs_[index] : Run{0u, 0u};
}

void WrittenRuns::clear()
{
    AV_SCOPE("WrittenRuns::clear");
    count_ = 0u;
}

bool WrittenRuns::invariants_hold() const
{
    AV_SCOPE("WrittenRuns::invariants_hold");
    if (count_ > max_runs) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (runs_[index].bytes_ == 0u || runs_[index].bytes_ > ~uint64_t{0} - runs_[index].offset_) return false;
        if (index == 0u) continue;
        const uint64_t before = runs_[index - 1u].offset_ + runs_[index - 1u].bytes_;
        if (runs_[index].offset_ < before || runs_[index].offset_ - before < merge_gap) return false;
    }
    return true;
}

}
