/* One hardware queue as dxgkrnl and VidSch drive it: DXGHWQUEUE::Initialize
 * @ 14035be70 creates the progress fence and calls DxgkDdiCreateHwQueue on the
 * driver's context; SubmitRenderToHwQueue @ 14001ae10 hands each packet over at
 * DISPATCH with its fence id and the fence's CPU address; after a
 * MONITORED_FENCE_SIGNALED, VidSchiUnwaitNodeHwQueueProgress @ 140008b00 reads
 * the fence and completes every packet at or below it; DestroyCoreState
 * @ 1403954f8 calls DxgkDdiDestroyHwQueue. A status dxgkrnl does not accept, a
 * fence that goes backwards or past what was submitted, fails the test. */
#ifndef AV_FAKE_HW_QUEUE_HPP
#define AV_FAKE_HW_QUEUE_HPP

#include "../../src/core/ddi_table.hpp"
#include "mock_platform.hpp"

#include <cstdint>

namespace av {

class FakeHwQueue {
public:
    static constexpr std::uint64_t fence_gpu_address = 0x0000000700000000ull;
    static constexpr std::uint32_t fence_handle = 0x40000042u;

    FakeHwQueue(MockPlatform &platform, const DdiTable &table, void *adapter, void *context);
    /* A queue still created is destroyed, as DXGCONTEXT::DestroyContext does first. */
    ~FakeHwQueue();
    FakeHwQueue(const FakeHwQueue &) = delete;
    FakeHwQueue &operator=(const FakeHwQueue &) = delete;

    std::int32_t create(std::uint32_t flags);
    /* The next fence id, with DXGK_SUBMITCOMMANDFLAGS `flags`. */
    std::int32_t submit(std::uint64_t dma_address, std::uint32_t flags);
    void         observe_progress();
    std::int32_t destroy();

    void         *handle() const;
    std::uint64_t submitted() const;
    std::uint64_t completed() const;

private:
    enum : std::int32_t {
        status_not_implemented = static_cast<std::int32_t>(0xC0000002u),
        status_invalid_parameter = static_cast<std::int32_t>(0xC000000Du),
        status_no_memory = static_cast<std::int32_t>(0xC0000017u),
    };
    static constexpr std::uint32_t dma_bytes = 0x1000u;

    MockPlatform   &platform_;
    const DdiTable &table_;
    void           *adapter_;
    void           *context_;
    void           *handle_;
    std::uint64_t   progress_fence_;
    std::uint64_t   submitted_;
    std::uint64_t   completed_;
};

}  // namespace av

#endif  // AV_FAKE_HW_QUEUE_HPP
