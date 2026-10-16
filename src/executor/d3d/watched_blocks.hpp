/* What a Direct3D session's tables grow in: another BlockSource, every
 * request passed on, and whether one was refused during the command running.
 * A guest's objects, devices, ranges and queries have no count past which
 * they are refused, so the only way a command fails for want of room is the
 * host running out of memory - and that is what the guest is told: the
 * device the command removes is removed for E_OUTOFMEMORY, not for an
 * internal error. Watching the source the tables grow in tells the two apart
 * without threading a status through every handler the wire dispatches to.
 *
 * One session's worker asks, so the flag needs no lock. A byte budget for a
 * hostile guest's host memory would be charged here too. */
#ifndef AV_WATCHED_BLOCKS_HPP
#define AV_WATCHED_BLOCKS_HPP

#include "core/platform/block_source.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct WatchedBlocksProbe;

class WatchedBlocks final : public BlockSource {
public:
    explicit WatchedBlocks(BlockSource &blocks);
    WatchedBlocks(const WatchedBlocks &) = delete;
    WatchedBlocks &operator=(const WatchedBlocks &) = delete;

    void *allocate(size_t bytes, Pool pool) override;
    void  release(void *block) override;
    void  require_irql_at_most(Irql ceiling, const char *function) override;

    /* A command starts: no request of it has been refused. */
    void     forget();
    /* Why the command that just failed failed: E_OUTOFMEMORY when a request
     * of it was refused, `otherwise` when none was. */
    uint32_t refusal(uint32_t otherwise) const;

private:
    /* A pointer, so a test can stand a failing source under a session it
     * made before anything was allocated. */
    BlockSource *blocks_;
    bool         refused_;

    friend struct WatchedBlocksProbe;
};

}  // namespace av

#endif  // AV_WATCHED_BLOCKS_HPP
