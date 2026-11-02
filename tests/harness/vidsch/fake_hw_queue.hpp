#ifndef AV_FAKE_HW_QUEUE_HPP
#define AV_FAKE_HW_QUEUE_HPP

#include "miniport/driver/ddi_table.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class FakeHwQueue {
public:
    static constexpr std::uint64_t fence_gpu_address = 0x0000000700000000ull;
    static constexpr std::uint32_t fence_handle = 0x40000042u;

    FakeHwQueue(MockPlatform &platform, const DdiTable &table, void *adapter, void *context);
    ~FakeHwQueue();
    FakeHwQueue(const FakeHwQueue &) = delete;
    FakeHwQueue &operator=(const FakeHwQueue &) = delete;

    std::int32_t create(std::uint32_t flags);
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

}

#endif
