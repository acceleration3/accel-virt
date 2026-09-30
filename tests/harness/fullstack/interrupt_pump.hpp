/* The interrupt line between the device core and the driver, delivered on the
 * test thread as dxgkrnl delivers it between the calls a process makes: a
 * queue the bridge completed, a fence queue a device worker raised, each
 * through FakeDxgkrnlFdo's message routine at device IRQL with the MSI-X
 * message the driver programmed for that queue (and its DPC at DISPATCH), and
 * a DPC a DDI queued from outside the ISR. Raises that arrive while one is
 * delivered are coalesced into the next, as a message the device raises twice
 * before the ISR runs is one interrupt. MockPlatform is single-threaded, so
 * nothing but this reaches the driver from a device worker's raise. */
#ifndef AV_INTERRUPT_PUMP_HPP
#define AV_INTERRUPT_PUMP_HPP

#include "harness/loopback/loopback_doorbells.hpp"

#include <cstdint>

namespace av {

class FakeCommonConfig;
class FakeDxgkrnlAdapter;
class FakeDxgkrnlFdo;
class MockHost;
class VirtioBridge;

class InterruptPump {
public:
    /* The fence queues' lines, and the display clock's after them. */
    static constexpr std::uint32_t interrupt_lines = LoopbackDoorbells::pages + 1u;

    InterruptPump(FakeDxgkrnlFdo &fdo, FakeDxgkrnlAdapter &dxgkrnl, const FakeCommonConfig &common, MockHost &host,
                  VirtioBridge &bridge);

    /* Delivers everything raised until a pass finds nothing new: how many
     * interrupts and DPCs it delivered. */
    std::uint32_t deliver();
    /* Interrupts delivered since the rig started, DPCs excluded. */
    std::uint32_t delivered() const;

private:
    static constexpr std::uint16_t first_fence_queue = 2u;
    static constexpr std::uint16_t no_vector = 0xFFFFu;
    static constexpr std::uint32_t max_passes = 64u;

    /* One pass over every source: how many it delivered. */
    std::uint32_t pass();
    /* The ISR and DPC for `queue`'s message; false when the driver gave the
     * queue no vector, which raises nothing. */
    bool          interrupt(std::uint16_t queue);

    FakeDxgkrnlFdo         &fdo_;
    FakeDxgkrnlAdapter     &dxgkrnl_;
    const FakeCommonConfig &common_;
    MockHost               &host_;
    VirtioBridge           &bridge_;
    std::uint32_t           seen_[interrupt_lines];
    std::uint32_t           dpcs_seen_;
    std::uint32_t           delivered_;
};

}  // namespace av

#endif  // AV_INTERRUPT_PUMP_HPP
