#ifndef AV_OVERLAY_WORK_HPP
#define AV_OVERLAY_WORK_HPP

#include <stdint.h>

namespace av {

class OverlayEntry;
class ScenarioSteps;

class OverlayWork {
public:
    static constexpr uint32_t support_direct = 0x1u;
    static constexpr uint32_t support_scaling = 0x2u;
    static constexpr uint32_t format_bgra = 87u;
    static constexpr uint32_t format_nv12 = 103u;
    static constexpr uint32_t mode_overlay = 1u;
    static constexpr uint32_t presents = 150u;
    static constexpr int32_t  window_x = 160;
    static constexpr int32_t  window_y = 120;
    static constexpr uint32_t window_width = 480u;
    static constexpr uint32_t window_height = 320u;
    static constexpr uint8_t  colour[4] = {200u, 40u, 120u, 255u};
    static constexpr int32_t  disjoint = static_cast<int32_t>(0x887A000Bu);
    static constexpr uint32_t disjoint_retries = 30u;

    OverlayWork(OverlayEntry &dxgi, uint32_t hold = 0u);
    ~OverlayWork();
    OverlayWork(const OverlayWork &) = delete;
    OverlayWork &operator=(const OverlayWork &) = delete;

    void run(ScenarioSteps &steps);
    uint32_t mode() const;

    bool invariants_hold() const;

private:
    void promote(ScenarioSteps &steps, const char *step, int32_t x, int32_t y, uint32_t width, uint32_t height,
                 uint32_t hold);

    OverlayEntry &dxgi_;
    uint32_t      hold_;
    bool          made_;
    uint32_t      mode_;
};

}

#endif
