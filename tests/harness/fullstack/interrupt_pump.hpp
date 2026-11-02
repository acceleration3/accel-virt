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
    static constexpr std::uint32_t interrupt_lines = LoopbackDoorbells::pages + 1u;

    InterruptPump(FakeDxgkrnlFdo &fdo, FakeDxgkrnlAdapter &dxgkrnl, const FakeCommonConfig &common, MockHost &host,
                  VirtioBridge &bridge);

    std::uint32_t deliver();
    std::uint32_t delivered() const;

private:
    static constexpr std::uint16_t first_fence_queue = 2u;
    static constexpr std::uint16_t no_vector = 0xFFFFu;
    static constexpr std::uint32_t max_passes = 64u;

    std::uint32_t pass();
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

}

#endif
