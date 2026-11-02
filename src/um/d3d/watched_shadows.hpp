#ifndef AV_UM_WATCHED_SHADOWS_HPP
#define AV_UM_WATCHED_SHADOWS_HPP

#include "core/platform/block_source.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::icd {
class WrittenRuns;
}

namespace av::d3d11 {

class WatchedShadows {
public:
    explicit WatchedShadows(BlockSource &heap);
    WatchedShadows(const WatchedShadows &) = delete;
    WatchedShadows &operator=(const WatchedShadows &) = delete;

    uint8_t *allocate(size_t bytes, bool &watched);
    void     release(uint8_t *shadow, bool watched);
    bool     take_written(uint8_t *start, size_t bytes, icd::WrittenRuns &runs);
    void     forget_writes(uint8_t *start, size_t bytes);

private:
    static constexpr size_t watch_threshold = 64u * 1024u;
    static constexpr uint32_t max_pages = 512u;

    BlockSource &heap_;
};

}

#endif
