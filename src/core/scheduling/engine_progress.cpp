#include "core/scheduling/engine_progress.hpp"

#include "core/scheduling/execution_node.hpp"

namespace av {

EngineProgress::EngineProgress(Platform &platform, ExecutionNode &node)
    : platform_(platform), node_(node), answered_progress_(0u), answered_(false) {}

bool EngineProgress::responsive()
{
    platform_.require_irql_at_most(Irql::dispatch, "EngineProgress::responsive");
    if (node_.pending() == 0u) return true;
    const uint64_t progress = node_.progress_time();
    const bool     moved = !answered_ || progress != answered_progress_;
    answered_progress_ = progress;
    answered_ = true;
    return moved;
}

}  // namespace av
