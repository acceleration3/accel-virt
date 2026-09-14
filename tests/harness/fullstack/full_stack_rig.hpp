/* A Windows guest's whole path in one Linux process (docs/testing.md, L4): the
 * registered driver image, started by a fake dxgkrnl on virtio-gpu-rvk-pci's
 * PCI function, whose notify region, doorbell pages and interrupts are served
 * by the real device core over the guest's own MockPlatform memory, with
 * DdiKmt as the D3DKMT a guest process calls. The function is set up the way
 * tools/guest/run_guest_suite.py starts QEMU: rvk's subsystem, the doorbell
 * region, a hostmem region as large as the device core's, and eleven MSI-X
 * messages. Its device core runs MockExecutorOps, or - built with an executor
 * table and a host GPU - that executor, the real library's root in process
 * among them (feature 044, T028). The destructor ends the process, removes
 * the adapter while the device core can still answer its teardown, then lets
 * the device core go before the driver unloads and the platform's checks run. */
#ifndef AV_FULL_STACK_RIG_HPP
#define AV_FULL_STACK_RIG_HPP

#include "harness/device/fake_virtio_gpu_function.hpp"
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
    /* run_guest_suite.py's FENCE_VECTORS: three sources and eight messages the
     * fence queues share. */
    static constexpr std::uint32_t message_interrupts = 11u;
    static constexpr std::uint16_t rvk_subsystem = 0x5256u;
    /* virtio-gpu's VIRTIO_GPU_SHM_ID_HOST_VISIBLE, on a BAR of its own, sized
     * as the MockHost LoopbackDevice builds. */
    static constexpr std::uint8_t  hostmem_region = 1u;
    static constexpr std::uint8_t  hostmem_bar = 2u;
    static constexpr std::uint64_t hostmem_bytes = 64u << 20u;
    static constexpr std::uint64_t hostmem_address = 0x4000000000ull;

    FullStackRig();
    FullStackRig(const RvkExecutorOps &executor, const std::uint8_t *gpu);
    ~FullStackRig();
    FullStackRig(const FullStackRig &) = delete;
    FullStackRig &operator=(const FullStackRig &) = delete;

    /* AddDevice's, then StartDevice's NTSTATUS. */
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

}  // namespace av

#endif  // AV_FULL_STACK_RIG_HPP
