/* The device core as a guest core reaches it in one process (docs/testing.md,
 * L4): a realized virtio-gpu-rvk over a MockHost whose guest memory is the
 * guest's MockPlatform, running commands through a MockExecutorOps. Control
 * requests pass as bytes, doorbell stores ring the host, and interrupts are
 * waited for and then served by the test on its own thread, as a DPC would.
 * The thread that builds it is both the guest's thread and the device's main
 * loop. */
#ifndef AV_LOOPBACK_DEVICE_HPP
#define AV_LOOPBACK_DEVICE_HPP

#include "device/module/virtio_gpu_rvk.hpp"
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

    explicit LoopbackDevice(MockPlatform &guest);
    ~LoopbackDevice();
    LoopbackDevice(const LoopbackDevice &) = delete;
    LoopbackDevice &operator=(const LoopbackDevice &) = delete;

    MockHost          &host();
    MockExecutorOps   &executor();
    VirtioGpuRvk      &device();
    LoopbackDoorbells &doorbells();

    /* One control request through the device core: the answer's length. */
    std::size_t exchange(const std::uint8_t *request, std::size_t length, std::uint8_t *answer, std::size_t capacity);
    /* Whether fence line `line` has been raised more than `seen` times, within patience. */
    bool        raised_past(std::uint32_t line, std::uint32_t seen);
    /* Whether `count` commands are waiting behind the executor's hold, within patience. */
    bool        holding(std::uint32_t count);

private:
    MockPlatform     &guest_;
    MockHost          host_;
    MockExecutorOps   executor_;
    VirtioGpuRvk      device_;
    LoopbackDoorbells doorbells_;
};

}  // namespace av

#endif  // AV_LOOPBACK_DEVICE_HPP
