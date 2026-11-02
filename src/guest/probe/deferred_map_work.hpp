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
    static constexpr uint32_t contexts = 2u;
    static constexpr uint32_t constant_count = 4u;
    static constexpr uint32_t constant_sizes[constant_count] = {80u, 1040u, 2288u, 4112u};
    static constexpr uint32_t geometry_bytes = 1u << 20u;
    static constexpr uint32_t piece_bytes = 64u;
    static constexpr uint32_t pieces = 4u;
    static constexpr uint32_t head_bytes = piece_bytes * pieces;
    static constexpr uint32_t warm_frames = 30u;
    static constexpr uint32_t measured_frames = 300u;
    static constexpr uint64_t most_growth = uint64_t{16} << 20u;

    explicit DeferredMapWork(D3d11DeferredEntry &d3d11);
    ~DeferredMapWork();
    DeferredMapWork(const DeferredMapWork &) = delete;
    DeferredMapWork &operator=(const DeferredMapWork &) = delete;

    void run(ScenarioSteps &steps, void *window);

    bool invariants_hold() const;

private:
    int32_t  frame(uint32_t number);
    int32_t  frames();
    uint8_t  value(uint32_t number, uint32_t piece) const;
    uint32_t matching() const;

    D3d11DeferredEntry &d3d11_;
    bool                made_;
    uint32_t            drawn_;
    uint64_t            growth_;
    uint8_t             head_[head_bytes];

    friend struct DeferredMapWorkProbe;
};

}

#endif
