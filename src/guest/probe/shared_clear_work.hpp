#ifndef AV_SHARED_CLEAR_WORK_HPP
#define AV_SHARED_CLEAR_WORK_HPP

#include <stdint.h>

namespace av {

class D3d11SharedEntry;
class ScenarioSteps;
struct SharedClearWorkProbe;

class SharedClearWork {
public:
    static constexpr uint32_t targets = 3u;
    static constexpr uint32_t widths[targets] = {614u, 33u, 1u};
    static constexpr uint32_t heights[targets] = {289u, 7u, 1u};
    static constexpr uint8_t  colours[targets][4] = {{200u, 40u, 90u, 255u}, {30u, 170u, 220u, 255u},
                                                     {250u, 250u, 10u, 255u}};
    static constexpr uint32_t texel_bytes = 4u;

    explicit SharedClearWork(D3d11SharedEntry &d3d11);
    ~SharedClearWork();
    SharedClearWork(const SharedClearWork &) = delete;
    SharedClearWork &operator=(const SharedClearWork &) = delete;

    void run(ScenarioSteps &steps);

    bool invariants_hold() const;

private:
    int32_t  make_targets();
    int32_t  read_texels();
    uint32_t coloured() const;

    D3d11SharedEntry &d3d11_;
    bool              made_;
    uint32_t          made_targets_;
    uint32_t          read_;
    uint8_t           texels_[targets][texel_bytes];

    friend struct SharedClearWorkProbe;
};

}

#endif
