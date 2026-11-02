#ifndef AV_RETIRED_SYNC_TOKENS_PROBE_HPP
#define AV_RETIRED_SYNC_TOKENS_PROBE_HPP

#include "executor/opengl/retired_sync_tokens.hpp"

#include <stdint.h>

namespace av {

struct RetiredSyncTokensProbe {
    explicit RetiredSyncTokensProbe(RetiredSyncTokens &target);

    void set_run(uint32_t index, uint64_t first, uint64_t last);

    RetiredSyncTokens &retired;
};

}

#endif
