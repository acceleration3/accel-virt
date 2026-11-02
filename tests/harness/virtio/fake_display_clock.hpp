#ifndef AV_FAKE_DISPLAY_CLOCK_HPP
#define AV_FAKE_DISPLAY_CLOCK_HPP

#include <cstdint>

namespace av {

class FakeVirtqueueDevice;
class MockPlatform;

class FakeDisplayClock {
public:
    explicit FakeDisplayClock(MockPlatform &platform);
    FakeDisplayClock(const FakeDisplayClock &) = delete;
    FakeDisplayClock &operator=(const FakeDisplayClock &) = delete;

    void          answer(FakeVirtqueueDevice &control);
    void          refuse();
    std::uint64_t page() const;
    void          blank(std::uint32_t latched = 0u);
    void          blank_at(std::uint64_t sequence, std::uint32_t latched);
    std::uint32_t interrupt_every() const;

private:
    std::uint8_t *bytes() const;

    MockPlatform &platform_;
    std::uint64_t page_;
    std::uint64_t sequence_;
    bool          refusing_;
};

}

#endif
