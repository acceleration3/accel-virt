/* Dark Souls Remastered's frame, as its calls into the driver show it
 * (AV_D3D11_TRACE): the frame recorded on one of two deferred contexts in
 * turn, its dynamic constant buffers each mapped with WRITE_DISCARD there, a
 * dynamic buffer of one MiB that is both vertex and index buffer mapped with
 * WRITE_DISCARD and then three times with WRITE_NO_OVERWRITE, a piece
 * further each time, the command list finished, executed on the immediate
 * context and let go of, and a present.
 *
 * What it judges is the memory and the bytes: the process's committed memory
 * over hundreds of frames, which a driver that keeps what each deferred map
 * took grows by the buffer every frame - the guest ran out of memory under
 * the game in minutes - and the geometry buffer's head after the last frame,
 * which must be that frame's four pieces. */
#ifndef AV_DEFERRED_MAP_WORK_HPP
#define AV_DEFERRED_MAP_WORK_HPP

#include <stdint.h>

namespace av {

class D3d11DeferredEntry;
class ScenarioSteps;
struct DeferredMapWorkProbe;

class DeferredMapWork {
public:
    static constexpr uint32_t extent = 64u;
    /* The game records alternate frames on two deferred contexts. */
    static constexpr uint32_t contexts = 2u;
    /* Its dynamic constant buffers' sizes. */
    static constexpr uint32_t constant_count = 4u;
    static constexpr uint32_t constant_sizes[constant_count] = {80u, 1040u, 2288u, 4112u};
    static constexpr uint32_t geometry_bytes = 1u << 20u;
    /* A frame writes the geometry buffer in pieces: one discarding map, then
     * maps that do not overwrite, each a piece further. */
    static constexpr uint32_t piece_bytes = 64u;
    static constexpr uint32_t pieces = 4u;
    static constexpr uint32_t head_bytes = piece_bytes * pieces;
    /* Frames before the memory is first measured - what the driver and the
     * runtime set up once is not a leak - and frames measured after. */
    static constexpr uint32_t warm_frames = 30u;
    static constexpr uint32_t measured_frames = 300u;
    /* What the process may commit over the measured frames: a driver that
     * keeps a frame's map of the geometry buffer commits 300 MiB. */
    static constexpr uint64_t most_growth = uint64_t{16} << 20u;

    explicit DeferredMapWork(D3d11DeferredEntry &d3d11);
    ~DeferredMapWork();
    DeferredMapWork(const DeferredMapWork &) = delete;
    DeferredMapWork &operator=(const DeferredMapWork &) = delete;

    /* Every step through `steps`, stopping at the first that failed. */
    void run(ScenarioSteps &steps, void *window);

    bool invariants_hold() const;

private:
    /* One frame's calls on its context; the first refusal, or 0. */
    int32_t  frame(uint32_t number);
    /* Every frame, warm and measured; the first refusal, or 0. */
    int32_t  frames();
    /* What piece `piece` of frame `number` is written with: never zero, and
     * never the same in two frames running. */
    uint8_t  value(uint32_t number, uint32_t piece) const;
    /* How many of the head's bytes are what the last frame wrote. */
    uint32_t matching() const;

    D3d11DeferredEntry &d3d11_;
    bool                made_;
    uint32_t            drawn_;
    uint64_t            growth_;
    uint8_t             head_[head_bytes];

    friend struct DeferredMapWorkProbe;
};

}  // namespace av

#endif  // AV_DEFERRED_MAP_WORK_HPP
