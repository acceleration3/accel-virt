/* Whether the execution node is getting anywhere, as dxgkrnl's timeout check
 * asks it. VidSchiCheckNodeTimeout (dxgmms2.sys @1400c9b50) decides a node is
 * late, asks DxgkDdiQueryEngineStatus, and returns "no timeout this round" for
 * any answer with the responsive bit set - so an answer that is always yes
 * defers a hung host forever and no TDR ever runs.
 *
 * The answer is therefore about movement, not about hope: the node has nothing
 * queued, or its progress has moved since the last time this was asked. The
 * first answer about a busy node is a deferral, because the check that asks has
 * only just decided the node is late and a slow host may be inside the round
 * trip that finishes it. */
#ifndef AV_ENGINE_PROGRESS_HPP
#define AV_ENGINE_PROGRESS_HPP

#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class ExecutionNode;

class EngineProgress {
public:
    EngineProgress(Platform &platform, ExecutionNode &node);
    EngineProgress(const EngineProgress &) = delete;
    EngineProgress &operator=(const EngineProgress &) = delete;

    /* DISPATCH: a DPC inside dxgkrnl's own timer asks this, so it allocates
     * nothing, blocks on nothing and takes no mutex. */
    [[nodiscard]] bool responsive();

private:
    Platform      &platform_;
    ExecutionNode &node_;
    uint64_t       answered_progress_;
    bool           answered_;
};

}  // namespace av

#endif  // AV_ENGINE_PROGRESS_HPP
