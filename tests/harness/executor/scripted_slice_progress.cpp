#include "harness/executor/scripted_slice_progress.hpp"

namespace av {

ScriptedSliceProgress::ScriptedSliceProgress() : next_(1u), reached_(0u) {}

ScriptedSliceProgress::ScriptedSliceProgress(std::uint64_t next, std::uint64_t reached)
    : next_(next), reached_(reached)
{
}

std::uint64_t ScriptedSliceProgress::next() const { return next_; }
std::uint64_t ScriptedSliceProgress::reached() const { return reached_; }

void ScriptedSliceProgress::set(std::uint64_t next, std::uint64_t reached)
{
    next_ = next;
    reached_ = reached;
}

void ScriptedSliceProgress::submit() { next_++; }

void ScriptedSliceProgress::complete_through(std::uint64_t mark)
{
    if (mark > reached_) reached_ = mark;
}

}
