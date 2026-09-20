/* The device core as a guest core reaches it in one process (docs/testing.md,
 * L4): a realized virtio-gpu-rvk over a MockHost whose guest memory is the
 * guest's MockPlatform, running commands through a MockExecutorOps - or through
 * an executor table it is given, the real library's among them (feature 044). Control
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

    /* The UUID of the host GPU the device is bound to: the default is the one
     * every rig shares, and another one is another device to a guest that
     * remembers what it started on (feature 040, FR-005). */
    static constexpr std::uint8_t default_gpu[16] = {0x10, 0x0B};

    /* `record` names the wire recording the device writes, or nothing. */
    explicit LoopbackDevice(MockPlatform &guest, const std::uint8_t *gpu = default_gpu,
                            const char *record = nullptr);
    /* Realized on `executor` instead, bound to the host GPU whose UUID is
     * `gpu`; executor() is then a mock nothing calls. */
    LoopbackDevice(MockPlatform &guest, const RvkExecutorOps &executor, const std::uint8_t *gpu);
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
    void realize(const RvkExecutorOps &executor, const std::uint8_t *gpu, const char *record);

    MockPlatform     &guest_;
    MockHost          host_;
    MockExecutorOps   executor_;
    VirtioGpuRvk      device_;
    LoopbackDoorbells doorbells_;
};

}  // namespace av

#endif  // AV_LOOPBACK_DEVICE_HPP
