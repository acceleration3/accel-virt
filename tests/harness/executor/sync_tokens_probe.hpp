#ifndef AV_SYNC_TOKENS_PROBE_HPP
#define AV_SYNC_TOKENS_PROBE_HPP

#include "executor/opengl/sync_tokens.hpp"

#include <stdint.h>

namespace av {

struct SyncTokensProbe {
    explicit SyncTokensProbe(SyncTokens &target);

    void set_token(uint32_t index, uint64_t token);
    void set_fence(uint32_t index, void *fence);
    void set_newest(uint64_t newest);
    void set_stand_in(void *fence);
    RetiredSyncTokens &retired();

    SyncTokens &tokens;
};

}

#endif
