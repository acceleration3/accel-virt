#ifndef AV_FULL_STACK_RIG_HPP
#define AV_FULL_STACK_RIG_HPP

#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_fdo.hpp"
#include "harness/fullstack/ddi_kmt.hpp"
#include "harness/fullstack/interrupt_pump.hpp"
#include "harness/fullstack/registered_driver.hpp"
#include "harness/fullstack/virtio_bridge.hpp"
#include "harness/loopback/loopback_device.hpp"

#include <cstdint>

namespace av {

class FullStackRig {
public:
    static constexpr std::uint32_t message_interrupts = 11u;
    static constexpr std::uint16_t av_subsystem = 0x5256u;
    static constexpr std::uint16_t host_vendor = 0x1002u;
    static constexpr std::uint16_t host_device = 0x7550u;
    static constexpr std::uint8_t  host_revision = 0xC0u;
    static constexpr std::uint8_t  hostmem_region = 1u;
    static constexpr std::uint8_t  hostmem_bar = 2u;
    static constexpr std::uint64_t hostmem_bytes = LoopbackDevice::hostmem_bytes;
    static constexpr std::uint64_t hostmem_address = 0x4000000000ull;
    static constexpr std::uint64_t worker_clock_step_ns = 1000u;

    FullStackRig();
    FullStackRig(const AvExecutorOps &executor, const std::uint8_t *gpu);
    ~FullStackRig();
    FullStackRig(const FullStackRig &) = delete;
    FullStackRig &operator=(const FullStackRig &) = delete;

    std::int32_t           added() const;
    std::int32_t           started() const;
    RegisteredDriver      &driver();
    MockPlatform          &platform();
    FakeVirtioGpuFunction &function();
    FakeDxgkrnlAdapter    &dxgkrnl();
    FakeDxgkrnlFdo        &fdo();
    LoopbackDevice        &device();
    VirtioBridge          &bridge();
    InterruptPump         &pump();
    DdiKmt                &kmt();

private:
    void start();

    RegisteredDriver      driver_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    FakeDxgkrnlFdo        fdo_;
    LoopbackDevice        device_;
    VirtioBridge          bridge_;
    InterruptPump         pump_;
    DdiKmt                kmt_;
    std::int32_t          added_;
    std::int32_t          started_;
};

}

#endif
