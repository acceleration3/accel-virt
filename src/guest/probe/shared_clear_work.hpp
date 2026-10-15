/* What Windows.Graphics.Capture has DWM do to capture one window: a render
 * target shared between processes (D3D11_RESOURCE_MISC_SHARED), the window's
 * size, cleared and read. The host makes a shared texture over linear memory
 * other APIs can open (ExportableImageInfo); NVIDIA's driver, left to place
 * that memory's rows, put them 32 bytes apart, and clearing one 614 texels
 * wide lost the host's Direct3D 11 device - every session on the GPU, DWM's
 * composition among them, and the desktop went black.
 *
 * Each target here is a size whose tight rows are no multiple of 128 bytes:
 * the capture that lost the device, a small odd one and a single texel. Each
 * is cleared to a colour of its own and its last texel read back, which
 * judges the clear and where its rows are; the device must still stand. */
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
    /* Each target's colour, red first. */
    static constexpr uint8_t  colours[targets][4] = {{200u, 40u, 90u, 255u}, {30u, 170u, 220u, 255u},
                                                     {250u, 250u, 10u, 255u}};
    static constexpr uint32_t texel_bytes = 4u;

    explicit SharedClearWork(D3d11SharedEntry &d3d11);
    ~SharedClearWork();
    SharedClearWork(const SharedClearWork &) = delete;
    SharedClearWork &operator=(const SharedClearWork &) = delete;

    /* Every step through `steps`, stopping at the first that failed. */
    void run(ScenarioSteps &steps);

    bool invariants_hold() const;

private:
    /* Every target made; the first refusal, or 0. */
    int32_t  make_targets();
    /* Every target's last texel read; the first refusal, or 0. */
    int32_t  read_texels();
    /* How many targets came back their colour, within the one step of
     * rounding a driver may choose. */
    uint32_t coloured() const;

    D3d11SharedEntry &d3d11_;
    bool              made_;
    uint32_t          made_targets_;
    uint32_t          read_;
    uint8_t           texels_[targets][texel_bytes];

    friend struct SharedClearWorkProbe;
};

}  // namespace av

#endif  // AV_SHARED_CLEAR_WORK_HPP
