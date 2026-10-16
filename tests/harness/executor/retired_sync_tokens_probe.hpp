/* Reaches into a RetiredSyncTokens so invariant tests can corrupt it. Declared
 * a friend by RetiredSyncTokens; nothing outside the tests uses it. */
#ifndef AV_RETIRED_SYNC_TOKENS_PROBE_HPP
#define AV_RETIRED_SYNC_TOKENS_PROBE_HPP

#include "executor/opengl/retired_sync_tokens.hpp"

#include <stdint.h>

namespace av {

struct RetiredSyncTokensProbe {
    explicit RetiredSyncTokensProbe(RetiredSyncTokens &target);

    void set_count(uint32_t count);
    void set_run(uint32_t index, uint64_t first, uint64_t last);

    RetiredSyncTokens &retired;
};

}  // namespace av

#endif  // AV_RETIRED_SYNC_TOKENS_PROBE_HPP
