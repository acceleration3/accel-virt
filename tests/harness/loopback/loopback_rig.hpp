/* One process holding both sides: a guest's MockPlatform, the device core over
 * it, the driver carrying requests between them, and two guest contexts. The
 * device is reset before the contexts' memory is released, as a guest's
 * memory outlives the device's view of it. */
#ifndef AV_LOOPBACK_RIG_HPP
#define AV_LOOPBACK_RIG_HPP

#include "core/rvk/capset_query.hpp"
#include "core/rvk/guest_context.hpp"
#include "core/support/status.hpp"
#include "harness/loopback/loopback_device.hpp"
#include "harness/loopback/loopback_driver.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class LoopbackRig {
public:
    static constexpr std::uint8_t  ring_order = 12u;
    static constexpr std::uint32_t contexts = 2u;

    /* `gpu` names the host GPU the device is bound to; rigs share one unless a
     * test asks for another (feature 040, FR-005). */
    explicit LoopbackRig(const std::uint8_t *gpu = LoopbackDevice::default_gpu);
    ~LoopbackRig();
    LoopbackRig(const LoopbackRig &) = delete;
    LoopbackRig &operator=(const LoopbackRig &) = delete;

    MockPlatform   &platform();
    LoopbackDevice &device();
    LoopbackDriver &driver();
    GuestContext   &context(std::uint32_t index);
    /* The doorbell, and fence line, context `index` sets up on: index + 1. */
    std::uint32_t   line(std::uint32_t index) const;

    /* Reads the capset: whether it completed. */
    bool   read_capset();
    /* What the device offered, once read. */
    const CapsetInfo &capset() const;
    /* Sets context `index` up with ids of its own: the set_up status, then
     * whether every request was answered and it is established. */
    Status set_up(std::uint32_t index);
    bool   establish(std::uint32_t index);
    /* Tears context `index` down and releases it: whether it went idle. */
    bool   retire(std::uint32_t index);
    Status submit(std::uint32_t index, std::uint64_t fence, std::uint64_t begin, std::uint64_t end);
    Status serve(std::uint32_t index);

private:
    MockPlatform   platform_;
    LoopbackDevice device_;
    LoopbackDriver driver_;
    CapsetQuery    query_;
    GuestContext   first_;
    GuestContext   second_;
};

}  // namespace av

#endif  // AV_LOOPBACK_RIG_HPP
