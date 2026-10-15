/* A Direct3D 11 application under a host GPU fault, as Windows tells it of a
 * removed device: the GPU's Direct3D 11 device on the host is lost
 * (HostLossRequest), and the probe's own device must say so within moments -
 * a call failing, or GetDeviceRemovedReason - and never leave a call
 * waiting; a device made afterwards, while the removed one is still held, as
 * DWM makes its new device before it lets the old one go, must draw.
 *
 * Steps, under the D3D11 probe's scenario: loss_device (a device, a render
 * target cleared and read back), loss_lose (the host asked; a host that
 * kept its device - started without the loss layer - is a note and the
 * steps stop), loss_removed (the milliseconds until the old device said it
 * was removed), loss_reason (GetDeviceRemovedReason's removal code), and
 * loss_recreated with loss_texel (the new device cleared and read back the
 * colour asked). */
#ifndef AV_HOST_LOSS_WORK_HPP
#define AV_HOST_LOSS_WORK_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class D3d11SharedEntry;
class HostLossRequest;
class Kmt;
class ScenarioSteps;

class HostLossWork {
public:
    /* How long the removed device may take to say so: its process is told
     * by its context's fence page at once, so anything near this is a
     * device nobody told. */
    static constexpr uint32_t removal_ms = 10000u;
    static constexpr uint32_t width = 16u;
    static constexpr uint32_t height = 16u;
    static constexpr uint8_t  before_colour[4] = {20u, 140u, 220u, 255u};
    static constexpr uint8_t  after_colour[4] = {230u, 60u, 20u, 255u};

    /* `before` is the device the host loses - one of the system's runtime on
     * this driver; a layer's is noted and not lost - `after` the one made again;
     * `kmt` keeps the time. */
    HostLossWork(D3d11SharedEntry &before, D3d11SharedEntry &after, HostLossRequest &request, Kmt &kmt);
    ~HostLossWork();
    HostLossWork(const HostLossWork &) = delete;
    HostLossWork &operator=(const HostLossWork &) = delete;

    /* Every step through `steps`, stopping at the first that failed. */
    void run(ScenarioSteps &steps);

    bool invariants_hold() const;

private:
    /* A device of `entry`'s, its target cleared to `colour` and read back:
     * the first refusal, invalid_state for a texel of another colour. */
    int32_t  draw(D3d11SharedEntry &entry, const uint8_t *colour);
    /* Polls the old device until a read is refused or it says it was
     * removed: ok with `waited_ms`, or invalid_state past removal_ms. */
    [[nodiscard]] Status await_removal(uint32_t &waited_ms);
    uint32_t elapsed_ms(uint64_t started) const;

    D3d11SharedEntry &before_;
    D3d11SharedEntry &after_;
    HostLossRequest  &request_;
    Kmt              &kmt_;
    bool              made_before_;
    bool              made_after_;
};

}  // namespace av

#endif  // AV_HOST_LOSS_WORK_HPP
