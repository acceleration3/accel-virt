#include "harness/fullstack/interrupt_pump.hpp"

#include "harness/virtio/fake_common_config.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_fdo.hpp"
#include "harness/fullstack/virtio_bridge.hpp"
#include "harness/host/mock_host.hpp"

namespace av {

InterruptPump::InterruptPump(FakeDxgkrnlFdo &fdo, FakeDxgkrnlAdapter &dxgkrnl, const FakeCommonConfig &common,
                             MockHost &host, VirtioBridge &bridge)
    : fdo_(fdo), dxgkrnl_(dxgkrnl), common_(common), host_(host), bridge_(bridge), seen_{},
      dpcs_seen_(dxgkrnl.dpcs_queued()), delivered_(0u) {}

std::uint32_t InterruptPump::deliver()
{
    std::uint32_t total = 0u;
    for (std::uint32_t passes = 0u; passes < max_passes; passes++) {
        const std::uint32_t delivered = pass();
        if (delivered == 0u) break;
        total += delivered;
    }
    return total;
}

/* The fence counts are read before their interrupts run, so a raise that
 * lands while the DPC reads a fence page is seen by the next pass. */
std::uint32_t InterruptPump::pass()
{
    std::uint32_t delivered = 0u;
    if (dxgkrnl_.dpcs_queued() != dpcs_seen_) {
        dpcs_seen_ = dxgkrnl_.dpcs_queued();
        fdo_.dpc();
        delivered++;
    }
    for (std::uint16_t queue = VirtioBridge::control_queue; queue < VirtioBridge::served_queues; queue++)
        if (bridge_.take_completion(queue) && interrupt(queue)) delivered++;
    for (std::uint32_t line = 0u; line < interrupt_lines; line++) {
        const std::uint32_t raised = host_.raised(line);
        if (raised == seen_[line]) continue;
        seen_[line] = raised;
        if (interrupt(static_cast<std::uint16_t>(first_fence_queue + line))) delivered++;
    }
    return delivered;
}

bool InterruptPump::interrupt(std::uint16_t queue)
{
    const std::uint16_t vector = common_.queue(queue).vector;
    if (vector == no_vector) return false;
    static_cast<void>(fdo_.interrupt(vector));
    dpcs_seen_ = dxgkrnl_.dpcs_queued();
    delivered_++;
    return true;
}

std::uint32_t InterruptPump::delivered() const { return delivered_; }

}  // namespace av
