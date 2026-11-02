#ifndef AV_LOOPBACK_RIG_HPP
#define AV_LOOPBACK_RIG_HPP

#include "miniport/adapter/capset_query.hpp"
#include "miniport/av_context/context_memory.hpp"
#include "miniport/av_context/guest_context.hpp"
#include "core/support/status.hpp"
#include "harness/loopback/loopback_device.hpp"
#include "harness/loopback/loopback_driver.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class LoopbackRig {
public:
    static constexpr std::uint8_t  ring_order = 12u;
    static constexpr std::uint32_t reply_bytes = 4096u;
    static constexpr std::uint32_t contexts = 2u;

    explicit LoopbackRig(const std::uint8_t *gpu = LoopbackDevice::default_gpu,
                         const char *record = nullptr);
    ~LoopbackRig();
    LoopbackRig(const LoopbackRig &) = delete;
    LoopbackRig &operator=(const LoopbackRig &) = delete;

    MockPlatform   &platform();
    LoopbackDevice &device();
    LoopbackDriver &driver();
    GuestContext   &context(std::uint32_t index);
    ContextMemory  &memory(std::uint32_t index);
    std::uint8_t   *reply(std::uint32_t index);
    std::uint32_t   line(std::uint32_t index) const;

    bool   read_capset();
    const CapsetInfo &capset() const;
    Status set_up(std::uint32_t index);
    bool   establish(std::uint32_t index);
    bool   retire(std::uint32_t index);
    Status submit(std::uint32_t index, std::uint64_t fence, std::uint64_t begin, std::uint64_t end);
    Status serve(std::uint32_t index);

private:
    MockPlatform   platform_;
    LoopbackDevice device_;
    LoopbackDriver driver_;
    CapsetQuery    query_;
    ContextMemory  first_memory_;
    ContextMemory  second_memory_;
    GuestContext   first_;
    GuestContext   second_;
};

}

#endif
