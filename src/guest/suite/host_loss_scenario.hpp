/* A host device lost under the guest, as a host GPU fault loses one, and
 * what each context sees (`avtest loss`; docs/protocol.md, "Losing the host
 * device"):
 *
 * - drawn: a Direct3D context makes a device, a render target, clears it and
 *   finishes - the GPU's Direct3D 11 device, which every Direct3D context of
 *   the guest on that GPU shares, is at work;
 * - idle_made: a second Direct3D context makes a device and then sends
 *   nothing more, as DWM on a desktop that does not change;
 * - other_before: a Vulkan context, whose host device is its own, answers -
 *   on a kernel device of its own, as another process's is: dxgkrnl puts a
 *   kernel device whose submission the driver refused in error, and every
 *   context on it with it, so the faulted contexts' device is no other's;
 * - lose: the first context asks the host to lose its device
 *   (LOSE_HOST_DEVICE). A host started without the layer that does it keeps
 *   it, and the scenario notes that and stops;
 * - drawing_removed: the first context's next call fails, at once - its
 *   milliseconds are the measurement;
 * - idle_removed: the idle context is told without sending anything - its
 *   fence page says faulted within a few seconds - and idle_code says why,
 *   7, the host device lost; idle_refused: its next call fails without
 *   reaching the host;
 * - other_kept: the Vulkan context still answers;
 * - recreated: a new Direct3D context - on a kernel device of its own, as a
 *   session the user-mode driver opens after a fault is - draws on the
 *   device the host made again.
 *
 * The idle context is what the desktop was: every Direct3D context of the
 * guest on that GPU, DWM's among them, is faulted the same way by a run of
 * this, and the desktop is drawn again only by a DWM that makes its device
 * again. */
#ifndef AV_HOST_LOSS_SCENARIO_HPP
#define AV_HOST_LOSS_SCENARIO_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "guest/transport/guest_connection.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;
class KmtTransport;

class HostLossScenario {
public:
    /* How long an idle context may go unfaulted after the loss: its worker is
     * woken at once, so anything near this is a loss nobody told it of. */
    static constexpr uint32_t idle_seconds = 5u;
    /* The fault code a lost host device is (docs/protocol.md, fault codes). */
    static constexpr uint32_t device_lost_code = 7u;

    HostLossScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);
    HostLossScenario(const HostLossScenario &) = delete;
    HostLossScenario &operator=(const HostLossScenario &) = delete;

    void run();

private:
    /* A device of id `device` on `transport`'s context, a render target
     * cleared on it, and a finish that says the device was not removed. */
    [[nodiscard]] Status draw(KmtTransport &transport, uint32_t device);
    /* A device made on `transport`'s context, and a finish. */
    [[nodiscard]] Status make_device(KmtTransport &transport, uint32_t device);
    /* The milliseconds since `started`, a performance counter reading. */
    uint32_t             elapsed_ms(uint64_t started) const;
    /* Whether `transport`'s fence page says faulted within idle_seconds. */
    [[nodiscard]] Status await_fault(KmtTransport &transport, uint32_t &waited_ms);

    Kmt              &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps     steps_;
    ScenarioTransport link_;
    /* Each further context's commands, one at a time per context. */
    uint8_t           idle_staging_[ScenarioTransport::command_bytes];
    uint8_t           other_staging_[ScenarioTransport::command_bytes];
    uint8_t           next_staging_[ScenarioTransport::command_bytes];
    /* The kernel devices the Vulkan context and the new Direct3D one are on. */
    GuestConnection   other_connection_;
    GuestConnection   next_connection_;
};

}  // namespace av

#endif  // AV_HOST_LOSS_SCENARIO_HPP
