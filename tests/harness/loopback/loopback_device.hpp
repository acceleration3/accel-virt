#ifndef AV_LOOPBACK_DEVICE_HPP
#define AV_LOOPBACK_DEVICE_HPP

#include "device/module/virtio_gpu_av.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/loopback/loopback_doorbells.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockPlatform;

class LoopbackDevice {
public:
    static constexpr std::uint32_t patience_ms = 5000u;
    static constexpr std::uint64_t hostmem_bytes = std::uint64_t{256} << 20u;

    static constexpr std::uint8_t default_gpu[16] = {0x10, 0x0B};

    explicit LoopbackDevice(MockPlatform &guest, const std::uint8_t *gpu = default_gpu,
                            const char *record = nullptr);
    LoopbackDevice(MockPlatform &guest, const AvExecutorOps &executor, const std::uint8_t *gpu);
    ~LoopbackDevice();
    LoopbackDevice(const LoopbackDevice &) = delete;
    LoopbackDevice &operator=(const LoopbackDevice &) = delete;

    MockHost          &host();
    MockExecutorOps   &executor();
    VirtioGpuAv      &device();
    LoopbackDoorbells &doorbells();

    std::size_t exchange(const std::uint8_t *request, std::size_t length, std::uint8_t *answer, std::size_t capacity);
    bool        raised_past(std::uint32_t line, std::uint32_t seen);
    bool        holding(std::uint32_t count);

private:
    void realize(const AvExecutorOps &executor, const std::uint8_t *gpu, const char *record);

    MockPlatform     &guest_;
    MockHost          host_;
    MockExecutorOps   executor_;
    VirtioGpuAv      device_;
    LoopbackDoorbells doorbells_;
};

}

#endif
