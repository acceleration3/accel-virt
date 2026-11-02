#ifndef AV_RETIRED_SYNC_TOKENS_HPP
#define AV_RETIRED_SYNC_TOKENS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class RetiredSyncTokens {
public:
    explicit RetiredSyncTokens(BlockSource &blocks);
    RetiredSyncTokens(const RetiredSyncTokens &) = delete;
    RetiredSyncTokens &operator=(const RetiredSyncTokens &) = delete;

    [[nodiscard]] Status add(uint64_t token);
    bool                 contains(uint64_t token) const;
    void                 remove(uint64_t token);
    uint64_t             newest() const;
    void                 clear();
    uint32_t             runs() const;

    bool invariants_hold() const;

private:
    friend struct RetiredSyncTokensProbe;

    struct Run {
        uint64_t first_;
        uint64_t last_;
    };

    uint32_t run_of(uint64_t token) const;
    void     join_oldest();

    GrowableArray<Run> runs_;
};

}

#endif
