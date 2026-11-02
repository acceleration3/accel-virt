#ifndef AV_SYNC_TOKENS_HPP
#define AV_SYNC_TOKENS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "executor/opengl/retired_sync_tokens.hpp"

#include <stdint.h>

namespace av {

class SyncTokens {
public:
    static constexpr uint32_t retire_after = 256u;

    explicit SyncTokens(BlockSource &blocks);
    SyncTokens(const SyncTokens &) = delete;
    SyncTokens &operator=(const SyncTokens &) = delete;

    [[nodiscard]] Status bind(uint64_t token, void *fence);
    bool                 crowded() const;
    void                *oldest() const;
    [[nodiscard]] Status retire_oldest(void *&deleted);
    void                *fence_for(uint64_t token) const;
    void                *forget(uint64_t token);
    [[nodiscard]] bool   take_any(void *&fence);

    uint32_t count() const;

    bool invariants_hold() const;

private:
    friend struct SyncTokensProbe;

    struct Live {
        uint64_t token_;
        void    *fence_;
    };

    uint32_t index_of(uint64_t token) const;

    GrowableArray<Live> live_;
    uint64_t            newest_;
    void               *stand_in_;
    RetiredSyncTokens   retired_;
};

}

#endif
