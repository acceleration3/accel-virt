/* The overlay proof, step by step: the output says it has overlays and can put
 * a B8G8R8A8 swap chain in one; a swap chain on a window covering part of the
 * screen, presented for long enough for DWM to promote it, is shown in an
 * overlay plane rather than composed; and one covering the whole output is
 * flipped to directly. What it proves is the whole overlay path of this
 * driver: avgpu's caps and its check of each configuration, its MPO3 flips of
 * several planes, and avumd's multiplane presents - DWM promotes a swap chain
 * only when every one of those agreed. */
#ifndef AV_OVERLAY_WORK_HPP
#define AV_OVERLAY_WORK_HPP

#include <stdint.h>

namespace av {

class OverlayEntry;
class ScenarioSteps;

class OverlayWork {
public:
    /* DXGI_OVERLAY_SUPPORT_FLAG_DIRECT and _SCALING - DXGI answers one or the
     * other, scaling where the planes stretch (CDXGIOutput::CheckOverlaySupport)
     * - and DXGI_FRAME_PRESENTATION_MODE_OVERLAY. */
    static constexpr uint32_t support_direct = 0x1u;
    static constexpr uint32_t support_scaling = 0x2u;
    static constexpr uint32_t mode_overlay = 1u;
    /* Presents before the mode is asked: DWM promotes a swap chain after it has
     * presented steadily for a while, a couple of seconds at sixty a second. */
    static constexpr uint32_t presents = 150u;
    /* The partial window, well inside any mode this driver offers. */
    static constexpr int32_t  window_x = 160;
    static constexpr int32_t  window_y = 120;
    static constexpr uint32_t window_width = 480u;
    static constexpr uint32_t window_height = 320u;
    static constexpr uint8_t  colour[4] = {200u, 40u, 120u, 255u};
    /* DXGI_ERROR_FRAME_STATISTICS_DISJOINT: the statistics restarted - a mode
     * change, or a presentation mode changing - and are asked again after
     * another present, as many times as this. */
    static constexpr int32_t  disjoint = static_cast<int32_t>(0x887A000Bu);
    static constexpr uint32_t disjoint_retries = 30u;

    explicit OverlayWork(OverlayEntry &dxgi);
    ~OverlayWork();
    OverlayWork(const OverlayWork &) = delete;
    OverlayWork &operator=(const OverlayWork &) = delete;

    /* Every step through `steps`, stopping at the first a later one needs. */
    void run(ScenarioSteps &steps);
    /* The last composition mode read, 0 (composed) before any. */
    uint32_t mode() const;

    bool invariants_hold() const;

private:
    /* A swap chain at the rectangle, presented `presents` times, and the mode
     * it was then shown in judged as an overlay under `step`. */
    void promote(ScenarioSteps &steps, const char *step, int32_t x, int32_t y, uint32_t width, uint32_t height);

    OverlayEntry &dxgi_;
    bool          made_;
    uint32_t      mode_;
};

}  // namespace av

#endif  // AV_OVERLAY_WORK_HPP
