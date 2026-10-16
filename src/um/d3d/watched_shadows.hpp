/* The shadows of large buffers the CPU writes, allocated with the process's
 * write watch, so that an unmap sends the host the pages the application wrote
 * rather than the whole buffer. DWM appends a little to each large dynamic
 * buffer with NO_OVERWRITE and unmaps it many times a frame; copying all of
 * it into the stream each time was most of its render thread's memmove.
 * A smaller shadow comes from the heap and is sent whole. */
#ifndef AV_UM_WATCHED_SHADOWS_HPP
#define AV_UM_WATCHED_SHADOWS_HPP

#include "core/platform/block_source.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::icd {
class WrittenRuns;
}  // namespace av::icd

namespace av::d3d11 {

class WatchedShadows {
public:
    explicit WatchedShadows(BlockSource &heap);
    WatchedShadows(const WatchedShadows &) = delete;
    WatchedShadows &operator=(const WatchedShadows &) = delete;

    /* `watched` says which kind it gave: the release must be told. */
    uint8_t *allocate(size_t bytes, bool &watched);
    void     release(uint8_t *shadow, bool watched);
    /* The pages of [start, start + bytes) written since the watch last
     * began, noted in `runs` (empty first) from `start`, then the watch
     * begins again. False when they cannot be told - too many pages or runs -
     * and the range goes whole. */
    bool     take_written(uint8_t *start, size_t bytes, icd::WrittenRuns &runs);
    /* The watch begins again: what the driver itself wrote there, the host has. */
    void     forget_writes(uint8_t *start, size_t bytes);

private:
    /* A watched region is reserved in the allocation granularity (64 KiB), so
     * a smaller shadow would waste most of its reservation. */
    static constexpr size_t watch_threshold = 64u * 1024u;
    static constexpr uint32_t max_pages = 512u;

    BlockSource &heap_;
};

}  // namespace av::d3d11

#endif  // AV_UM_WATCHED_SHADOWS_HPP
