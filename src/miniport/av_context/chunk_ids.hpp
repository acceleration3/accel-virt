#ifndef AV_CHUNK_IDS_HPP
#define AV_CHUNK_IDS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Platform;
struct ChunkIdsProbe;

class ChunkIds {
public:
    ChunkIds(Platform &platform, uint32_t most);
    ChunkIds(const ChunkIds &) = delete;
    ChunkIds &operator=(const ChunkIds &) = delete;

    [[nodiscard]] Status take(uint32_t &id);
    void                 give_back(uint32_t id);
    uint32_t             taken() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t word_bits = 64u;

    void mark(uint32_t index);

    Platform               &platform_;
    GrowableArray<uint64_t> words_;
    uint32_t                most_;
    uint32_t                taken_;
    uint32_t                lowest_;

    friend struct ChunkIdsProbe;
};

}

#endif
