/* The device's display clock as the kernel's tests see it (docs/protocol.md,
 * "Display clock"): DISPLAY_CLOCK answered and the page it names headed, as the
 * device heads it; head 0's blanks written into the page whole, as the
 * device's clock thread writes them; and what the driver asked of them. The
 * caller runs the DPC the display line's interrupt would queue. */
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

    /* The DISPLAY_CLOCK pending on `control`, answered: OK and the page headed,
     * or ERR_INVALID_PARAMETER once refuse() was called. */
    void          answer(FakeVirtqueueDevice &control);
    void          refuse();
    /* The page the driver registered; 0 before. */
    std::uint64_t page() const;
    /* Head 0's next blank, or blank `sequence`, latching `latched`. Nothing
     * without a page. */
    void          blank(std::uint32_t latched = 0u);
    void          blank_at(std::uint64_t sequence, std::uint32_t latched);
    /* What the driver asked of head 0's blanks; 0 without a page. */
    std::uint32_t interrupt_every() const;

private:
    std::uint8_t *bytes() const;

    MockPlatform &platform_;
    std::uint64_t page_;
    std::uint64_t sequence_;
    bool          refusing_;
};

}  // namespace av

#endif  // AV_FAKE_DISPLAY_CLOCK_HPP
